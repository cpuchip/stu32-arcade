#!/bin/bash
# SPDX-License-Identifier: MIT
# Babal as a WebAssembly game (decision 67: one .wasm a game, an .aot a target from our pinned wamrc): builds
# wasm/babal/babal.wasm with the guest shim (sdk/wasm/guest.c) and wasm/babal/babal.aot for the ESP32-S3, and
# writes their sha256s and the toolchain into wasm/babal/stu32-game.txt. With --check it builds into a scratch
# directory and compares with what is committed: an .aot from anyone else's wamrc fails here.
# Env: WASI_SDK (wasi-sdk 34), WAMRC (wamrc from the pinned WAMR), WAMR_TREE_HASH (that WAMR's tree,
# stu32-firmware third_party/wamr/PIN), LLVM_COMMIT (the LLVM wamrc was built on). None is in this repository.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${WASI_SDK:?set WASI_SDK}" "${WAMRC:?set WAMRC}" "${WAMR_TREE_HASH:?set WAMR_TREE_HASH}" "${LLVM_COMMIT:?set LLVM_COMMIT}"
CHECK=0
[ "${1:-}" = --check ] && CHECK=1
OUT=$ROOT/wasm/babal
[ "$CHECK" = 1 ] && OUT=$(mktemp -d)
make -s -C "$ROOT" build/firmware/.pin
mkdir -p "$OUT"
cd "$ROOT" # relative paths, so nothing of this checkout's location reaches the module
"$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -ffp-contract=off \
    -Wall -Wextra -Werror -Wno-missing-field-initializers -DAPP_DEF=babal_app -Igames/babal/app -Igames/babal/core -Ibuild/firmware/firmware \
    -Wl,--allow-undefined -Wl,-z,stack-size=65536 -o "$OUT/babal.wasm" sdk/wasm/guest.c games/babal/app/babal_app.c games/babal/app/render.c \
    build/firmware/firmware/font5x7.c games/babal/core/*.c -lm
AOT_FLAGS="--target=xtensa --cpu=esp32s3"
# shellcheck disable=SC2086
"$WAMRC" $AOT_FLAGS -o "$OUT/babal.aot" "$OUT/babal.wasm" > /dev/null
WASM_SUM=$(sha256sum "$OUT/babal.wasm" | cut -c1-64)
AOT_SUM=$(sha256sum "$OUT/babal.aot" | cut -c1-64)
if [ "$CHECK" = 1 ]; then
    m=$ROOT/wasm/babal/stu32-game.txt
    ok=1
    grep -q "^wasm_sha256 = $WASM_SUM$" "$m" || { echo "babal.wasm rebuilds as $WASM_SUM, not as committed"; ok=0; }
    grep -q "^aot_sha256 = $AOT_SUM$" "$m" || { echo "babal.aot rebuilds as $AOT_SUM, not as committed"; ok=0; }
    cmp -s "$OUT/babal.aot" "$ROOT/wasm/babal/babal.aot" || { echo "babal.aot differs from the committed file"; ok=0; }
    rm -rf "$OUT"
    [ "$ok" = 1 ] && { echo "wasm/babal reproduces: .wasm $WASM_SUM, .aot $AOT_SUM"; exit 0; }
    exit 1
fi
cat > "$OUT/stu32-game.txt" <<EOF
# Babal as a WebAssembly game, for the firmware's STU_WASM (made by scripts/build_wasm_game.sh; do not edit)
name = BABLW
folder = GAMES
kind = wasm
aot = wasm/babal/babal.aot
aot_sha256 = $AOT_SUM
wasm = wasm/babal/babal.wasm
wasm_sha256 = $WASM_SUM
author = arcade (the games seat), porting Christian Mauduit's Babal
licence = MIT
original = Babal by Christian Mauduit, gitlab.com/liberecofr/babal (public domain, the Unlicense)
source = games/babal/app/ and games/babal/core/ through sdk/wasm/guest.c, built by scripts/build_wasm_game.sh
toolchain = $(cat "$WASI_SDK/VERSION" | head -1 | tr -d '\n') clang -mcpu=mvp -O2; wamrc $AOT_FLAGS, WAMR tree $WAMR_TREE_HASH, LLVM $LLVM_COMMIT
EOF
echo "wasm/babal: .wasm $WASM_SUM, .aot $AOT_SUM"
