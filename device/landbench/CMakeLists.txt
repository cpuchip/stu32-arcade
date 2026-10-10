# SPDX-License-Identifier: MIT
# LAND on the ESP32-S3 (docs/land.md): oracle/land's app scripts played through apps.h's calls, every frame hashed
# against the SDK runner's (oracle/land/frames), key, tick and draw timed apart.
# Built and run by scripts/device_land.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(land_bench)
