/* SPDX-License-Identifier: MIT */
/* A test game for the S3 gate (soroban #5467), never listed: run through the bridge on the calc task, key 0
 * recurses 100 deep and returns (the control), any other key recurses without end, which the bridge's stack
 * guard must turn into a trap the calculator survives. Built by probes/wasm/trap/build.sh. */
#include <stdint.h>
#include <string.h>

#include "apps.h"

static volatile uint32_t sink; /* stored after each call returns, so no frame can be folded into a loop */

__attribute__((noinline)) static uint32_t deep(uint32_t n)
{
    if (n == 0)
        return sink;
    uint32_t r = deep(n - 1);
    sink = r;
    return r + n;
}

static void load(void *ctx, const uint8_t *save, uint32_t n)
{
    (void)ctx, (void)save, (void)n;
}
static void start(void *ctx)
{
    (void)ctx;
}
static bool key(void *ctx, int k, bool down, uint32_t ms)
{
    uint32_t *depth = ctx;
    (void)ms;
    if (!down)
        return false;
    *depth = deep(k == 0 ? 100u : 0x7fffffffu);
    return true;
}
static uint32_t tick(void *ctx, uint32_t ms)
{
    (void)ctx, (void)ms;
    return 0;
}
static void draw(void *ctx, ls027_frame *f)
{
    uint32_t depth = *(uint32_t *)ctx;
    memset(f, 0, sizeof *f);
    if (depth == 5050u) /* the control returned: the top row dark (a set bit is dark) */
        memset(f->px[0], 0xff, sizeof f->px[0]);
}
static void save(void *ctx, uint8_t *s, uint32_t cap)
{
    (void)ctx, (void)s, (void)cap;
}
static void stop(void *ctx)
{
    (void)ctx;
}

const app_def trap_app = {"TRAP", sizeof(uint32_t), load, start, key, tick, draw, save, stop, 0};
