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

## The app (first cut)

`app/babal_app.c` on apps.h v1 (FIRMWARE_PIN): a title (4 and 6 or a soft key choose the skill,
the best shown, ENTER or 8 plays), the game, a finish screen (FINISH or NEW BEST). Keys go to
babalcore's own InputSteer (4 left as id 1, 6 right as id 2) and InputJump (8 and ENTER on the
press) at their own ms; a tick's step is clamped to 100 ms (quirks 7 and 8); rows are made a
few a tick (100 to 200 ahead, as babalgame keeps them) instead of 100 at once; a tone a tick
(finish, fall, overdrive, boost, high score, a thousand, land). The save: magic, version, the
skill, the best per skill, the games played (28 of 64 bytes). ctx: 23,944 bytes on the host.

The oracle (`make appcheck`, oracle/app_check.sh): tools/apphost.c plays seeded app scripts
(oracle/gen_appscripts.py: a title, a start, uneven ticks some of them clamped, keys at their own
times) through apps.h's calls in the framework's order, and writes the same game as an oracle/ref
script from its own copy of the key spec. Every tick's readings and private state equal
babalcore's (less the level's row counts, which differ by design): 8 sessions, up to 4,001 ticks,
one played to its finish and on into a second game; the save round-trips. Six plants red: 4
steering right, the step clamped at 50 ms, key times in microseconds, a jump on the release, the
tick's clock a millisecond late, no rows made ahead (red by a hang, caught by the timeout).

## The app on the S3 (measured 2026-10-07, soroban's lent board, window 06:43-07:01Z)

`scripts/device_app.sh`, device/appbench: the 8 app scripts played through apps.h's calls on the
chip, the ctx in PSRAM (23,880 bytes on the S3), the frame in internal RAM. **DEVICE PASS**: every
frame's hash (16,639 frames) equals tools/apphost.c's on the host; the control (one key 1 ms
later) differs.

- tick (the rules, rows made a few at a time): **151 us average, 558 us worst**.
- row generation alone: 100 rows at once 16,385 us (the first bench's 16.8 ms worst tick, now
  measured), one at a time 164 us. So spreading it was the fix, and it holds.
- draw (render.c, the whole frame): **41,016 us average, 45,251 us worst**. Too slow: with the
  LS027's SPI send (12.5 KB, about 50 ms at 2 MHz) it caps the game near 10 frames a second. The
  renderer works pixel by pixel in 64-bit arithmetic on a 32-bit core; spans and 32-bit maths next,
  judged by frames byte-identical to these.

## The span renderer on the S3 (window 07:20-07:21Z, on soroban's go #4599)

The same app bench with render.c drawing in 32-bit spans (0fcff5e): **DEVICE PASS**, all 16,639
frames identical to the host's; the control differs. **Draw 1,499 us average, 2,343 us worst**
(was 41,016 / 45,251: about 27 times faster); tick 154 us average, 535 us worst. So a whole frame
of Babal costs under 3 ms on the S3, and the frame rate is the LS027's SPI send (about 12.5 KB a
frame, roughly 50 ms at 2 MHz, soroban's arithmetic) and the app's 50 ms tick, not the drawing.

## Fresh eyes on stage 2 (2026-10-07)

A reviewer that did not write it, under the same clean-room rules, read and probed stage 2. The
span renderer matched its pixel-by-pixel definition on 200,000 probe frames, the windows held,
and the rendering stayed in the frame at every extreme it tried. It found:

- the app never gave the saved best to the rules, so the high-score event and its tone could
  never happen (babalgame does: update_player_best_score). Fixed;
- left shifts of negative numbers in render.c (undefined in C, arithmetic in GCC). Now
  multiplications by 2^6, the same values: 43,571 frames identical to the committed renderer;
- a corrupt save half-taken. Now a save is taken whole or not at all, and a best below 0 is none;
- the title's help line off-centre. Centred by its own length;
- the oracle compared only each session's first game with babalcore (6,589 of 16,639 frames),
  checked none of the app's own rules, and compared the picture only with itself: seven plants
  survived (soft keys reversed, one seed for every game, a tie as a new best, a 60 s finish hold,
  the fall tone silenced, the track mirrored, the ball never lifting).

tools/apphost.c now judges the app three ways, each against something the app does not share:
every game against babalcore (with the saved best); the app's own rules against a spec written in
the harness (soft key k starts skill k, 4 and 6 step the skill, no two seeds alike, the 600 ms
finish hold, NEW BEST only above the best, the tone table, the windows at each game's end, the
save's bytes, corrupt saves refused, and a tie made on purpose by replaying game 1 from a save
holding its score); and every frame against tools/render_ref.c, the committed per-pixel renderer
(c522381) with the HUD and title composed from the spec. The harness also runs under ASan and
UBSan with no recovery. `make appcheck`: 16,639 ticks over 15 games identical to babalcore,
every frame equal to the reference, 15 plants red (the review's seven and its two defects among
them).

## Next

- Controls (proposed, with Michael through basecamp, d15): 4 and 6 steer, 8 and ENTER jump, C exits.
- The registry patch is with soroban (patches/firmware/0001-*.patch, #4593), abacus reviewing.
- Changed-line updates (ls027_encode's dirty rows): measure whether they buy frames on a scrolling track.
