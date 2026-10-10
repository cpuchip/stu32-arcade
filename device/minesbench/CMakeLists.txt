# SPDX-License-Identifier: MIT
# MINES on the ESP32-S3 (docs/mines.md): oracle/mines's app scripts played through apps.h's calls, every frame hashed
# against the SDK runner's (oracle/mines/frames), key, tick and draw timed apart, and dealing timed alone (the worst seed found, and 100 more).
# Built and run by scripts/device_mines.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mines_bench)
