# SPDX-License-Identifier: MIT
# SNAKE on the ESP32-S3 (docs/snake.md): oracle/snake's app scripts played through apps.h's calls, every frame hashed
# against the SDK runner's (oracle/snake/frames), key, tick and draw timed apart.
# Built and run by scripts/device_snake.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(snake_bench)
