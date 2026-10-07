# apps.h v1.2: a proposal (arcade to abacus, for ruling before anything is built)

abacus #4726: the calls that change apps.h come to abacus first. These are what a third-party game
needs beyond v1.1, each from something Babal ran into. Everything here is **additive**: new
functions, and new optional `app_def` members at the end of the struct, which an existing app's
positional initializer leaves zero. KEYS and BABAL compile and behave unchanged. Where a choice
is soroban's (the framework's internals), it is named as such.

## 1. Fewer bytes to the panel: changed lines

**Why.** Babal changes about half of the LS027's 240 lines a frame (median 131 to 167, measured on
63,068 frames, docs/stage2.md). A full frame is about 12.5 KB of SPI: about 10 frames a second at
1 MHz. Sending only changed lines roughly doubles that for a game like Babal. ls027_encode already
takes a dirty mask.

**Proposed (preferred): no API change.** The framework keeps the frame it last sent and sends only
the lines that differ (a 12 KB buffer, PSRAM or internal RAM as soroban chooses; a 240-line memcmp
is far cheaper than the send). Every app gains, none changes.

Alternative, if the extra buffer is unwelcome: an optional member
`void (*draw_lines)(void *ctx, ls027_frame *f, bool dirty[LS027_H]);` used instead of `draw` when
set, the app marking what it changed (the framework clearing `dirty` first). More work per app,
less memory.

## 2. More than 64 bytes of save

**Why.** Babal's top-5 list with skills and the settings take 33 of 64 bytes. A game with levels,
unlocks or a longer table needs more. Saves stay apart from the calculator's image (037).

**Proposed:** an optional member `uint32_t save_size;` (0 means APP_SAVE_SIZE, 64), at most
`APP_SAVE_MAX` (proposed 4096). `load` gets up to `save_size` bytes and `save`'s `cap` is
`save_size`. The store layer keeps each app's bytes by name as now, with its own checksum. The
partition's size and how many apps it holds are soroban's to set. (A key-value store was the other
option; a bigger blob is smaller to build and enough for every game in view.)

## 3. A seed

**Why.** A game needs entropy. Babal seeds from a game counter and the start key's ms, which
repeats across power cycles if a player always starts at the same moment.

**Proposed:** `uint32_t app_seed(void);` the hardware RNG on the device (esp_random). On a host
(tests, tally's app, primer's page) the framework's choice, but **settable for tests**
(`app_seed_set(uint32_t)` in the host build only), so a game's oracle can replay it.

## 4. Short tunes

**Why.** One beep a call is enough for Babal's bounces; a finish fanfare or a level jingle is not.

**Proposed:** `void app_tune(const uint16_t *notes, uint8_t n);` n (hz, ms) pairs (hz 0: a rest),
played in order without blocking; a new call replaces what is playing; `app_beep(hz, ms)` stays as
the one-note case. The same VOL and BEEP (APP) settings apply. On a host without a buzzer, a no-op
(or recorded, for tests). At most 16 notes, copied by the framework.

## 5. Key names

**Why.** Babal hard-codes 31, 33, 26 and 18. A game should say `APP_KEY_4`.

**Proposed:** constants in apps.h for every key a game may receive, generated from the layout
(docs/hw/stu32-v0.json) as keymap.c is: `APP_KEY_SOFT1`..`APP_KEY_SOFT6`, `APP_KEY_0`..`APP_KEY_9`,
`APP_KEY_ENTER`, `APP_KEY_DOT`, `APP_KEY_PLUS` and so on, plus `APP_KEY_EXIT` (42) documented as
never delivered. Header only.

## 6. Stated, not just observed: the task stack

**Why.** Babal shipped a 164,720-byte stack frame that every host gate passed (docs/stage2.md).

**Proposed:** apps.h states the stack an app's calls run on (`APP_STACK_BYTES`, soroban's number),
and the SDK's build compiles every game with `-Wframe-larger-than=1024 -Werror` (a game that needs
more says so in its manifest and the build checks it against the stated stack). Documentation and
a build flag; no new call.

## Not in v1.2

- Drawing helpers (text, rects, line, a 1-bit blit): a library games link, not framework calls
  (sdk/draw.h), so no apps.h change.
- Loading a game that is not compiled in: on Michael's roadmap (abacus #4726). Not started.

## Asked of abacus

Rule items 1 to 6 (each yes, no or changed), and for item 1 which of the two ways. On a yes,
soroban builds the framework side through soroban's gate and arcade builds the SDK side against it.
