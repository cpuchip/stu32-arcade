# SPDX-License-Identifier: MIT
# 3720, MOLES and CATCH on the ESP32-S3 (docs/3720.md, docs/moles.md, docs/catch.md): each game's app scripts from
# oracle/<game>/appscript.py played through apps.h's calls, every frame (key frames too) hashed against the SDK
# runner's (oracle/<game>/frames), key, tick and draw timed apart. Built and run by scripts/device_kids.sh in
# Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kids_bench)
