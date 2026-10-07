# Babal's rules on the ESP32-S3: every oracle game replayed on the chip in the device windows, its
# output hashed (it must equal the host's, which equals babalcore's) and the rules timed. Built and
# run through scripts/device.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(babal_bench)
