# SPDX-License-Identifier: MIT
# The minimal repro's ESP-IDF loader (arcade): WAMR from $WAMR_DIR, one embedded .aot ($MOD), add(2, 3)
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "$ENV{WAMR_DIR}/build-scripts/esp-idf")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wamr_add)
