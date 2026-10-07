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
| `HashMap<isize, f64>` scores log | array indexed by row, with a presence flag | rows are >= 0 |
| `HashMap<i64, KeyState>` steer keys | small array of keys | see "order" below |
| `VecDeque<Row>` | ring of `sliding_max` rows, storage given by the caller | the S3 cannot spare 320 KB for 10,000 rows; `sliding_max = 10000` reproduces the Rust |
| `Vec<PlayerEvent>` from `tick` | a fixed array, at most `BB_EVENTS_MAX` | |
| `impl LevelQuery` / `impl InputQuery` | the level by pointer; input as two function pointers and a context | |

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
3. `InputSteer::flush_keys` sums keys in HashMap order, which Rust randomizes per run. With one
   key per side (the calculator's case) the sum is exact; with two keys on one side even Rust is
   not reproducible. The oracle scripts use one key id per side.
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
(the C port). The gate compares their output byte for byte; floats are printed as their bits.

- `pins`: ChaCha20 f64 streams, `lerp`, `Duration::from_secs_f64`, `as_secs_f64`, `round`,
  `fract`, the f32 exports, on fixed and seeded inputs.
- `level <seed> <skill> <n>`: every row as text, `item` and `find_start_spot` on a grid that
  includes negative, past-the-end and post-slide rows.
- `game <script>`: a script of synthetic time steps, key presses and releases, jumps and mouse
  moves, driving `Player::tick` through `InputSteer` and `InputJump`, with rows kept 100 ahead of
  the player as babalgame's `generate_missing` does. Each tick prints the events and every public
  reading of the player: `position`, `speed` (f32 bits), `level_col`, `level_row`, `dist`,
  `score`, `best_row`, `best_dist`, `is_boosting`, `backing_camera_blend`, `speed_factor`,
  `time_progress_done`, `time_progress_total`, and the level's `first`, `last`, `len`.
  Scripts come from `oracle/gen_scripts.py` (seeded play-like scripts) plus hand-written ones
  (idle, jump spam, a full run to `finish`, zero and long deltas, a run past the slide).

Gate (abacus's): N seeds x M scripts identical; every plant red; a fresh clone builds and passes.
Plants are applied by `oracle/run.sh` to a copy of the C source at run time (sed, then a check
that the sed changed the file and the mutant builds), never kept as files:
a constant (JUMP_GRAVITY), a skipped RNG draw, the ChaCha stream word, an event dropped, two
handlers swapped, a quirk "fixed". Each must turn the comparison red.

babalcore's own tests (unit and doc) are ported to `tests/babal_test.c` and run by `make test`.

## Not in stage 1

The renderer, the device, the APPS framework (unit 037, soroban's), the frame-time budget.
