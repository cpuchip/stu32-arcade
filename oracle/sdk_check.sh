#!/bin/bash
# SPDX-License-Identifier: MIT
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
# The template game (sdk/template): its own sources under the frame guard (alone: the runner is a
# host tool), then its demo through the generic runner under ASan and UBSan, to the game-over screen
# and a save that starts with its magic.
for f in "$ROOT/sdk/template/ball.c" "$ROOT/sdk/draw.c"; do
    cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wframe-larger-than=1024 -I"$ROOT/sdk" -I"$FW" -c "$f" -o /dev/null \
        || { echo "RED: $(basename "$f") fails the frame guard"; ok=0; }
done
# shellcheck disable=SC2086
if cc $CFLAGS -DAPP_DEF=ball_app -I"$ROOT/sdk" -I"$FW" "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/template/ball.c" "$ROOT/sdk/draw.c" \
    "$FW/font5x7.c" -o "$B/apprun-ball" && "$B/apprun-ball" "$ROOT/sdk/template/demo.txt" -save "$B/ball.save" > "$B/ball.out" 2>&1 \
    && [ "$(head -c 1 "$B/ball.save")" = T ]; then echo "  template: $(tail -1 "$B/ball.out")"
else echo "RED: the template's demo: $(tail -2 "$B/ball.out")"; ok=0; fi
# The generic runner against tools/apphost.c, two harnesses one app: Babal's app-0, frame for frame.
APP0=$ROOT/build/appcheck/scripts/app-0.txt
[ -f "$APP0" ] || python3 "$ROOT/oracle/gen_appscripts.py" "$ROOT/build/appcheck/scripts" 8 > /dev/null
# shellcheck disable=SC2086
if cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -DAPP_DEF=babal_app -I"$ROOT/app" -I"$ROOT/babal" -I"$FW" \
        "$ROOT/sdk/runner/apprun.c" "$ROOT"/app/babal_app.c "$ROOT"/app/render.c "$FW/font5x7.c" "$ROOT"/babal/*.c -lm -o "$B/apprun-babal" \
    && cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -I"$ROOT/app" -I"$ROOT/babal" -I"$ROOT/oracle" -I"$FW" \
        "$ROOT/tools/apphost.c" "$ROOT/tools/render_ref.c" "$ROOT"/app/*.c "$FW/font5x7.c" "$ROOT/oracle/out.c" "$ROOT/oracle/game.c" "$ROOT"/babal/*.c -lm -o "$B/apphost" \
    && "$B/apprun-babal" "$APP0" -hashes "$B/run.hashes" > /dev/null && "$B/apphost" "$APP0" "$B/host" "$B/host.frames" > /dev/null 2>&1 \
    && cmp -s "$B/run.hashes" "$B/host.frames"; then echo "  runner: apprun and apphost agree on all $(wc -l < "$B/run.hashes") frames of Babal's app-0"
else echo "RED: apprun and apphost disagree on Babal's app-0"; ok=0; fi
[ "$ok" = 1 ] && { echo "SDK GATE GREEN"; exit 0; }
echo "SDK GATE RED"; exit 1
