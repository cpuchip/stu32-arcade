# Writing a game for the STU-32 (the games SDK, first draft)

A game is a C program the calculator runs under APPS, with the screen and the keys to itself until
it exits. This is what a game author needs, from what Babal (the first) ran into. It is written by
arcade under abacus's ruling (#4726); the framework is soroban's (abacus-firmware firmware/apps.h).
Parts marked **v1.2** are ruled (docs/apps-h-v1.2.md) and not yet built.

## Where a game lives

In its own repository, as a folder: the game's C sources and headers and a manifest. The firmware
reads a games list (repository, pin, licence, folder) and exports each game at its pin with
`git archive`, as it does Babal (ARCADE_PIN). Every entry passes soroban's gate; a game by someone
outside our seats needs Michael's word before it is listed (decision 59).

The manifest (format to be set with soroban): the name APPS shows (at most 5 cells), the folder
(GAMES or TOOLS), author, licence, and where the source came from.

## The calls (apps.h v1.1)

A game is one `app_def`:

| call | when | what the game does |
|---|---|---|
| `load(ctx, save, n)` | first, before `start`; n is 0 when nothing was saved | take its saved bytes, whole or not at all |
| `start(ctx)` | after `load` | set up its first screen |
| `tick(ctx, ms)` | right after `start`, then as asked | advance by `ms` (time since the last tick); return the ms to the next tick, never 0 while it wants to run |
| `key(ctx, key, down, ms)` | a key goes down or up | `key` is row * 6 + column; `ms` on the same clock as `tick`; return true to be drawn now |
| `draw(ctx, frame)` | after every tick, and after a key that returned true | draw the whole 400 x 240 frame (pixel x of row y in bit x % 8 of byte x / 8, set is dark) |
| `save(ctx, save, cap)` | on leaving, and before the idle limit stops it | write what to keep, at most `cap` bytes (64; **v1.2**: up to 4096 by `save_size`) |
| `stop(ctx)` | last | nothing to free: there is no allocation |

What the framework promises: `ctx` is `ctx_size` bytes, at most APP_CTX_MAX (256 KB), zeroed for each
run, in PSRAM; the calculator's own state is never touched while a game runs and comes back as it
was; C (key 42) always ends the game and is never delivered to it; the save is kept apart from the
calculator's (a MEMORY CLEAR leaves it); after twice the auto-off time without a key the game is
saved and stopped; `app_beep(hz, ms)` plays a tone at once, honouring VOL and BEEP; `app_now_ms()`
is the game's clock. **v1.2**: `app_seed()` (a random seed, settable on a host for tests),
`app_tune(notes, n)` (a short tune, one at a time), `APP_KEY_*` names, `APP_STACK_BYTES`, and the
framework sending only the lines that changed.

What a game must not do: allocate, call the calculator's core (`ab_*`), keep pointers into the frame
after `draw`, or put anything big on the stack: its calls run on the framework's task stack, a few
KB. Build with `-Wframe-larger-than=1024 -Werror` (Babal once shipped a 164 KB stack frame that every
host test passed). Put big state in `ctx`.

## Drawing: sdk/draw.h

Shades (`DRAW_DARK`, `DRAW_HALF`, ... a byte for each row parity), `draw_fill`, `draw_frame_rect`,
`draw_line`, `draw_blit` (1-bit sprites: OR, SET, XOR, CLEAR), `draw_text` in the firmware's 5 x 7
face at any scale, `draw_text_width`. Everything clips to the frame. A line costs at most 400 steps
however far its ends lie. Tested against a pixel-by-pixel reference (`make sdkcheck`).

## Testing a game

What Babal uses, to be lifted into the SDK: a host runner that plays a script of keys and ticks
through the calls in the framework's order and writes each frame (tools/apphost.c), a reference
for the picture where one exists, and an S3 bench that replays the same scripts on the chip and
compares frame hashes with the host's (device/appbench, scripts/device_app.sh). Determinism makes
all of this possible: draw only from `ctx`, and take time only from `tick` and `key`.

## Still to come

The games-list format and its generator (with soroban), the v1.2 calls (soroban, through the gate),
a generic host runner and bench template, and a template game.
