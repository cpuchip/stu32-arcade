/* SPDX-License-Identifier: MIT */
/* 3720, MOLES and CATCH on the S3: each game's app scripts played through apps.h's calls as the framework makes them, with app_seed() and the
 * loaded save as the SDK runner's -seed and -load give them, the ctx in PSRAM as the framework gives it, the frame in
 * internal RAM. For each script: the hash of its frames' hashes, the frames drawn after a key that asks for one
 * included as the runner's -keyframes takes them (scripts/device_kids.sh compares it with the runner's), and key, tick and draw times apart. The report repeats, so a reader that attaches late still gets a
 * whole one. */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "appscripts.h"
#include "apps.h"

static EXT_RAM_BSS_ATTR uint64_t ctx[4096 / 8];
static ls027_frame frame;
static uint32_t clock_ms, seed;
void app_beep(uint16_t hz, uint16_t ms)
{
    (void)hz, (void)ms;
}
void app_tune(const uint16_t *notes, uint8_t n)
{
    (void)notes, (void)n;
}
uint32_t app_now_ms(void)
{
    return clock_ms;
}
uint32_t app_seed(void)
{
    return seed; /* the script's, as the runner's -seed gives it */
}

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

typedef struct {
    uint64_t hash;
    uint32_t frames;
    int64_t key_max, tick_total, tick_max, draw_total, draw_max;
} result;
static result results[APPSCRIPT_COUNT];

/* as the runner's -hashes writes them: the frame's hash as 16 hex digits and a newline */
static void hash_frame(result *r)
{
    uint64_t fh = fnv(0xcbf29ce484222325ULL, frame.px, sizeof frame.px);
    char hex[18];
    snprintf(hex, sizeof hex, "%016" PRIx64 "\n", fh);
    r->hash = fnv(r->hash, hex, 17);
    r->frames++;
}

static void play(const appscript *s, result *r)
{
    const app_def *app = s->app;
    memset(ctx, 0, sizeof ctx);
    clock_ms = 0;
    seed = s->seed;
    app->load(ctx, s->save, s->save_len);
    app->start(ctx);
    r->hash = 0xcbf29ce484222325ULL;
    const char *p = s->text, *end = s->text + s->len;
    char line[64];
    while (p < end) {
        const char *e = memchr(p, '\n', (size_t)(end - p));
        if (!e)
            e = end;
        size_t n = (size_t)(e - p) < sizeof line - 1 ? (size_t)(e - p) : sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        p = e + 1;
        int k, d;
        unsigned ms;
        if (sscanf(line, "key %d %d %u", &k, &d, &ms) == 3) {
            int64_t t0 = esp_timer_get_time();
            bool draw = app->key(ctx, k, d != 0, ms);
            int64_t dt = esp_timer_get_time() - t0;
            if (dt > r->key_max) r->key_max = dt;
            if (draw) {
                app->draw(ctx, &frame);
                hash_frame(r);
            }
        } else if (sscanf(line, "tick %u", &ms) == 1) {
            clock_ms += ms;
            int64_t t0 = esp_timer_get_time();
            app->tick(ctx, ms);
            int64_t t1 = esp_timer_get_time();
            app->draw(ctx, &frame);
            int64_t t2 = esp_timer_get_time();
            if (t1 - t0 > r->tick_max) r->tick_max = t1 - t0;
            if (t2 - t1 > r->draw_max) r->draw_max = t2 - t1;
            r->tick_total += t1 - t0;
            r->draw_total += t2 - t1;
            hash_frame(r);
        }
    }
}

static void bench(void *arg)
{
    (void)arg;
    for (int i = 0; i < APPSCRIPT_COUNT; i++)
        if (appscripts[i].app->ctx_size > sizeof ctx) {
            for (;;) {
                printf("BENCH FAIL: %s ctx %u > %u\n", appscripts[i].app->name, (unsigned)appscripts[i].app->ctx_size,
                       (unsigned)sizeof ctx);
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    for (int i = 0; i < APPSCRIPT_COUNT; i++) {
        play(&appscripts[i], &results[i]);
        printf("ran %d/%d %s\n", i + 1, APPSCRIPT_COUNT, appscripts[i].name);
    }
    for (;;) {
        printf("BENCH BEGIN %d\n", APPSCRIPT_COUNT);
        for (int i = 0; i < APPSCRIPT_COUNT; i++) {
            const result *r = &results[i];
            printf("A %s %016" PRIx64 " %" PRIu32 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
                   appscripts[i].name, r->hash, r->frames, r->tick_total, r->tick_max, r->draw_total, r->draw_max,
                   r->key_max);
        }
        printf("BENCH DONE\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore(bench, "bench", 32768, NULL, 5, NULL, 1);
}
