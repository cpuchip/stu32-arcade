# Writing a game for the STU-32 (the games SDK, first draft)

A game is a C program the calculator runs under APPS, with the screen and the keys to itself until
it exits. This is what a game author needs, from what Babal (the first) ran into. It is written by
arcade under abacus's ruling (#4726); the framework is soroban's (abacus-firmware firmware/apps.h).
It is written against apps.h v1.2 (abacus-firmware 0bc7be9, soroban; the rulings in
docs/apps-h-v1.2.md).

## Where a game lives

In its own repository, as a folder: the game's C sources and headers and a manifest. The firmware
reads a games list (`games.list`: id, repository, pin, licence, manifest) and exports each game at
its pin with `git archive` (the firmware's scripts/gen_games.py, through soroban's gate; sdk/gamelist
keeps a copy, and changes go to the firmware as patches, patches/firmware/). Every entry passes
soroban's gate; a game by someone outside our seats needs Michael's word before it is listed
(decision 59).

The manifest (`stu32-game.txt`; its format is in gen_games.py's header): the name APPS shows (1 to
5 letters, digits or spaces), the folder (GAMES or TOOLS), the licence (one SPDX id, the same as
the list's), the sources, the include folders (on the game's own files only), the flags it builds
with, and for a port, original. Nothing exported may be a symlink.

## The calls (apps.h v1.2)

A game is one `app_def`:

| call | when | what the game does |
|---|---|---|
| `load(ctx, save, n)` | first, before `start`; n is 0 when nothing was saved | take its saved bytes, whole or not at all |
| `start(ctx)` | after `load` | set up its first screen |
| `tick(ctx, ms)` | right after `start`, then as asked | advance by `ms` (time since the last tick); return the ms to the next tick, never 0 while it wants to run |
| `key(ctx, key, down, ms)` | a key goes down or up | `key` is an `APP_KEY_*` (row * 6 + column; app_keys.h); `ms` on the same clock as `tick`; return true to be drawn now |
| `draw(ctx, frame)` | after every tick, and after a key that returned true | draw the whole 400 x 240 frame (pixel x of row y in bit x % 8 of byte x / 8, set is dark) |
| `save(ctx, save, cap)` | on leaving, and before the idle limit stops it | write what to keep, at most `cap` bytes: the app_def's `save_size`, 0 meaning 64, at most APP_SAVE_MAX (4096) |
| `stop(ctx)` | last | nothing to free: there is no allocation |

What the framework promises: `ctx` is `ctx_size` bytes, at most APP_CTX_MAX (256 KB), zeroed for each
run, in PSRAM; the calculator's own state is never touched while a game runs and comes back as it
was; C (key 42) always ends the game and is never delivered to it; the save is kept apart from the
calculator's (a MEMORY CLEAR leaves it); after twice the auto-off time without a key the game is
saved and stopped; `app_beep(hz, ms)` plays a tone at once, honouring VOL and BEEP; `app_now_ms()`
is the game's clock; `app_seed()` is a seed from the device's hardware RNG (on a host, fixed by
`app_seed_set` for tests; it returns the same value each call then, so mix in a game counter if each
game should differ); `app_tune(notes, n)` plays n (hz, ms) pairs from one array, hz 0 a rest, at
most APP_TUNE_MAX (16), a new tune replacing the last and C stopping it; and the framework sends the
panel only the lines that changed, so a game just draws the whole frame.

What a game must not do: allocate, call the calculator's core (`ab_*`), keep pointers into the frame
after `draw`, or put anything big on the stack: its calls may use APP_STACK_BYTES (16 KB) of the framework's
task stack. Build with `-Wframe-larger-than=1024 -Werror` (Babal once shipped a 164 KB stack frame that every
host test passed). Put big state in `ctx`.
On a host that spelling is the portable one (clang and primer's emcc take it, abacus #5012); the
device's gcc build may use `-Werror=frame-larger-than=1024`, but never mix the two forms
(`-Wframe-larger-than=1024 -Werror=frame-larger-than` is refused outright by gcc 15 and the S3's
xtensa gcc).

## Drawing: draw.h (the framework's)

Shades (`DRAW_DARK`, `DRAW_HALF`, ... a byte for each row parity), `draw_fill`, `draw_frame_rect`,
`draw_line`, `draw_blit` (1-bit sprites: OR, SET, XOR, CLEAR), `draw_text` in the firmware's 5 x 7
face at any scale, `draw_text_width`. Everything clips to the frame; any int coordinates are safe.
It is abacus-firmware's firmware/draw.{h,c} since apps.h v1.2: one copy for every game. It began
here as sdk/draw; soroban's gate made it 64-bit where the S3's 32-bit `long` lost far lines, and
made lines exact for any ends. The framework's tests/draw_test.c checks it against a pixel-by-pixel
reference on the host and the chip; `make sdkcheck` runs that test here on the copy at FIRMWARE_PIN.

## Starting from the template: sdk/template

`sdk/template/ball.c` is BALL, a paddle (4 and 6) keeping a ball in play: one `app_def`, all state in
a 48-byte ctx, integer positions, a long tick clamped, held keys from key-down and key-up by
`APP_KEY_*`, the serve's side from `app_seed()`, a tone a bounce and a tune at the miss, the best saved whole or not at all, drawing only with draw.h. Copy the folder with its
manifest (`stu32-game.txt`), rename, and build on it.

## Testing a game: sdk/runner/apprun.c

Build the runner with the game (`cc -DAPP_DEF=my_app sdk/runner/apprun.c my_game.c firmware/draw.c
firmware/font5x7.c ...`) and give it a script: `key K D MS` and `tick MS` lines. It plays the game through the
calls in the framework's order and checks the framework's bounds (ctx size, name, a key never before
the last tick, no tick after the game asked for none); `-hashes FILE` writes a frame hash a tick,
`-pbm DIR -every N` writes frames, `-load`/`-save` take and keep a save of `save_size` bytes, `-seed N`
fixes `app_seed()`, `-v` prints the tones and tunes. It
agrees frame for frame with Babal's own harness (tools/apphost.c) on 8,002 frames. Build a game's own
sources with `-Wframe-larger-than=1024 -Werror` (alone: the runner is a host tool), and run the
runner under ASan and UBSan. `make sdkcheck` does all of this for the template, and checks the
runner's v1.2 calls with a test app (tests/v12_app.c).

For a game whose rules have a reference (Babal's are babalcore), compare against it as Babal does
(oracle/app_check.sh); for the chip, the S3 bench replays the same scripts and compares frame hashes
with the host's (device/appbench, scripts/device_app.sh; a generic template is still to come).
Determinism makes all of this possible: draw only from `ctx`, and take time only from `tick` and `key`.

## A WebAssembly game: sdk/wasm

The same game source builds as a .wasm (decision 67: WAMR, one .wasm a game, an .aot a target from our
pinned wamrc). `sdk/wasm/guest.c` wraps the game's `app_def` as a reactor module: the ctx and the frame
live in its linear memory, and the four framework calls it uses become imports from `env`. On the
calculator, `sdk/wasm/bridge.c` runs the .aot through WAMR as an ordinary `app_def` (`WASM_GAME(...)`),
holding WAMR only between the game's load and stop and setting the calling task's native stack boundary.
`make wasmcheck` (with WASI_SDK, WAMRC and WAMR_TREE set) builds Babal both ways and requires identical
frames, save and counts over every app script, with planted bridge faults red. How it enters the
firmware: docs/wasm-integration.md.

## Still to come

A generic S3 bench template.
