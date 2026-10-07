#!/usr/bin/env bash
# Builds, flashes and runs Babal's app bench on the board soroban lends (claim it in both lanes
# first). Exit 0 on DEVICE PASS: every app script's frames on the chip hash as the host's, and the
# control does not. Prints tick and draw times and row generation's.
#   scripts/device_app.sh [FLASH_PORT] [CONSOLE_PORT]
set -euo pipefail
FLASH="${1:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_34:85:18:76:CE:8C-if00}"
CONSOLE="${2:-/dev/serial/by-id/usb-1a86_USB_Single_Serial_56E1012198-if00}"
IMAGE="espressif/idf:v5.4.2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -s build/apphost
rm -rf build/device-app && mkdir -p build/device-app
python3 oracle/gen_appscripts.py build/device-app/scripts 8
python3 scripts/device_app.py build/device-app build/apphost
F="$(readlink -f "$FLASH")"
C="$(readlink -f "$CONSOLE")"
docker run --rm -v "$ROOT:/project" -w /project \
    --user "$(id -u):$(id -g)" --group-add "$(stat -c %g "$F")" --device "$F" --device "$C" \
    -e HOME=/tmp -e F="$F" -e C="$C" "$IMAGE" bash -c '
set -euo pipefail
. "$IDF_PATH/export.sh" >/dev/null 2>&1
cd device/appbench
B=/project/build/device-app/idf
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 build
idf.py -B "$B" -DSDKCONFIG="$B/sdkconfig" -DIDF_TARGET=esp32s3 -p "$F" flash
python /project/scripts/capture.py "$C" /project/build/device-app/s3.log 1800
'
python3 scripts/device_app_compare.py build/device-app/expected.txt build/device-app/s3.log.report
