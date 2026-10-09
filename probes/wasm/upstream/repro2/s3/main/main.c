/* SPDX-License-Identifier: MIT */
/* Load an embedded .aot, instantiate it, call add(2, 3), print the result. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wasm_export.h"

extern const uint8_t mod_start[] asm("_binary_module_aot_start");
extern const uint8_t mod_end[] asm("_binary_module_aot_end");

static void *run(void *arg)
{
    char err[128];
    RuntimeInitArgs init;
    uint32_t n = (uint32_t)(mod_end - mod_start), argv[2] = {2, 3};
    uint8_t *buf = malloc(n);
    (void)arg;
    memset(&init, 0, sizeof init);
    init.mem_alloc_type = Alloc_With_System_Allocator;
    if (!buf || !wasm_runtime_full_init(&init)) {
        printf("FAIL init\n");
        return NULL;
    }
    memcpy(buf, mod_start, n);
    wasm_module_t mod = wasm_runtime_load(buf, n, err, sizeof err);
    if (!mod) {
        printf("FAIL load: %s\n", err);
        return NULL;
    }
    wasm_module_inst_t inst = wasm_runtime_instantiate(mod, 16 * 1024, 0, err, sizeof err);
    if (!inst) {
        printf("FAIL instantiate: %s\n", err);
        return NULL;
    }
    wasm_exec_env_t env = wasm_runtime_create_exec_env(inst, 16 * 1024);
    wasm_function_inst_t f = wasm_runtime_lookup_function(inst, "add");
    printf("calling add(2, 3)\n");
    if (!env || !f || !wasm_runtime_call_wasm(env, f, 2, argv)) {
        printf("FAIL call: %s\n", wasm_runtime_get_exception(inst));
        return NULL;
    }
    printf("add(2, 3) = %u\n", (unsigned)argv[0]);
    return NULL;
}

void app_main(void)
{
    pthread_attr_t a;
    pthread_t t;
    vTaskDelay(pdMS_TO_TICKS(3000));
    printf("mini: module %u bytes\n", (unsigned)(mod_end - mod_start));
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 16 * 1024);
    pthread_create(&t, &a, run, NULL);
    pthread_join(t, NULL);
    printf("PROBE DONE\n");
}
