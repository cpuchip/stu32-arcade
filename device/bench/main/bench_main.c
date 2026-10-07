/* Babal's rules on the S3: each oracle game replayed in the device windows (512 rows, 256 log
 * rows, in PSRAM as the app's ctx is), its output hashed with FNV-1a 64 (scripts/device.sh
 * compares with the host's), the rules timed per tick (row generation and Player::tick). The
 * report repeats, so a reader that attaches late still gets a whole one. */
#include <inttypes.h>
#include <stdio.h>

#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "oracle.h"
#include "scripts.h"

#define LEVEL_WINDOW 512
#define LOG_WINDOW 256

static EXT_RAM_BSS_ATTR bb_row rows[LEVEL_WINDOW];
static EXT_RAM_BSS_ATTR bb_score_entry log_slots[LOG_WINDOW];

static uint64_t hash;
static void hash_sink(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        hash ^= (uint8_t)s[i];
        hash *= 0x100000001b3ULL;
    }
}
static int64_t now_us(void)
{
    return esp_timer_get_time();
}

typedef struct {
    uint64_t hash;
    game_result res;
    int rc;
} result;
static result results[SCRIPT_COUNT];

static void bench(void *arg)
{
    (void)arg;
    out_set_sink(hash_sink);
    for (int i = 0; i < SCRIPT_COUNT; i++) {
        hash = 0xcbf29ce484222325ULL;
        results[i].res = (game_result){.now_us = now_us};
        results[i].rc = game_run(scripts[i].text, scripts[i].len, rows, LEVEL_WINDOW, log_slots, LOG_WINDOW, true,
                                 &results[i].res);
        results[i].hash = hash;
        printf("ran %d/%d %s\n", i + 1, SCRIPT_COUNT, scripts[i].name);
    }
    for (;;) {
        printf("BENCH BEGIN %d\n", SCRIPT_COUNT);
        for (int i = 0; i < SCRIPT_COUNT; i++) {
            const game_result *r = &results[i].res;
            printf("J %s %016" PRIx64 " %" PRIu64 " %" PRId64 " %" PRId64 " %zu %zu %d\n", scripts[i].name,
                   results[i].hash, r->ticks, r->us_total, r->us_max, r->depth_level, r->depth_log, results[i].rc);
        }
        printf("BENCH DONE\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore(bench, "bench", 32768, NULL, 5, NULL, 1);
}
