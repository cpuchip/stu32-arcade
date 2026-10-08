#!/bin/bash
# SPDX-License-Identifier: MIT
# build.sh VARIANT BUILDDIR DEFAULTS: one probe image in the IDF image (run inside the container)
. "$IDF_PATH/export.sh" > /dev/null
idf.py -B "$2" -DSDKCONFIG="$2/sdkconfig" -DSDKCONFIG_DEFAULTS="$3" build
