/* SPDX-License-Identifier: MIT */
/* On a plain FreeRTOS task (as the firmware's calc task): add(2, 3), a shallow deep(10), a deep(1000000)
 * that must overflow the native stack, then add(2, 3) again. SET_BOUNDARY: the bridge's way, the task's
 * stack start + WAMR's 1 KB guard as the exec env's native stack boundary. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "wasm_export.h"

#define TASK_STACK 16384
extern const uint8_t mod_start[] asm("_binary_module_aot_start");
extern const uint8_t mod_end[] asm("_binary_module_aot_end");
static wasm_module_inst_t inst;
static wasm_exec_env_t env;

static void call(const char *name, uint32_t a, uint32_t b, int argc)
{
    uint32_t argv[2] = {a, b};
    wasm_function_inst_t f = wasm_runtime_lookup_function(inst, name);
    if (!f || !wasm_runtime_call_wasm(env, f, (uint32_t)argc, argv)) {
        printf("%s(%u): trapped: %s\n", name, (unsigned)a, wasm_runtime_get_exception(inst));
        wasm_runtime_clear_exception(inst);
        return;
    }
    printf("%s(%u%s) = %u\n", name, (unsigned)a, argc == 2 ? ", 3" : "", (unsigned)argv[0]);
}

static void run(void *arg)
{
    char err[128];
    RuntimeInitArgs init;
    uint32_t n = (uint32_t)(mod_end - mod_start);
    uint8_t *buf = malloc(n);
    (void)arg;
    memset(&init, 0, sizeof init);
    init.mem_alloc_type = Alloc_With_System_Allocator;
    if (!buf || !wasm_runtime_full_init(&init)) {
        printf("FAIL init\n");
        vTaskDelete(NULL);
    }
    memcpy(buf, mod_start, n);
    wasm_module_t mod = wasm_runtime_load(buf, n, err, sizeof err);
    inst = mod ? wasm_runtime_instantiate(mod, 8 * 1024, 0, err, sizeof err) : NULL;
    env = inst ? wasm_runtime_create_exec_env(inst, 8 * 1024) : NULL;
    if (!env) {
        printf("FAIL load: %s\n", err);
        vTaskDelete(NULL);
    }
#ifdef SET_BOUNDARY
    uint8_t *start = pxTaskGetStackStart(NULL);
    wasm_runtime_set_native_stack_boundary(env, start + 1024);
    printf("boundary: task stack starts %p, boundary %p\n", (void *)start, (void *)(start + 1024));
#else
    printf("boundary: none set\n");
#endif
    call("add", 2, 3, 2);
    call("deep", 10, 0, 1);
    call("deep", 1000000, 0, 1);
    call("add", 2, 3, 2);
    printf("stack: %u of %u bytes never used\n", (unsigned)uxTaskGetStackHighWaterMark(NULL), TASK_STACK);
    printf("PROBE DONE\n");
    vTaskDelete(NULL);
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    printf("guard: module %u bytes, on a FreeRTOS task (not a pthread)\n", (unsigned)(mod_end - mod_start));
    xTaskCreatePinnedToCore(run, "calclike", TASK_STACK, NULL, 5, NULL, 1);
}
