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
# The runner's apps.h v1.2 (tests/v12_app.c): a seed fixed by -seed comes back in the save; the save
# is save_size bytes (100) with that cap, and comes back whole on -load; a 20-note tune keeps 16; a
# save_size past APP_SAVE_MAX is refused before the game starts.
printf 'tick 10\n' > "$B/v12.txt"
# shellcheck disable=SC2086
if cc $CFLAGS -DAPP_DEF=v12_app -I"$FW" "$ROOT/sdk/runner/apprun.c" "$ROOT/tests/v12_app.c" -o "$B/v12" \
    && "$B/v12" "$B/v12.txt" -seed 0x12345678 -save "$B/v12.a" -v > "$B/v12.out" 2>&1 \
    && "$B/v12" "$B/v12.txt" -seed 7 -load "$B/v12.a" -save "$B/v12.b" > /dev/null 2>&1 \
    && grep -q "tune of 20 notes .* (4 dropped)$" "$B/v12.out" \
    && python3 - "$B/v12.a" "$B/v12.b" <<'PY'
import sys
a, b = (open(p, "rb").read() for p in sys.argv[1:])
assert len(a) == 100 and len(b) == 100, (len(a), len(b))
assert a[:5] == b"V\x78\x56\x34\x12" and a[5:6] == b"-" and a[6:10] == (100).to_bytes(4, "little") and a[99:] == b"Z", a[:12]
assert b[:6] == b"V\x07\x00\x00\x00L", b[:12]
PY
then echo "  runner v1.2: seed fixed, a 100-byte save out and back whole, a 20-note tune cut to 16"
else echo "RED: the runner's v1.2 calls: $(tail -2 "$B/v12.out" 2>/dev/null)"; ok=0; fi
# shellcheck disable=SC2086
cc $CFLAGS -DAPP_DEF=v12_app -DV12_SAVE_SIZE=5000 -I"$FW" "$ROOT/sdk/runner/apprun.c" "$ROOT/tests/v12_app.c" -o "$B/v12big" \
    && { "$B/v12big" "$B/v12.txt" > "$B/v12big.out" 2>&1; [ $? = 3 ]; } && grep -q "save_size 5000 (max 4096)" "$B/v12big.out" \
    && echo "  runner v1.2: a save_size of 5000 refused" || { echo "RED: a save_size of 5000 was not refused"; ok=0; }
[ "$ok" = 1 ] && { echo "SDK GATE GREEN"; exit 0; }
echo "SDK GATE RED"; exit 1
