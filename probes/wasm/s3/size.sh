#!/bin/bash
# SPDX-License-Identifier: MIT
. "$IDF_PATH/export.sh" > /dev/null 2>&1
idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" size-components --format csv 2>/dev/null
idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" size --format csv 2>/dev/null
