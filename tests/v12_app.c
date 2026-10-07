/* SPDX-License-Identifier: MIT */
/* V12: a test app for sdk/runner/apprun.c's apps.h v1.2 calls (oracle/sdk_check.sh). It asks a
 * save_size of V12_SAVE_SIZE (100; 5000 for the bounds check), plays a 20-note tune (4 past
 * APP_TUNE_MAX), and saves: 'V', the seed app_seed() gave (4 bytes, little end first), 'L' when the
 * save it was given came back whole (V12_SAVE_SIZE bytes, its first and last marks), the cap it was
 * given (4 bytes), then '.' to the cap's last byte, which is 'Z'. */
#include <string.h>

#include "apps.h"

#ifndef V12_SAVE_SIZE
#define V12_SAVE_SIZE 100
#endif

typedef struct {
    uint32_t seed;
    int whole;
} v12_ctx;

static void v12_load(void *ctx, const uint8_t *save, uint32_t n)
{
    v12_ctx *c = ctx;
    c->whole = n == V12_SAVE_SIZE && save && save[0] == 'V' && save[n - 1] == 'Z';
}

static void v12_start(void *ctx)
{
    static const uint16_t notes[40] = {440, 50, 0, 10, 494, 50, 0, 10, 523, 50, 0, 10, 587, 50, 0, 10, 659, 50, 0,
                                       10,  698, 50, 0, 10,  784, 50, 0, 10,  880, 50, 0, 10,  988, 50, 0, 10,  1047, 50};
    v12_ctx *c = ctx;
    c->seed = app_seed();
    app_tune(notes, 20);
}

static bool v12_key(void *ctx, int key, bool down, uint32_t ms)
{
    (void)ctx, (void)key, (void)down, (void)ms;
    return false;
}

static uint32_t v12_tick(void *ctx, uint32_t ms)
{
    (void)ctx, (void)ms;
    return 0;
}

static void v12_draw(void *ctx, ls027_frame *f)
{
    (void)ctx;
    memset(f, 0, sizeof *f);
}

static void v12_save(void *ctx, uint8_t *save, uint32_t cap)
{
    v12_ctx *c = ctx;
    if (cap < 11)
        return;
    memset(save, '.', cap);
    save[0] = 'V';
    for (int i = 0; i < 4; i++) {
        save[1 + i] = (uint8_t)(c->seed >> (8 * i));
        save[6 + i] = (uint8_t)(cap >> (8 * i));
    }
    save[5] = c->whole ? 'L' : '-';
    save[cap - 1] = 'Z';
}

static void v12_stop(void *ctx)
{
    (void)ctx;
}

const app_def v12_app = {"V12", sizeof(v12_ctx), v12_load, v12_start, v12_key, v12_tick,
                         v12_draw, v12_save, v12_stop, V12_SAVE_SIZE};
