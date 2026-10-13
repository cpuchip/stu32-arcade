# Stage 1: Babal's rules in C, judged against babalcore

The brief is abacus's stage 1 (decision 55, plan docs/ideas/2026-10-07-babal.md in the abacus
repo): port rust/babalcore to C one to one, and show that the same seeded game gives the same
rows, slabs, events and score in both, with a planted divergence red first.

## The reference

- gitlab.com/liberecofr/babal at `fa985cd49111270edade2c7bebdab7ce204be6a8` (main, 2026-09-14).
  LICENSE and the 1995 original's own LICENSE (in its folder upstream) are the Unlicense (read
  from the repository on 2026-10-07).
- `oracle/fetch-babal.sh` clones it into `oracle/babal/` (gitignored) and refuses any other commit.
  Nothing in it is edited. babalcore's own suite passes there (`cargo test --locked -p babalcore`).
- Crates the reference pulls (host only, never read, never shipped; MIT/Apache-2.0): rand 0.10.2,
  rand_core 0.10.1, chacha20 0.10.2, euclid 0.22.14, num-traits 0.2.19, the same versions as
  babal's own rust/Cargo.lock (oracle/ref/Cargo.lock is committed, built `--locked`).
- Never ported: the music (Alexander Nakarada, Serpent Sound Studios) and godot/fonts. The Godot
  engine, godot-rust and babalgame are not used. The 1995 original (its folder upstream, assembly
  for an older calculator) is look and feel only (stage 2), and that folder also ships third
  parties' tools (ASM.lib, PCT.lib, Secrets.lib, StrWrt.lib, credited to others in its README) that
  its Unlicense cannot cover: never opened, used or copied. This repo names no calculator maker or
  model (the charter's rule; stu32-firmware's too).

## The port

`babal/`: one C file per babalcore module, the same names with a `bb_` prefix, f64 where Rust has
f64 and f32 where it has f32, the same order of operations. Built with `-ffp-contract=off` and
never `-ffast-math`, so that no multiply-add is fused. What is not a one-to-one translation:

| Rust | C | why |
|---|---|---|
| `rand::rngs::ChaCha20Rng` + `random::<f64>()` | `chacha20.c`: RFC 8439 block, 64-bit counter in words 12-13, stream 0; a u64 is the low word then the high; f64 = (u64 >> 11) * 2^-53 | written from the published algorithm; rand's layout pinned black-box (`ref pins`), not read |
| `euclid::Vector3D<f64>`, `lerp` | `bb_vec3`, `a*(1-t) + b*t` | pinned black-box |
| `Instant`, `Duration`, `Option<Instant>` | int64 nanoseconds, a `has_` flag; no clock inside the rules, the caller passes `now` | the device's clock is the app's tick; `from_secs_f64` / `as_secs_f64` pinned black-box |
| `HashMap<SlabDef, f64>` densities | sorted array (the Rust sorts it before use) | same order |
| `HashMap<isize, f64>` scores log | a window of the newest rows in caller storage, row r at slot r % window | no allocation; docs/stage2.md |
| `HashMap<i64, KeyState>` steer keys | up to `BB_STEER_KEYS_MAX` (8) ids a side, in insertion order | see quirk 3 and "C-only limits" |
| `VecDeque<Row>` | babalcore's length and row0, the newest `window` rows held in caller storage | the window must exceed the deepest read; docs/stage2.md |
| `Vec<PlayerEvent>` from `tick` | a fixed array, at most `BB_EVENTS_MAX` (16; a tick makes at most 10) | |
| `VecDeque<f64>` in FakeInput | a queue of `BB_FAKE_STEERS_MAX` (256) | tests only |
| `impl LevelQuery` / `impl InputQuery` | the level by pointer; input as two function pointers and a context | |

**The ring size is not free.** Because of quirks 1 and 2, a ring smaller than 10,000 rows starts
sliding during real play and changes the game from there on. Measured by the fresh-eyes review: the
same noob script with a 256-row ring diverges at row 121 (tick 1613) and stalls at best row 171,
where babalcore reaches 665. Stage 2 must keep the logical 10,000-row level and hold only a
window of recent rows physically, with a count of reads outside the window that the oracle
checks stays 0.

### C-only limits (must never be reached; each is counted, and the C driver fails if it moves)

- Scores log: since stage 2 a window of the newest rows (no allocation at all); a lookup older
  than the window adds to `log_out_of_window`. (Before the review the log grew by realloc, could
  skip silently, and on an overflowing size freed the log and used it after the free; the second
  review's growth plant went with that code.)
- Level: a read older than the window adds to `out_of_window` (docs/stage2.md).
- Steer keys: a ninth id on one side adds to `dropped_keys` (babalcore keeps every id; keys are
  never removed in either).
- FakeInput: a steer pushed past 256 queued adds to `dropped`. No driver or game uses FakeInput
  (babalcore's tests do not either), so nothing checks this count; the queue costs 2 KB.
- `Duration::from_secs_f64`: past 2^64 ns Rust still returns (it panics only past 2^64 seconds,
  or on inf or NaN); the C saturates everything past 2^64 ns at UINT64_MAX (pinned), and a
  stickiness past 2^63 ns (292 years) is held at INT64_MAX ns. `pop_jump` compares delays far
  below either, so jumps behave the same.
- On a 32-bit S3, rows (`isize`) saturate at 2^31 where 64-bit Rust counts on; a game reaches
  hundreds. On a 64-bit host a key of about 1e8-1e9 rows would make the log allocate gigabytes
  where Rust stores one entry: the same answer, slowly; no game produces one.

## Kept as they are (quirks, named so nobody "fixes" one silently)

1. `Level::generate` seeds row j with `rows.len()`, not the absolute row index. Past the slide
   (10,000 rows kept) every new row is generated from seed index 10000 and the row before it.
2. `Level::slab_kind(col, row)` reads deque index `row + row0`, which is right only for a row
   given relative to `-row0`; every caller passes an absolute row (`item` adds `row0` first,
   `find_start_spot` works between `first` and `last`, babalgame's tunnel adds the player's row).
   So once the level has slid (row0 > 0) every read lands `row0` rows further on, and from
   `last - row0` on it reads void: a run past 10,000 rows sees the tunnel shifted, then empty.
   Measured: seed 123 medium, 10,350 rows: `slab_kind` gives void from row 10000 while the deque
   holds generated rows (oracle `level`, the `r` lines against the `deque` section).
3. `InputSteer::flush_keys` sums a side's keys in HashMap order, which Rust randomizes per run.
   One or two keys a side give the same sum in any order (two terms commute); three can differ,
   so even babalcore is not reproducible there. The oracle uses at most two ids a side; the
   calculator uses one.
4. `Skill::def_density0` sums two densities in HashMap order; two terms commute, so it is exact.
5. `update_fall`'s first phase lerps from `begin` to `begin` (the ball holds still).
6. `update_fall`'s last phase lerps from `top_fall` to `end`, not from `bottom_fall`.
7. `handle_steer` slows the sideways speed by `delta * slow_down` and clamps only at zero, so a
   negative delta with the ball moving sideways grows `x` past 2^61, where `x + 500 == x`, and
   the wrap loop never ends. Measured on babalcore: `tickd -2e12` with a key held hangs (timeout),
   the same tick with no sideways speed returns, and so does the script without that tick.
8. `handle_forward`'s square term flips sign for a huge delta (`sz - delta * 0.5 * sz` with `sz`
   already hugely negative), so the speed clamps to the maximum and the ball jumps ahead:
   `tickd 1e13` moved it 136,301,045,122,305 rows, and keeping rows ahead of it (babalgame's
   `generate_missing`) then never finishes. Measured on babalcore by bisecting the script.
   Both are kept in the rules; the app's tick passes a small positive delta (stage 2).

## The oracle

Two drivers print the same text: `oracle/ref` (Rust, babalcore by path) and `oracle/driver.c`
(the C port). The gate compares their output byte for byte; floats are printed as their bits, any
NaN as `nan` (its sign is not meaningful, and Rust's Debug text drops it).

- `pins`: ChaCha20 f64 streams, `lerp`, `Duration::from_secs_f64` (ties, exact x.5 ns, up to
  1.8e10 s), `as_secs_f64`, `round`, `fract`, `floor`, the casts; maths.rs on boundaries,
  specials and a sweep; a TimeTracker sequence through exactly `TIME_TRACKER_MAX_SEC` and its
  Display; PlayerEvent's Display.
- `level <seed> <skill> <n>`: every row as `slab_kind` reads it, `item` and `find_start_spot` on a
  grid that includes negative, past-the-end and post-slide rows, and the rows the deque holds.
- `game <script>`: synthetic time steps, key presses and releases (two ids a side), jumps and mouse
  moves, driving `Player::tick` through `InputSteer` and `InputJump`, with rows kept 100 ahead of
  the player as babalgame's `generate_missing` does (the C side runs with glibc's
  `MALLOC_PERTURB_`, so memory it forgets to initialise shows). Each tick prints the events, every public
  reading of the player, and every private field (read back from Rust's Debug text, which prints
  an f64 shortest-round-trip); the scores log is printed at the end. Scripts: seeded play-like
  runs, three 30,000-tick games, and edges (idle, jump spam, a run to `finish`, zero, long and
  negative deltas, NaN, stickiness).
- `points <seed>`: PointsCounter alone, seeded walks of distance, the time to complete chosen by
  seed so every entry is used (30, 49.9, 50, 50.1, 120, 199.9, 200, 200.1, 300, 0, -1 s), with
  its private state after each step.
- `inputs <seed>`: InputJump and InputSteer alone, with time standing still or running backwards.

Reach, measured: the longest game holds 500 rows and reaches best row 378; the `points` walks log up
to about 1,900 rows (rows past 15,000); every one of the 12 event kinds occurs. No game runs past the
10,000-row slide (it would take hours of play); the `level` jobs cover the slide.

Gate (abacus's): every job identical; every plant red; a fresh clone builds and passes. Plants are
applied by `oracle/run.sh` to a copy of the C source at run time (sed, then a check that the sed
changed the file and the mutant builds), never kept as files. One mutant is known equivalent and
is not a plant: `backing_camera_blend`'s `now_sec <= begin_sec` made `<` gives 0.0 at equality
either way.

babalcore's own tests (unit and doc) are ported to `tests/babal_test.c` and run by `make test`;
a plant there must turn them red too.

## Fresh eyes (2026-10-07)

A reviewer that did not write the port, under the same clean-room rules, read both trees and
probed them. Its findings and what changed:

- The scores log's growth could skip silently and, on an overflowing size, free the log and use
  it after the free (ASan: heap-buffer-overflow); now checked and counted.
- Stickiness past 2^63 ns turned negative and past 2^64 ns wrapped; now saturates.
- The steer-key and FakeInput caps were silent; now larger and counted.
- The oracle could not judge NaN, never observed the inputs or maths.rs directly, never grew the
  scores log past 256 rows, and claimed a game past the slide that did not exist. Seven plants
  survived the first gate; six now go red (the seventh is the equivalent mutant above).
- This doc implied a smaller ring was harmless; the measurement above says otherwise.

A second pass over the fixes (ASan and UBSan over all jobs; about 4 million `from_secs_f64` bit
patterns against Rust, none different) found no defect in the C. It found three plants the gate
missed (an uninitialised growth entry: now caught under `MALLOC_PERTURB_`; the step edges at 50
and 200 s: the `points` seeds now use every band edge), the panic threshold misstated here (2^64
seconds, not ns), the FakeInput count claimed as checked, and a NaN edge that tested nothing new.

## Not in stage 1

The renderer, the device, the APPS framework (unit 037, soroban's; interface in docs/apps-h-v1.md),
the ring window, the frame-time budget. Not checked on the device yet: printf's `PRId64` under
newlib-nano (Display only), sine_wave through newlib's sin (visuals only).
