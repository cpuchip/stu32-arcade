# APPS interface v1 (soroban, unit 037), as frozen 2026-10-07

Relayed by soroban in chillacks #4499 after arcade's requests (#4497: a time on key events, a beep,
key() returning whether to redraw), accepted by abacus (#4501); app_now_ms added in #4506. The header itself is
abacus-firmware's firmware/apps.h; this copy is for planning stage 2 and is not authoritative.

```c
#define APP_SAVE_SIZE 64
#define APP_CTX_MAX (32 * 1024)
typedef struct {
    const char *name;           /* APPS' label: at most 5 cells */
    uint32_t ctx_size;          /* bytes of state the framework allocates, zeroed, per run (<= APP_CTX_MAX) */
    void (*load)(void *ctx, const uint8_t *save, uint32_t n);   /* before start: the saved bytes, n 0 none */
    void (*start)(void *ctx);
    bool (*key)(void *ctx, int key, bool down, uint32_t ms);   /* row*6+col, down on press, at ms on the
                                                   app's clock; return true to be drawn now */
    uint32_t (*tick)(void *ctx, uint32_t ms);   /* ms since the last tick; returns ms to the next (0: none) */
    void (*draw)(void *ctx, ls027_frame *f);    /* the whole 400x240 frame: after every tick, and a key that returned true */
    void (*save)(void *ctx, uint8_t *save, uint32_t cap);       /* cap APP_SAVE_SIZE */
    void (*stop)(void *ctx);
} app_def;
void app_beep(uint16_t hz, uint16_t ms);  /* framework-provided: at once, non-blocking, honours VOL and BEEP */
uint32_t app_now_ms(void);                /* added at abacus's request (#4506): the app's clock, for draw */
```

- Calls: load, start, then key or tick (each tick followed by draw), then save, stop.
- key and tick share one clock, ms since start; a tick's argument is the time since the last.
- C (key 42) always exits and is never delivered. Keys are row*6+col (firmware/keymap.c KEYMAP,
  layout docs/hw/stu32-v0.json).
- ls027_frame: `uint8_t px[240][50]`, bit (x % 8) of byte x/8, set = dark.
- app_beep: VOL sets the level; SETUP > BEEP gains an "APP" use, on by default; a no-op without a buzzer.
- Frame rate on Rev A's LS027 not measured yet: plan for 10-20 full frames a second.

What it means for Babal (stage 2): the ctx must hold the level in at most 32 KB with no
allocation, so the 10,000-row ring and the scores log become windows of recent rows, with a
counter the oracle checks stays 0 (no read outside the window). delta = ms / 1000.0, clamped
in the app (quirks 7 and 8). Key ms map to the rules' nanosecond instants (ms * 1,000,000).
