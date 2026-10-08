#!/bin/bash
# SPDX-License-Identifier: MIT
. "$IDF_PATH/export.sh" > /dev/null 2>&1
xtensa-esp32s3-elf-addr2line -pfiaC -e "$1" "${@:2}"
