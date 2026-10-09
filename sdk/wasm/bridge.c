/* SPDX-License-Identifier: MIT */
/* The bridge (bridge.h): WAMR (wasm_export.h) between the framework's app_def calls and a game built with
 * sdk/wasm/guest.c. WAMR is initialised at a game's load and destroyed at its stop, so it holds nothing
 * while the calculator runs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge.h"
#include "wasm_export.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#define BRIDGE_STACK_GUARD 2048 /* WAMR's 1 KB default left 212 B after a trap on the S3; 2 KB (abacus #5450) */
#include "esp_heap_caps.h"
/* WAMR's allocations and the module's copy in PSRAM: the calculator's internal heap is the tight one, and none
   of WAMR runs with the cache off (soroban #5501) */
#define BRIDGE_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
static void *bridge_malloc(unsigned int n)
{
    return heap_caps_malloc(n, BRIDGE_CAPS);
}
static void *bridge_realloc(void *p, unsigned int n)
{
    return heap_caps_realloc(p, n, BRIDGE_CAPS);
}
static void bridge_free(void *p)
{
    heap_caps_free(p);
}
#else
static void *bridge_malloc(unsigned int n)
{
    return malloc(n);
}
static void *bridge_realloc(void *p, unsigned int n)
{
    return realloc(p, n);
}
static void bridge_free(void *p)
{
    free(p);
}
#endif

#define WASM_STACK 8192 /* WAMR's operand stack for the instance (AOT barely uses it) */

static void imp_beep(wasm_exec_env_t env, int32_t hz, int32_t ms)
{
    (void)env;
    app_beep((uint16_t)hz, (uint16_t)ms);
}
static int32_t imp_now_ms(wasm_exec_env_t env)
{
    (void)env;
    return (int32_t)app_now_ms();
}
static int32_t imp_seed(wasm_exec_env_t env)
{
    (void)env;
    return (int32_t)app_seed();
}
static void imp_tune(wasm_exec_env_t env, int32_t notes, int32_t n)
{
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    if (n < 0 || n > 255 || !wasm_runtime_validate_app_addr(inst, (uint64_t)(uint32_t)notes, (uint64_t)n * 4))
        return; /* n out of range is ignored; notes outside the guest's memory set the exception (a trap) */
    app_tune(wasm_runtime_addr_app_to_native(inst, (uint64_t)(uint32_t)notes), (uint8_t)n);
}

/* WAMR takes each import as a void *; ISO C has no conversion from a function pointer, so a union */
static void *fn(void (*f)(void))
{
    union {
        void (*f)(void);
        void *p;
    } u;
    u.f = f;
    return u.p;
}

static NativeSymbol imports[4];

static void fill_imports(void)
{
    imports[0] = (NativeSymbol){"app_beep", fn((void (*)(void))imp_beep), "(ii)", NULL};
    imports[1] = (NativeSymbol){"app_now_ms", fn((void (*)(void))imp_now_ms), "()i", NULL};
    imports[2] = (NativeSymbol){"app_seed", fn((void (*)(void))imp_seed), "()i", NULL};
    imports[3] = (NativeSymbol){"app_tune", fn((void (*)(void))imp_tune), "(ii)", NULL};
}

static int runtimes; /* games loaded; WAMR is up while this is non-zero */

static void fail(wasm_bridge_ctx *c, const char *what, const char *why)
{
    c->dead = true;
    snprintf(c->error, sizeof c->error, "%.40s: %.50s", what, why ? why : "?"); /* both bounded: 93 of 96 */
    printf("wasm %s: %s\n", c->game ? c->game->name : "?", c->error);
}

static bool call(wasm_bridge_ctx *c, const char *name, uint32_t argc, uint32_t *argv)
{
    if (c->dead)
        return false;
    wasm_function_inst_t f = wasm_runtime_lookup_function(c->inst, name);
    if (!f) {
        fail(c, name, "not exported");
        return false;
    }
    if (!wasm_runtime_call_wasm(c->env, f, argc, argv)) {
        fail(c, name, wasm_runtime_get_exception(c->inst));
        return false;
    }
    return true;
}

/* the native address of `size` bytes at the guest pointer `app`, or NULL */
static uint8_t *guest_bytes(wasm_bridge_ctx *c, uint32_t app, uint32_t size)
{
    if (!wasm_runtime_validate_app_addr(c->inst, app, size))
        return NULL;
    return wasm_runtime_addr_app_to_native(c->inst, app);
}

void wasm_bridge_load(const wasm_game *g, void *ctx, const uint8_t *save, uint32_t n)
{
    wasm_bridge_ctx *c = ctx;
    char err[96] = "";  /* wasm_runtime_create_exec_env has no error text of its own */
    uint32_t argv[1];
    c->game = g;
    if (runtimes == 0) {
        RuntimeInitArgs init;
        fill_imports();
        memset(&init, 0, sizeof init);
        init.mem_alloc_type = Alloc_With_Allocator;
        init.mem_alloc_option.allocator.malloc_func = fn((void (*)(void))bridge_malloc);
        init.mem_alloc_option.allocator.realloc_func = fn((void (*)(void))bridge_realloc);
        init.mem_alloc_option.allocator.free_func = fn((void (*)(void))bridge_free);
        init.native_module_name = "env";
        init.native_symbols = imports;
        init.n_native_symbols = sizeof imports / sizeof imports[0];
        if (!wasm_runtime_full_init(&init)) {
            fail(c, "init", "wasm_runtime_full_init failed");
            return;
        }
    }
    runtimes++;
    c->counted = true;
    uint32_t aot_size = (uint32_t)(g->aot_end - g->aot);
    c->aot_copy = bridge_malloc(aot_size);
    if (!c->aot_copy) {
        fail(c, "load", "no memory for the module");
        return;
    }
    memcpy(c->aot_copy, g->aot, aot_size);
    if (!(c->module = wasm_runtime_load(c->aot_copy, aot_size, err, sizeof err))
        || !(c->inst = wasm_runtime_instantiate(c->module, WASM_STACK, 0, err, sizeof err))
        || !(c->env = wasm_runtime_create_exec_env(c->inst, WASM_STACK))) {
        fail(c, "load", err[0] ? err : "no exec env");
        return;
    }
#ifdef ESP_PLATFORM
    /* the calling task's own stack start: WAMR's lookup returns NULL without the FreeRTOS trace facility,
       which would leave the AOT code's native stack checks without a boundary */
    wasm_runtime_set_native_stack_boundary(c->env, pxTaskGetStackStart(NULL) + BRIDGE_STACK_GUARD);
#endif
    if (!call(c, "_initialize", 0, NULL) || !call(c, "game_io", 0, argv))
        return;
    uint32_t cap = g->save_size ? g->save_size : APP_SAVE_SIZE;
    uint8_t *io = guest_bytes(c, argv[0], cap);
    if (!io) {
        fail(c, "load", "game_io out of bounds");
        return;
    }
    if (n > cap)
        n = 0; /* a save that does not fit is not taken */
    if (n)
        memcpy(io, save, n);
    argv[0] = n;
    if (call(c, "game_load", 1, argv) && !argv[0])
        fail(c, "load", "the game has no memory for its ctx");
}

void wasm_bridge_start(void *ctx)
{
    call(ctx, "game_start", 0, NULL);
}

bool wasm_bridge_key(void *ctx, int key, bool down, uint32_t ms)
{
    uint32_t argv[3] = {(uint32_t)key, down, ms};
    return call(ctx, "game_key", 3, argv) && argv[0];
}

uint32_t wasm_bridge_tick(void *ctx, uint32_t ms)
{
    uint32_t argv[1] = {ms};
    return call(ctx, "game_tick", 1, argv) ? argv[0] : 0;
}

void wasm_bridge_draw(void *ctx, ls027_frame *f)
{
    wasm_bridge_ctx *c = ctx;
    uint32_t argv[1];
    uint8_t *px;
    if (!call(c, "game_draw", 0, argv) || !(px = guest_bytes(c, argv[0], sizeof f->px))) {
        memset(f, 0, sizeof *f);
        return;
    }
    memcpy(f->px, px, sizeof f->px);
}

void wasm_bridge_save(void *ctx, uint8_t *save, uint32_t cap)
{
    wasm_bridge_ctx *c = ctx;
    uint32_t argv[1] = {cap};
    uint8_t *io;
    if (!call(c, "game_save", 1, argv) || !call(c, "game_io", 0, argv) || !(io = guest_bytes(c, argv[0], cap)))
        return;
    memcpy(save, io, cap);
}

void wasm_bridge_stop(void *ctx)
{
    wasm_bridge_ctx *c = ctx;
    if (c->inst && !c->dead)
        call(c, "game_stop", 0, NULL);
    if (c->env)
        wasm_runtime_destroy_exec_env(c->env);
    if (c->inst)
        wasm_runtime_deinstantiate(c->inst);
    if (c->module)
        wasm_runtime_unload(c->module);
    bridge_free(c->aot_copy);
    if (c->counted && --runtimes == 0)
        wasm_runtime_destroy();
    c->counted = false;
    c->env = c->inst = c->module = NULL;
    c->aot_copy = NULL;
}

const char *wasm_bridge_error(void *ctx)
{
    wasm_bridge_ctx *c = ctx;
    return c->error[0] ? c->error : NULL;
}
