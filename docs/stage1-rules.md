# Stage 1: Babal's rules in C, judged against babalcore

The brief is abacus's stage 1 (decision 55, plan docs/ideas/2026-10-07-babal.md in the abacus
repo): port rust/babalcore to C one to one, and show that the same seeded game gives the same
rows, slabs, events and score in both, with a planted divergence red first.

## The reference

- gitlab.com/liberecofr/babal at `fa985cd49111270edade2c7bebdab7ce204be6a8` (main, 2026-09-14).
  LICENSE and hp48/LICENSE are the Unlicense (read from the repository on 2026-10-07).
- `oracle/fetch-babal.sh` clones it into `oracle/babal/` (gitignored) and refuses any other commit.
  Nothing in it is edited. babalcore's own suite passes there (`cargo test --locked -p babalcore`).
- Crates the reference pulls (host only, never read, never shipped; MIT/Apache-2.0): rand 0.10.2,
  rand_core 0.10.1, chacha20 0.10.2, euclid 0.22.14, num-traits 0.2.19, the same versions as
  babal's own rust/Cargo.lock (oracle/ref/Cargo.lock is committed, built `--locked`).
- Never ported: the music (Alexander Nakarada, Serpent Sound Studios) and godot/fonts. The Godot
  engine, godot-rust and babalgame are not used. hp48/ is look and feel only (stage 2).

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
| `HashMap<isize, f64>` scores log | array indexed by row, with a presence flag, grown by doubling | rows are >= 0; see "C-only limits" |
| `HashMap<i64, KeyState>` steer keys | up to `BB_STEER_KEYS_MAX` (8) ids a side, in insertion order | see quirk 3 and "C-only limits" |
| `VecDeque<Row>` | ring of `sliding_max` rows, storage given by the caller | only `sliding_max = 10000` is babalcore's behaviour; see below |
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

- Scores log: a row too large to double the array for, or memory that runs out, adds to
  `log_failures` (babalcore's HashMap would have stored it). Before the review this path skipped
  silently, and an overflowing size freed the log and used it after the free.
- Steer keys: a ninth id on one side adds to `dropped_keys` (babalcore keeps every id; keys are
  never removed in either).
- FakeInput: a steer pushed past 256 queued adds to `dropped`.
- `Duration::from_secs_f64` panics in Rust on inf, NaN or more than 2^64 ns; the C saturates, and
  a stickiness past 2^63 ns (292 years) is held at INT64_MAX ns instead of turning negative.

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
  the player as babalgame's `generate_missing` does. Each tick prints the events, every public
  reading of the player, and every private field (read back from Rust's Debug text, which prints
  an f64 shortest-round-trip); the scores log is printed at the end. Scripts: seeded play-like
  runs, three 30,000-tick games, and edges (idle, jump spam, a run to `finish`, zero, long and
  negative deltas, NaN, stickiness).
- `points <seed>`: PointsCounter alone, seeded walks of distance over every time-to-complete
  band (30 to 300 s, 0, negative), with its private state after each step.
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

## Not in stage 1

The renderer, the device, the APPS framework (unit 037, soroban's; interface in docs/apps-h-v1.md),
the ring window, the frame-time budget. Not checked on the device yet: printf's `PRId64` under
newlib-nano (Display only), sine_wave through newlib's sin (visuals only).
