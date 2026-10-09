/* SPDX-License-Identifier: MIT */
/* The bridge: a WebAssembly game (an .aot built from sdk/wasm/guest.c) run through WAMR as an ordinary
 * app_def. One WASM_GAME line per game makes its app_def; the framework calls it like any app. A trap
 * ends the game cleanly (tick returns 0, the frame is cleared, the exception is kept and logged). */
#ifndef STU32_WASM_BRIDGE_H
#define STU32_WASM_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "apps.h"

typedef struct {
    const char *name;     /* APPS' label, as the game's manifest gives it */
    const uint8_t *aot, *aot_end; /* the .aot, as embedded in the image (EMBED_FILES' start and end) */
    uint32_t save_size;   /* the manifest's save_size (0: APP_SAVE_SIZE) */
} wasm_game;

typedef struct {
    const wasm_game *game;
    void *module, *inst, *env; /* WAMR's handles (wasm_export.h types, kept opaque here) */
    uint8_t *aot_copy;         /* WAMR's loader may write its buffer */
    bool counted, dead; /* counted: this game holds a share of WAMR's runtime */
    char error[96];
} wasm_bridge_ctx;

void wasm_bridge_load(const wasm_game *g, void *ctx, const uint8_t *save, uint32_t n);
void wasm_bridge_start(void *ctx);
bool wasm_bridge_key(void *ctx, int key, bool down, uint32_t ms);
uint32_t wasm_bridge_tick(void *ctx, uint32_t ms);
void wasm_bridge_draw(void *ctx, ls027_frame *f);
void wasm_bridge_save(void *ctx, uint8_t *save, uint32_t cap);
void wasm_bridge_stop(void *ctx);
/* the exception that ended the game, or NULL */
const char *wasm_bridge_error(void *ctx);

#define WASM_GAME(sym, label, aot_start, aot_stop, save_bytes)                                     \
    static const wasm_game sym##_game = {label, aot_start, aot_stop, save_bytes};                  \
    static void sym##_load(void *c, const uint8_t *s, uint32_t n)                                   \
    {                                                                                               \
        wasm_bridge_load(&sym##_game, c, s, n);                                                     \
    }                                                                                               \
    const app_def sym = {label,           sizeof(wasm_bridge_ctx), sym##_load,       wasm_bridge_start, \
                         wasm_bridge_key, wasm_bridge_tick,        wasm_bridge_draw, wasm_bridge_save,  \
                         wasm_bridge_stop, save_bytes}

#endif
