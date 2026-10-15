#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds, flashes and runs SALVO's bench (device/salvobench) on the board soroban lends (claim it first). Exit 0 on
# DEVICE PASS: both app scripts' frames on the chip hash as the SDK runner's (oracle/salvo/frames), and the control
# does not. Prints key, tick and draw times.
#   scripts/device_salvo.sh [FLASH_PORT] [CONSOLE_PORT]   (one port for both is fine: a board flashed over its UART)
set -euo pipefail
FLASH="${1:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_34:85:18:76:CE:8C-if00}"
CONSOLE="${2:-/dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -s build/firmware/.pin
rm -rf build/device-salvo && mkdir -p build/device-salvo
python3 scripts/device_salvo.py pack build/device-salvo
F="$(readlink -f "$FLASH")"
C="$(readlink -f "$CONSOLE")"
docker run --rm -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$F")" --device "$F" $([ "$C" = "$F" ] || echo --device "$C") \
    -e HOME=/tmp -e F="$F" -e C="$C" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null 2>&1
cd device/salvobench
B=/project/build/device-salvo/idf
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 -p "$F" flash
python /project/scripts/capture.py "$C" /project/build/device-salvo/s3.log 600
'
python3 scripts/device_salvo.py compare build/device-salvo
