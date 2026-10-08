# SPDX-License-Identifier: MIT
set(BACKEND $ENV{BACKEND})
set(WASM $ENV{WASM})
set(ARCADE $ENV{ARCADE})
set(B ${WASM}/bench/bench.c ${CMAKE_CURRENT_LIST_DIR}/main.c)
if(BACKEND STREQUAL "native")
    file(GLOB RULES ${ARCADE}/babal/*.c)
    idf_component_register(SRCS ${B} ${WASM}/guest/drive.c ${ARCADE}/app/babal_app.c ${ARCADE}/app/render.c
                                ${ARCADE}/build/firmware/firmware/font5x7.c ${RULES}
                           INCLUDE_DIRS ${ARCADE}/app ${ARCADE}/babal ${ARCADE}/build/firmware/firmware
                           PRIV_REQUIRES esp_timer esp_psram pthread freertos)
    target_compile_options(${COMPONENT_LIB} PRIVATE -DB_NATIVE -ffp-contract=off -Wno-missing-field-initializers)
elseif(BACKEND STREQUAL "wasm3" OR BACKEND STREQUAL "wasm3flash")
    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        idf_build_get_property(build_dir BUILD_DIR)
        add_subdirectory(${WASM}/src/wasm3/source ${build_dir}/m3)
    endif()
    if(BACKEND STREQUAL "wasm3")   # wasm3's own ESP-IDF example: its core in internal RAM
        set(LF ${WASM}/src/wasm3/platforms/embedded/esp32-idf/main/linker.lf)
    else()
        set(LF "")
    endif()
    idf_component_register(SRCS ${B} PRIV_REQUIRES esp_timer esp_psram pthread freertos
                           EMBED_FILES ${WASM}/guest/babal-mvp.wasm LDFRAGMENTS ${LF})
    target_link_libraries(${COMPONENT_LIB} PRIVATE m3)
    if(BACKEND STREQUAL "wasm3")
        target_compile_options(m3 PUBLIC -DM3_IN_IRAM -DESP32 -O3 -freorder-blocks)
    else()
        target_compile_options(m3 PUBLIC -O3 -freorder-blocks)   # generic: no ESP32, so no IRAM_ATTR on the ops
    endif()
    target_compile_options(${COMPONENT_LIB} PRIVATE -DB_WASM3)
elseif(BACKEND STREQUAL "wamraot")
    idf_component_register(SRCS ${B} PRIV_REQUIRES esp_timer esp_psram pthread freertos wamr
                           EMBED_FILES ${WASM}/guest/babal-s3.aot)
    target_compile_options(${COMPONENT_LIB} PRIVATE -DB_WAMR -DGAME_AOT)
else()
    idf_component_register(SRCS ${B} PRIV_REQUIRES esp_timer esp_psram pthread freertos wamr
                           EMBED_FILES ${WASM}/guest/babal-mvp.wasm)
    target_compile_options(${COMPONENT_LIB} PRIVATE -DB_WAMR)
endif()
