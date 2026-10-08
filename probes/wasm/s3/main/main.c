/* SPDX-License-Identifier: MIT */
/* The wasm probe's S3 entry: the bench in a pthread (WAMR's ESP-IDF port needs one: os_self_thread
 * calls pthread_self), the same 64 KB stack for every backend, and the stack it used reported. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BENCH_STACK (64 * 1024)

void bench_run(const uint8_t *wasm, uint32_t n);
#ifdef B_NATIVE
static const uint8_t none[1];
#define WASM none
#define WASM_N 0u
#else
extern const uint8_t wasm_start[] asm("_binary_babal_mvp_wasm_start");
extern const uint8_t wasm_end[] asm("_binary_babal_mvp_wasm_end");
#define WASM wasm_start
#define WASM_N ((uint32_t)(wasm_end - wasm_start))
#endif

static void *bench_thread(void *arg)
{
    (void)arg;
    bench_run(WASM, WASM_N);
    printf("stack: %u of %u bytes used\n", (unsigned)(BENCH_STACK - uxTaskGetStackHighWaterMark(NULL)),
           (unsigned)BENCH_STACK);
    return NULL;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(3000)); /* the capture opens the console after the flash's reset */
    printf("probe: psram %u bytes, module %u bytes\n", (unsigned)esp_psram_get_size(), (unsigned)WASM_N);
    pthread_attr_t a;
    pthread_t t;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, BENCH_STACK);
    if (pthread_create(&t, &a, bench_thread, NULL) != 0) {
        printf("FAIL pthread_create\n");
        return;
    }
    pthread_join(t, NULL);
    printf("PROBE DONE\n");
}
