/* SPDX-License-Identifier: MIT */
/* The wasm probe's guest (arcade, for abacus #5025): Babal's own app (app/babal_app.c, app/render.c,
 * babal/) played as app-far plays it (a noob game, 4 and 6 held in turn every 300 ms, a jump every
 * 700 ms, 20 ms ticks), plus two small loops. The same file builds native (the S3 bench, the host)
 * and as wasm32, so the frames' hash must agree everywhere. The framework's three calls are stubbed
 * here (no imports), app_seed as the framework's host choice. */
#include <stdint.h>
#include <string.h>

#include "babal_app.h"

#ifdef __wasm__
#define EXPORT(name) __attribute__((export_name(name)))
#else
#define EXPORT(name)
#endif

static babal_ctx ctx;
static ls027_frame frame;
static uint32_t clock_ms, swap_at, hop_at;
static int held;

void app_beep(uint16_t hz, uint16_t ms)
{
    (void)hz, (void)ms;
}
uint32_t app_now_ms(void)
{
    return clock_ms;
}
uint32_t app_seed(void)
{
    uint32_t x = clock_ms ^ 0x9E3779B9u;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return x;
}

EXPORT("drive_init") void drive_init(void)
{
    memset(&ctx, 0, sizeof ctx);
    clock_ms = 0;
    babal_app.load(&ctx, NULL, 0);
    babal_app.start(&ctx);
    clock_ms = 20;
    babal_app.tick(&ctx, 20);
    babal_app.key(&ctx, APP_KEY_SOFT1, true, 30);
    babal_app.key(&ctx, APP_KEY_SOFT1, false, 30);
    held = APP_KEY_4;
    babal_app.key(&ctx, held, true, 30);
    swap_at = hop_at = 30;
}

EXPORT("drive_tick") uint32_t drive_tick(void)
{
    clock_ms += 20;
    if (clock_ms - swap_at >= 300) {
        babal_app.key(&ctx, held, false, clock_ms - 20);
        held = held == APP_KEY_4 ? APP_KEY_6 : APP_KEY_4;
        babal_app.key(&ctx, held, true, clock_ms - 20);
        swap_at = clock_ms;
    }
    if (clock_ms - hop_at >= 700) {
        babal_app.key(&ctx, APP_KEY_8, true, clock_ms - 20);
        babal_app.key(&ctx, APP_KEY_8, false, clock_ms - 18);
        hop_at = clock_ms;
    }
    return babal_app.tick(&ctx, 20);
}

EXPORT("drive_draw") void drive_draw(void)
{
    babal_app.draw(&ctx, &frame);
}

/* FNV-1a 64 over the frame, chained from h (not timed: the bench calls it between draws) */
EXPORT("drive_hash") uint64_t drive_hash(uint64_t h)
{
    const uint8_t *b = &frame.px[0][0];
    for (size_t i = 0; i < sizeof frame.px; i++) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

EXPORT("drive_row") int32_t drive_row(void)
{
    return (int32_t)bb_player_level_row(&ctx.player);
}

/* the small loops: integer (xorshift) and f64 (a damped recurrence), n steps */
EXPORT("loop_i32") uint32_t loop_i32(uint32_t n)
{
    uint32_t x = 2463534242u, s = 0;
    for (uint32_t i = 0; i < n; i++) {
        x ^= x << 13, x ^= x >> 17, x ^= x << 5;
        s += x >> 7;
    }
    return s;
}

EXPORT("loop_f64") double loop_f64(uint32_t n)
{
    double x = 0.5, v = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        v = v * 0.999 + (0.25 - x) * 0.01;
        x = x + v;
    }
    return x;
}
