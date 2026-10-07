# Babal's app on the ESP32-S3: the app scripts of oracle/app_check.sh played through apps.h's calls,
# tick and draw timed apart, every frame hashed against the host's. Built and run by
# scripts/device_app.sh in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main esp_psram)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(babal_appbench)
