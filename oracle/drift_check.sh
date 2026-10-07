#!/bin/bash
# SPDX-License-Identifier: MIT
# The drift test (tools/drift.c): the app at its windows against the app holding everything, under
# a struggling player's repeated falls, across skills and walked-to rows. Every tick must print the
# same, and the windows' out-of-window counts must stay 0.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/drift
CFLAGS="-O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off"
FW=$ROOT/build/firmware/firmware
SRC=("$ROOT/tools/drift.c" "$ROOT"/app/*.c "$FW/font5x7.c" "$ROOT/oracle/out.c" "$ROOT/oracle/game.c" "$ROOT"/babal/*.c)
INC=(-I"$ROOT/app" -I"$ROOT/babal" -I"$ROOT/oracle" -I"$FW")
rm -rf "$B" && mkdir -p "$B"
# shellcheck disable=SC2086
cc $CFLAGS "${INC[@]}" "${SRC[@]}" -lm -o "$B/drift" || exit 1
# shellcheck disable=SC2086
cc $CFLAGS -DBABAL_ROWS=10000 -DBABAL_LOG=16384 "${INC[@]}" "${SRC[@]}" -lm -o "$B/drift-all" || exit 1
ok=1
for skill in 2 3 4; do
    for target in 400 600 2000; do
        n="$skill-$target"
        "$B/drift" "$skill" "$target" 120 > "$B/$n.app" 2> "$B/$n.err"; rc=$?
        "$B/drift-all" "$skill" "$target" 120 > "$B/$n.all" 2> "$B/$n.all.err"
        if [ "$rc" != 0 ] || ! cmp -s "$B/$n.app" "$B/$n.all"; then
            ok=0
            echo "RED $n: $(cat "$B/$n.err")$(cmp -s "$B/$n.app" "$B/$n.all" || echo "; differs from the unbounded build at line $(cmp "$B/$n.app" "$B/$n.all" | awk '{print $NF}')")"
        else
            echo "  $n: identical; $(cat "$B/$n.err")"
        fi
    done
done
# The control: the windows stage 2 first shipped (512 rows, 256 log rows) must fail this test.
# shellcheck disable=SC2086
cc $CFLAGS -DBABAL_ROWS=512 -DBABAL_LOG=256 "${INC[@]}" "${SRC[@]}" -lm -o "$B/drift-small" || exit 1
red=0
for skill in 2 3 4; do
    for target in 400 600 2000; do
        "$B/drift-small" "$skill" "$target" 120 > "$B/small.out" 2>/dev/null || red=$((red + 1))
    done
done
if [ "$red" -gt 0 ]; then echo "red: the old 512/256 windows fail $red of 9"; else echo "CONTROL SURVIVED: the old windows pass"; ok=0; fi
[ "$ok" = 1 ] && { echo "DRIFT GREEN"; exit 0; }
echo "DRIFT RED"; exit 1
