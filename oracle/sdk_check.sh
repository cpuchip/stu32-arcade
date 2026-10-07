#!/bin/bash
# sdk/draw against tests/draw_test.c (a pixel-by-pixel reference and line properties), under ASan
# and UBSan and the frame guard, then plants in sdk/draw.c, each of which must turn the test red.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/sdkcheck
FW=$ROOT/build/firmware/firmware
CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=undefined"
rm -rf "$B" && mkdir -p "$B"
ok=1
# shellcheck disable=SC2086
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wframe-larger-than=1024 -I"$ROOT/sdk" -I"$FW" -c "$ROOT/sdk/draw.c" -o /dev/null \
    || { echo "RED: sdk/draw.c fails the frame guard"; ok=0; }
build() { # build DRAW_C OUT
    # shellcheck disable=SC2086
    cc $CFLAGS -I"$ROOT/sdk" -I"$FW" "$ROOT/tests/draw_test.c" "$1" "$FW/font5x7.c" -lm -o "$2"
}
build "$ROOT/sdk/draw.c" "$B/draw_test" || exit 1
if "$B/draw_test" 20000 > "$B/out" 2>&1; then echo "  $(tail -1 "$B/out")"; else echo "RED: $(tail -3 "$B/out")"; ok=0; fi
PLANTS=(
    "s/uint8_t first = (uint8_t)(0xff << (x0 \& 7)), last = (uint8_t)(0xff >> (7 - ((x1 - 1) \& 7)));/uint8_t first = (uint8_t)(0xff << (x0 \& 7)), last = (uint8_t)(0xff >> (8 - ((x1 - 1) \& 7)));/|a fill's last byte one pixel short"
    "s/    if (x1 > LS027_W)/    if (x1 > LS027_W + 8)/|fills not clipped at the right edge"
    "s/    int stride = (w + 7) \/ 8;/    int stride = w \/ 8 + (w > 8);/|a blit's row stride"
    "s/        x += 6 \* scale;/        x += 5 * scale;/|text set 5 apart"
    "s/((2 \* k \* minor + major) \/ (2 \* major))/((2 * k * minor) \/ (2 * major))/|a line rounding down"
    "/    if (khi > major)/{n;s/        khi = major;/        khi = major - 1;/}|a line missing its last pixel"
    "s/: (uint8_t)(\*b \& ~m); break;/: *b; break;/|SET leaving the sprite's clear bits"
    "s/(uint8_t)((r \& 1) ? shade >> 8 : shade \& 0xff)/(uint8_t)((r \& 1) ? shade \& 0xff : shade >> 8)/|a shade's rows swapped"
)
for p in "${PLANTS[@]}"; do
    IFS='|' read -r expr what <<< "$p"
    cp "$ROOT/sdk/draw.c" "$B/plant.c"
    sed -i "$expr" "$B/plant.c"
    if cmp -s "$B/plant.c" "$ROOT/sdk/draw.c"; then echo "PLANT DID NOT APPLY: $what"; ok=0; continue; fi
    if ! build "$B/plant.c" "$B/plant" 2> "$B/cc.err"; then echo "PLANT DID NOT BUILD: $what"; ok=0; continue; fi
    if timeout 120 "$B/plant" 3000 > "$B/plant.out" 2>&1; then echo "SURVIVED: $what"; ok=0
    else echo "red: $what ($(grep -m1 -E 'FAIL|ERROR|runtime error' "$B/plant.out" | cut -c1-90))"; fi
done
[ "$ok" = 1 ] && { echo "SDK GATE GREEN"; exit 0; }
echo "SDK GATE RED"; exit 1
