#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds, flashes and runs MINES' bench (device/minesbench) on the board soroban lends (claim it first). Exit 0 on
# DEVICE PASS: both app scripts' frames on the chip hash as the SDK runner's (oracle/mines/frames), and the control
# does not, and every deal's attempts are the model's. Prints key, tick, draw and dealing times.
#   scripts/device_mines.sh [FLASH_PORT] [CONSOLE_PORT]
set -euo pipefail
FLASH="${1:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_34:85:18:76:CE:8C-if00}"
CONSOLE="${2:-/dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -s build/firmware/.pin
rm -rf build/device-mines && mkdir -p build/device-mines
python3 scripts/device_mines.py pack build/device-mines
F="$(readlink -f "$FLASH")"
C="$(readlink -f "$CONSOLE")"
timeout 1200 docker run --rm --name arcade-bench-mines --memory 4g -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$F")" --device "$F" $([ "$C" = "$F" ] || echo --device "$C") \
    -e HOME=/tmp -e F="$F" -e C="$C" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null 2>&1
cd device/minesbench
B=/project/build/device-mines/idf
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 -p "$F" flash
python /project/scripts/capture.py "$C" /project/build/device-mines/s3.log 600
'
python3 scripts/device_mines.py compare build/device-mines
