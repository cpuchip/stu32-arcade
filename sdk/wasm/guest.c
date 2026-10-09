/* SPDX-License-Identifier: MIT */
/* The guest shim: any SDK game's app_def (-DAPP_DEF=babal_app) compiled to a WebAssembly reactor module.
 * The game's ctx and frame live in the module's linear memory; saves pass through game_io(). The four
 * framework calls (app_beep, app_now_ms, app_seed, app_tune) are left undefined and become imports from
 * "env", which the host's bridge (sdk/wasm/bridge.c) provides. Build: docs/wasm-integration.md. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "apps.h"

#ifndef APP_DEF
#error "build with -DAPP_DEF=<the game's app_def>"
#endif
extern const app_def APP_DEF;

#define EXPORT(name) __attribute__((export_name(name)))

static void *ctx;
static uint8_t io[APP_SAVE_MAX];
static ls027_frame frame;

static uint32_t save_size(void)
{
    return APP_DEF.save_size ? APP_DEF.save_size : APP_SAVE_SIZE;
}

EXPORT("game_name") const char *game_name(void)
{
    return APP_DEF.name;
}

EXPORT("game_save_size") uint32_t game_save_size(void)
{
    return save_size();
}

EXPORT("game_io") uint8_t *game_io(void)
{
    return io;
}

/* n bytes of save are in game_io(); 0 when nothing was saved. Returns 0 when the ctx cannot be had. */
EXPORT("game_load") int game_load(uint32_t n)
{
    if (APP_DEF.ctx_size > APP_CTX_MAX || n > save_size())
        return 0;
    ctx = calloc(1, APP_DEF.ctx_size ? APP_DEF.ctx_size : 1);
    if (!ctx)
        return 0;
    APP_DEF.load(ctx, n ? io : NULL, n);
    return 1;
}

EXPORT("game_start") void game_start(void)
{
    APP_DEF.start(ctx);
}

EXPORT("game_key") int game_key(int key, int down, uint32_t ms)
{
    return APP_DEF.key(ctx, key, down != 0, ms);
}

EXPORT("game_tick") uint32_t game_tick(uint32_t ms)
{
    return APP_DEF.tick(ctx, ms);
}

EXPORT("game_draw") ls027_frame *game_draw(void)
{
    APP_DEF.draw(ctx, &frame);
    return &frame;
}

/* the save, at most cap bytes, left in game_io() */
EXPORT("game_save") void game_save(uint32_t cap)
{
    if (cap > save_size())
        cap = save_size();
    memset(io, 0, cap);
    APP_DEF.save(ctx, io, cap);
}

EXPORT("game_stop") void game_stop(void)
{
    APP_DEF.stop(ctx);
    free(ctx);
    ctx = NULL;
}
