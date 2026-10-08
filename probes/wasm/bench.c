/* SPDX-License-Identifier: MIT */
/* The wasm probe's bench (arcade, abacus #5025): Babal's app (guest/drive.c) driven natively, under
 * wasm3, or under WAMR, on the host or the S3; one backend per build (-DB_NATIVE, -DB_WASM3,
 * -DB_WAMR). Prints the frames' chained hash (must agree across backends), the furthest row, tick and
 * draw times (average and worst, us), and two small loops' times. Host: bench [babal.wasm]. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
static int64_t now_us(void)
{
    return esp_timer_get_time();
}
#else
#include <time.h>
static int64_t now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
#endif

#ifndef TICKS
#define TICKS 3000
#endif
#define LOOP_N 1000000u

static void fail(const char *what, const char *why)
{
    printf("FAIL %s: %s\n", what, why ? why : "?");
    exit(1);
}

/* ---- the three backends, one interface ---- */
#if defined(B_NATIVE)
#define BACKEND "native"
void drive_init(void);
uint32_t drive_tick(void);
void drive_draw(void);
uint64_t drive_hash(uint64_t h);
int32_t drive_row(void);
uint32_t loop_i32(uint32_t n);
double loop_f64(uint32_t n);
static void be_load(const uint8_t *wasm, uint32_t n)
{
    (void)wasm, (void)n;
}
static void g_init(void) { drive_init(); }
static uint32_t g_tick(void) { return drive_tick(); }
static void g_draw(void) { drive_draw(); }
static uint64_t g_hash(uint64_t h) { return drive_hash(h); }
static int32_t g_row(void) { return drive_row(); }
static uint32_t g_li(uint32_t n) { return loop_i32(n); }
static double g_lf(uint32_t n) { return loop_f64(n); }

#elif defined(B_WASM3)
#define BACKEND "wasm3"
#include "wasm3.h"
#ifndef M3_STACK
#define M3_STACK (32 * 1024)
#endif
static IM3Function f_init, f_tick, f_draw, f_hash, f_row, f_li, f_lf, f_ini;
static void find(IM3Runtime rt, IM3Function *f, const char *name)
{
    M3Result r = m3_FindFunction(f, rt, name);
    if (r)
        fail(name, r);
}
static void be_load(const uint8_t *wasm, uint32_t n)
{
    IM3Environment env = m3_NewEnvironment();
    IM3Runtime rt = env ? m3_NewRuntime(env, M3_STACK, NULL) : NULL;
    IM3Module mod;
    if (!rt)
        fail("wasm3", "no runtime");
    M3Result r = m3_ParseModule(env, &mod, wasm, n);
    if (r)
        fail("parse", r);
    if ((r = m3_LoadModule(rt, mod)))
        fail("load", r);
    find(rt, &f_ini, "_initialize");
    find(rt, &f_init, "drive_init");
    find(rt, &f_tick, "drive_tick");
    find(rt, &f_draw, "drive_draw");
    find(rt, &f_hash, "drive_hash");
    find(rt, &f_row, "drive_row");
    find(rt, &f_li, "loop_i32");
    find(rt, &f_lf, "loop_f64");
    if ((r = m3_CallV(f_ini)))
        fail("_initialize", r);
}
static void call0(IM3Function f, const char *what)
{
    M3Result r = m3_CallV(f);
    if (r)
        fail(what, r);
}
static void g_init(void) { call0(f_init, "init"); }
static uint32_t g_tick(void)
{
    uint32_t v = 0;
    call0(f_tick, "tick");
    m3_GetResultsV(f_tick, &v);
    return v;
}
static void g_draw(void) { call0(f_draw, "draw"); }
static uint64_t g_hash(uint64_t h)
{
    uint64_t v = 0;
    M3Result r = m3_CallV(f_hash, h);
    if (r)
        fail("hash", r);
    m3_GetResultsV(f_hash, &v);
    return v;
}
static int32_t g_row(void)
{
    int32_t v = 0;
    call0(f_row, "row");
    m3_GetResultsV(f_row, &v);
    return v;
}
static uint32_t g_li(uint32_t n)
{
    uint32_t v = 0;
    M3Result r = m3_CallV(f_li, n);
    if (r)
        fail("loop_i32", r);
    m3_GetResultsV(f_li, &v);
    return v;
}
static double g_lf(uint32_t n)
{
    double v = 0;
    M3Result r = m3_CallV(f_lf, n);
    if (r)
        fail("loop_f64", r);
    m3_GetResultsV(f_lf, &v);
    return v;
}

#elif defined(B_WAMR)
#define BACKEND "wamr"
#include "wasm_export.h"
#ifndef WAMR_STACK
#define WAMR_STACK (16 * 1024)
#endif
static wasm_module_inst_t inst;
static wasm_exec_env_t ee;
static wasm_function_inst_t f_init, f_tick, f_draw, f_hash, f_row, f_li, f_lf;
static wasm_function_inst_t find(const char *name)
{
    wasm_function_inst_t f = wasm_runtime_lookup_function(inst, name);
    if (!f)
        fail(name, "not exported");
    return f;
}
static void call(wasm_function_inst_t f, uint32_t argc, uint32_t *argv, const char *what)
{
    if (!wasm_runtime_call_wasm(ee, f, argc, argv))
        fail(what, wasm_runtime_get_exception(inst));
}
static void be_load(const uint8_t *wasm, uint32_t n)
{
    static char err[128];
    RuntimeInitArgs args;
    memset(&args, 0, sizeof args);
    args.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&args))
        fail("wamr", "init");
    uint8_t *copy = malloc(n); /* wasm_runtime_load may write its buffer */
    if (!copy)
        fail("wamr", "no memory for the module");
    memcpy(copy, wasm, n);
    wasm_module_t mod = wasm_runtime_load(copy, n, err, sizeof err);
    if (!mod)
        fail("load", err);
    inst = wasm_runtime_instantiate(mod, WAMR_STACK, 0, err, sizeof err);
    if (!inst)
        fail("instantiate", err);
    ee = wasm_runtime_create_exec_env(inst, WAMR_STACK);
    if (!ee)
        fail("exec env", "none");
    wasm_function_inst_t ini = wasm_runtime_lookup_function(inst, "_initialize");
#ifndef WAMR_SKIP_INITIALIZE
    if (ini)
        call(ini, 0, NULL, "_initialize");
#else
    (void)ini;
#endif
    f_init = find("drive_init");
    f_tick = find("drive_tick");
    f_draw = find("drive_draw");
    f_hash = find("drive_hash");
    f_row = find("drive_row");
    f_li = find("loop_i32");
    f_lf = find("loop_f64");
}
static void g_init(void) { call(f_init, 0, NULL, "init"); }
static uint32_t g_tick(void)
{
    uint32_t a[1] = {0};
    call(f_tick, 0, a, "tick");
    return a[0];
}
static void g_draw(void) { call(f_draw, 0, NULL, "draw"); }
static uint64_t g_hash(uint64_t h)
{
    uint32_t a[2];
    memcpy(a, &h, 8);
    call(f_hash, 2, a, "hash");
    memcpy(&h, a, 8);
    return h;
}
static int32_t g_row(void)
{
    uint32_t a[1] = {0};
    call(f_row, 0, a, "row");
    return (int32_t)a[0];
}
static uint32_t g_li(uint32_t n)
{
    uint32_t a[1] = {n};
    call(f_li, 1, a, "loop_i32");
    return a[0];
}
static double g_lf(uint32_t n)
{
    uint32_t a[2] = {n, 0};
    double d;
    call(f_lf, 1, a, "loop_f64");
    memcpy(&d, a, 8);
    return d;
}
#else
#error "build with -DB_NATIVE, -DB_WASM3 or -DB_WAMR"
#endif

#ifdef ESP_PLATFORM
static size_t free_int(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}
static size_t free_ext(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
#endif

void bench_run(const uint8_t *wasm, uint32_t n)
{
#ifdef ESP_PLATFORM
    size_t i0 = free_int(), e0 = free_ext();
#endif
    int64_t t = now_us();
    be_load(wasm, n);
    int64_t load_us = now_us() - t;
#ifdef ESP_PLATFORM
    size_t i1 = free_int(), e1 = free_ext();
    printf("ram %s: internal %u bytes, psram %u bytes (load and instantiate)\n", BACKEND, (unsigned)(i0 - i1),
           (unsigned)(e0 - e1));
#endif
    g_init();
    uint64_t h = 0xcbf29ce484222325ULL;
    int64_t tick_sum = 0, tick_max = 0, draw_sum = 0, draw_max = 0;
    int32_t far = 0;
    for (int i = 0; i < TICKS; i++) {
        int64_t a = now_us();
        g_tick();
        int64_t b = now_us();
        g_draw();
        int64_t c = now_us();
        tick_sum += b - a, draw_sum += c - b;
        if (b - a > tick_max)
            tick_max = b - a;
        if (c - b > draw_max)
            draw_max = c - b;
        h = g_hash(h);
        int32_t r = g_row();
        if (r > far)
            far = r;
    }
    t = now_us();
    uint32_t li = g_li(LOOP_N);
    int64_t li_us = now_us() - t;
    t = now_us();
    double lf = g_lf(LOOP_N);
    int64_t lf_us = now_us() - t;
    uint64_t lfb;
    memcpy(&lfb, &lf, 8);
    printf("bench %s: hash %016" PRIx64 " ticks %d furthest row %d; load %" PRId64 " us\n", BACKEND, h, TICKS, (int)far,
           load_us);
    printf("bench %s: tick %" PRId64 " us avg %" PRId64 " max; draw %" PRId64 " us avg %" PRId64 " max\n", BACKEND,
           tick_sum / TICKS, tick_max, draw_sum / TICKS, draw_max);
    printf("bench %s: loop_i32 %u steps %" PRId64 " us (= %08" PRIx32 "); loop_f64 %u steps %" PRId64 " us (= %016" PRIx64
           ")\n", BACKEND, LOOP_N, li_us, li, LOOP_N, lf_us, lfb);
}

#ifndef ESP_PLATFORM
int main(int argc, char **argv)
{
    static uint8_t buf[1 << 20];
    uint32_t n = 0;
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f)
            fail(argv[1], "cannot open");
        n = (uint32_t)fread(buf, 1, sizeof buf, f);
        fclose(f);
    }
    bench_run(buf, n);
    return 0;
}
#endif
