#!/bin/bash
# SPDX-License-Identifier: MIT
# The framework's drawing (firmware/draw.{h,c} at FIRMWARE_PIN, the games SDK's since apps.h v1.2)
# against the framework's own tests/draw_test.c, under ASan and UBSan and the frame guard: a check
# that the pinned draw is the one games here build with. Its plants are the framework's gate's.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/sdkcheck
FW=$ROOT/build/firmware/firmware
CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=undefined"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wframe-larger-than=1024 -I"$FW" -c "$FW/draw.c" -o /dev/null \
    || { echo "RED: draw.c fails the frame guard"; ok=0; }
# shellcheck disable=SC2086
cc $CFLAGS -I"$FW" "$ROOT/build/firmware/tests/draw_test.c" "$FW/draw.c" "$FW/font5x7.c" -lm -o "$B/draw_test" || exit 1
if "$B/draw_test" 20000 > "$B/out" 2>&1; then echo "  framework draw at $(cut -c1-7 "$ROOT/FIRMWARE_PIN"): $(tail -1 "$B/out")"; else echo "RED: $(tail -3 "$B/out")"; ok=0; fi
# The template game (sdk/template): its own sources under the frame guard (alone: the runner is a
# host tool), then its demo through the generic runner under ASan and UBSan, to the game-over screen
# and a save that starts with its magic.
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wframe-larger-than=1024 -I"$FW" -c "$ROOT/sdk/template/ball.c" -o /dev/null \
    || { echo "RED: ball.c fails the frame guard"; ok=0; }
# shellcheck disable=SC2086
if cc $CFLAGS -DAPP_DEF=ball_app -I"$FW" "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/template/ball.c" "$FW/draw.c" \
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
