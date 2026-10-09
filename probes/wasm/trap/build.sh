#!/bin/bash
# SPDX-License-Identifier: MIT
# The trap test game (trap_app.c) through the guest shim: trap.wasm, trap.aot for the ESP32-S3 and stu32-game.txt,
# into OUT (default build/trap). For scratch firmware images only; never listed.
# Env: WASI_SDK, WAMRC, as scripts/build_wasm_game.sh.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
: "${WASI_SDK:?set WASI_SDK}" "${WAMRC:?set WAMRC}"
OUT=${1:-$ROOT/build/trap}
make -s -C "$ROOT" build/firmware/.pin
mkdir -p "$OUT/w"
cd "$ROOT"
"$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
    -Wno-missing-field-initializers -DAPP_DEF=trap_app -Iapp -Ibuild/firmware/firmware -Wl,--allow-undefined \
    -Wl,-z,stack-size=65536 -o "$OUT/w/trap.wasm" sdk/wasm/guest.c probes/wasm/trap/trap_app.c
"$WAMRC" --target=xtensa --cpu=esp32s3 -o "$OUT/w/trap.aot" "$OUT/w/trap.wasm" > /dev/null
cat > "$OUT/stu32-game.txt" <<M
name = TRAP
folder = GAMES
kind = wasm
aot = w/trap.aot
aot_sha256 = $(sha256sum "$OUT/w/trap.aot" | cut -c1-64)
wasm = w/trap.wasm
wasm_sha256 = $(sha256sum "$OUT/w/trap.wasm" | cut -c1-64)
toolchain = probes/wasm/trap/build.sh
author = arcade
licence = MIT
source = probes/wasm/trap/trap_app.c
M
echo "trap: $(stat -c %s "$OUT/w/trap.aot") B .aot in $OUT"
