# SPDX-License-Identifier: MIT
# SALVO on the ESP32-S3 (docs/salvo.md): oracle/salvo's app scripts played through apps.h's calls, every frame (key
# frames too) hashed against the SDK runner's (oracle/salvo/frames), key, tick and draw timed apart.
# Built and run by scripts/device_salvo.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(salvo_bench)
