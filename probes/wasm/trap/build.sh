#!/bin/bash
# SPDX-License-Identifier: MIT
# The trap test game (trap_app.c, soroban #5467's device gate) through the guest shim: wasm/trap/trap.wasm, its .aot
# for the ESP32-S3, and wasm/trap/stu32-game.txt, as scripts/build_wasm_game.sh makes Babal's. With --check it builds
# into a scratch directory and compares with what is committed. Listed only by the firmware's wasm gate, never by
# its games.list.
# Env: WASI_SDK, WAMRC, WAMR_TREE_HASH, LLVM_COMMIT, as scripts/build_wasm_game.sh.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
: "${WASI_SDK:?set WASI_SDK}" "${WAMRC:?set WAMRC}" "${WAMR_TREE_HASH:?set WAMR_TREE_HASH}" "${LLVM_COMMIT:?set LLVM_COMMIT}"
CHECK=0
[ "${1:-}" = --check ] && CHECK=1
OUT=$ROOT/wasm/trap
[ "$CHECK" = 1 ] && OUT=$(mktemp -d)
make -s -C "$ROOT" build/firmware/.pin
mkdir -p "$OUT"
cd "$ROOT" # relative paths, so nothing of this checkout's location reaches the module
"$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
    -Wno-missing-field-initializers -DAPP_DEF=trap_app -Igames/babal/app -Ibuild/firmware/firmware -Wl,--allow-undefined \
    -Wl,-z,stack-size=65536 -o "$OUT/trap.wasm" sdk/wasm/guest.c probes/wasm/trap/trap_app.c
AOT_FLAGS="--target=xtensa --cpu=esp32s3"
# shellcheck disable=SC2086
"$WAMRC" $AOT_FLAGS -o "$OUT/trap.aot" "$OUT/trap.wasm" > /dev/null
WASM_SUM=$(sha256sum "$OUT/trap.wasm" | cut -c1-64)
AOT_SUM=$(sha256sum "$OUT/trap.aot" | cut -c1-64)
if [ "$CHECK" = 1 ]; then
    m=$ROOT/wasm/trap/stu32-game.txt
    ok=1
    grep -q "^wasm_sha256 = $WASM_SUM$" "$m" || { echo "trap.wasm rebuilds as $WASM_SUM, not as committed"; ok=0; }
    grep -q "^aot_sha256 = $AOT_SUM$" "$m" || { echo "trap.aot rebuilds as $AOT_SUM, not as committed"; ok=0; }
    cmp -s "$OUT/trap.aot" "$ROOT/wasm/trap/trap.aot" || { echo "trap.aot differs from the committed file"; ok=0; }
    rm -rf "$OUT"
    [ "$ok" = 1 ] && { echo "wasm/trap reproduces: .wasm $WASM_SUM, .aot $AOT_SUM"; exit 0; }
    exit 1
fi
cat > "$OUT/stu32-game.txt" <<EOF
# The trap test game, for the firmware's wasm gate only (made by probes/wasm/trap/build.sh; do not edit)
name = TRAP
folder = GAMES
kind = wasm
aot = wasm/trap/trap.aot
aot_sha256 = $AOT_SUM
wasm = wasm/trap/trap.wasm
wasm_sha256 = $WASM_SUM
author = arcade (the games seat)
licence = MIT
source = probes/wasm/trap/trap_app.c through sdk/wasm/guest.c, built by probes/wasm/trap/build.sh
toolchain = $(head -1 "$WASI_SDK/VERSION" | tr -d '\n') clang -mcpu=mvp -O2; wamrc $AOT_FLAGS, WAMR tree $WAMR_TREE_HASH, LLVM $LLVM_COMMIT
EOF
echo "wasm/trap: .wasm $WASM_SUM, .aot $AOT_SUM"
