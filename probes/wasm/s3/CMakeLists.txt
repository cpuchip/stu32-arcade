# SPDX-License-Identifier: MIT
# The wasm probe's S3 bench (arcade, abacus #5025). One image a backend: -DBACKEND=native|wasm3|wamr
cmake_minimum_required(VERSION 3.16)
set(BACKEND $ENV{BACKEND})
set(WASM $ENV{WASM})
set(ARCADE $ENV{ARCADE})
if(BACKEND STREQUAL "wamr")
    set(EXTRA_COMPONENT_DIRS "${WASM}/src/wamr/build-scripts/esp-idf")
elseif(BACKEND STREQUAL "wamraot")   # AOT: WAMR 2.4.5, matching wamrc 2.4.5 (AOT format 5)
    set(EXTRA_COMPONENT_DIRS "${WASM}/src/wamr-2.4.5/build-scripts/esp-idf")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wasmprobe_${BACKEND})
