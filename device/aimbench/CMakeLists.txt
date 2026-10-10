# SPDX-License-Identifier: MIT
# AIM on the ESP32-S3 (docs/aim.md): oracle/aim's app scripts played through apps.h's calls, every frame hashed
# against the SDK runner's (oracle/aim/frames), key, tick and draw timed apart, and the round generator timed alone.
# Built and run by scripts/device_aim.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(aim_bench)
