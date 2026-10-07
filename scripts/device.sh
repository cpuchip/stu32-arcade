#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds, flashes and runs Babal's S3 bench on the board soroban lends (claim it in both lanes
# first). Flash through its native USB, read the console on its CH343, both by their by-id paths
# (the ttyACM numbers move). Exit 0 on DEVICE PASS: every game's output on the chip hashes as the
# host's (which the gate shows equals babalcore's), and the control does not.
#   scripts/device.sh [FLASH_PORT] [CONSOLE_PORT]
set -euo pipefail
FLASH="${1:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_34:85:18:76:CE:8C-if00}"
CONSOLE="${2:-/dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -s all
rm -rf build/device/scripts && mkdir -p build/device
python3 oracle/gen_scripts.py build/device/scripts 8 5
python3 scripts/device_scripts.py build/device build/driver
F="$(readlink -f "$FLASH")"
C="$(readlink -f "$CONSOLE")"
docker run --rm -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$F")" --device "$F" --device "$C" \
    -e HOME=/tmp -e F="$F" -e C="$C" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null
cd device/bench
B=/project/build/device/idf
[ -f "$B/sdkconfig" ] && [ sdkconfig.defaults -nt "$B/sdkconfig" ] && rm -f "$B/sdkconfig"
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 size | tail -4
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 -p "$F" flash
python /project/scripts/capture.py "$C" /project/build/device/s3.log 2400
'
python3 scripts/device_compare.py build/device/expected.txt build/device/s3.log.report
