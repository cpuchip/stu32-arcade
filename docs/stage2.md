# Stage 2: Babal on the STU-32 (in progress)

The brief is abacus's stage 2 (decision 55): the rules of stage 1 running under APPS (unit 037,
docs/apps-h-v1.md), drawn in 1 bit on the 400 x 240 LS027, steered and jumped from the keys, a
beep on bounces, with frames tied to the model and the frame time measured on the S3.

## Memory: the windows (done)

An app gets 32 KB of state (APP_CTX_MAX) and may not allocate. babalcore keeps up to 10,000 rows
(320 KB) and a scores log that grows with every row, and stage 1's review measured that simply
keeping fewer rows changes real games (quirks 1 and 2: a 256-row level diverges at row 121). So
the level and the log stay babalcore's logically, and only their newest rows are held:

- `bb_level`: babalcore's length and row0, up to 10,000 rows; row r held at `rows[r % window]`.
- the scores log: row r at `log[r % log_window]`; rows are logged in rising order (the key is the
  best distance, rounded), so a slot only ever gives way to a newer row.
- A read older than its window returns a void row (or no score) and is counted
  (`out_of_window`, `log_out_of_window`); the oracle fails any run where a count moves.

Measured over every game and points job (oracle/run.sh, `BB_WINDOW`, `BB_LOG_WINDOW`): the deepest
level read is 298 rows behind the newest row, the deepest log lookup 103 rows. That matches the
rules' bound: rows are kept 100-200 ahead of the ball, a fall rewinds at most 75 rows (5 s at the
15 rows/s maximum) plus a walk to the nearest floor, which a full row without one ends (at expert,
0.5^12 per row).

Chosen: **512 level rows (16 KB at 32 bytes a row) and 256 log rows (4 KB)**. The gate runs every
game and points job in these windows and requires the same output (less the final log and rows
dumps, which show storage) with both counts at 0; a 150-row level window and a 64-row log window
must fail (they do), and so must a log slot read without checking its row.

## The rules on the S3 (done)

`scripts/device.sh` (on soroban's lent board, claimed in both lanes): every oracle game (51, 188,265
ticks) replayed on the chip in the device windows, with the windows in PSRAM as the app's ctx is,
through the same `oracle/game.c` as the host, its whole output hashed (FNV-1a 64) and compared with
the host's window-mode output, which the gate shows equals babalcore's. Measured 2026-10-07:
**DEVICE PASS**, all 51 identical (doubles in software, newlib); the control (edge-idle with one
tick 1 ns longer, expected to hash as edge-idle) differs, as it must. The deepest reads on the chip
match the host's (298 rows, 103 log rows).

Rules time per tick (row generation and `Player::tick`, esp_timer, 240 MHz): **180 us on average,
16,782 us at worst**. The worst is expected to be the generate-ahead burst (100 rows at once,
64 ChaCha draws a row); a row depends only on the seed, its index and the row before, so the app
can spread generation over ticks without changing a row. To be measured before it is relied on.

## The picture (first cut)

`app/render.c`: the flat track in perspective, as the HP 48 drew it (hp48/screenshot-hp48.jpg):
floor dark, boost slabs a 50% checker, overdrive 75%, void light, row seams dotted near the
camera; the ball a dithered disc at the centre column, lifted by its jump; the score, the best
and a time bar in the firmware's 5 x 7 face (FIRMWARE_PIN, exported read-only). The columns wrap
as the rules do (an unrolled cylinder) and one turn is drawn, so the track has edges and steering
past one brings the other side round. Integer fixed point from the model's doubles, so a frame is
the same bits on host and chip: `make frames` and `build/frames SCRIPT [TICK...]` print a hash per
tick (the frame oracle for the S3) and write chosen frames as PBM.

## Next

- The app (start, key, tick, draw, save) on apps.h v1; delta clamped to 0..100 ms (quirks 7, 8).
- Controls (proposed, with Michael through basecamp, d15): 4 and 6 steer, 8 and ENTER jump, C exits.
- Frames on the S3: the frame hashes against the host's, and the draw time.
- The generation burst: measure, then spread it over ticks.
- The registry patch to soroban (app_registry[], Makefile, both CMakeLists); sources stay here.
