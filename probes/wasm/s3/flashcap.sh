#!/bin/bash
# SPDX-License-Identifier: MIT
# flashcap.sh BUILDDIR PORT: flash one probe image and read its console (run inside the container)
. "$IDF_PATH/export.sh" > /dev/null
idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" -p "$2" flash > /dev/null 2> "$1.flash.err" || exit 3
timeout 240 python "$(dirname "$0")/cap.py" "$2" 220
