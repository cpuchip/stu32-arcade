#!/bin/bash
# The app against babalcore, end to end: tools/apphost.c plays seeded app scripts through apps.h's
# calls and writes the same game as an oracle/ref script; every tick's readings and private state
# must equal babalcore's (less the level's row counts: the app makes rows a few at a time, babalgame
# 100 at a time, the same rows either way). Then plants in the app, each of which must go red.
# Env: N (app scripts, 8). Needs the reference built (make ref) and FIRMWARE_REPO for the font.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
REF=$ROOT/oracle/ref/target/release/babal-ref
B=$ROOT/build/appcheck
N=${N:-8}
CFLAGS="-O2 -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -ffp-contract=off"
FW=$ROOT/build/firmware/firmware

rm -rf "$B" && mkdir -p "$B"
python3 "$ROOT/oracle/gen_appscripts.py" "$B/scripts" "$N" || exit 1

# check HOST: 0 when every script's ticks equal babalcore's.
check() {
    local host=$1 stop=${2:-} bad=0
    for s in "$B"/scripts/*.txt; do
        n=$(basename "$s" .txt)
        local rc=0
        timeout 120 "$host" "$s" "$B/$n.oracle" "$B/$n.frames" > "$B/$n.app" 2> "$B/$n.err" || rc=$?
        if [ "$rc" != 0 ]; then
            [ "$rc" = 124 ] && echo "apphost: timed out (hung)" >> "$B/$n.err"
            bad=$((bad + 1)); [ -n "$stop" ] && { echo "  red at $n (apphost failed: $(tail -1 "$B/$n.err"))"; return 1; }
            echo "  APPHOST FAILED $n: $(tail -1 "$B/$n.err")"; continue
        fi
        timeout 300 "$REF" game "$B/$n.oracle" | grep -E '^(t|s) ' | sed -E 's/ first=[0-9-]+ last=[0-9-]+ len=[0-9]+$//' > "$B/$n.ref"
        if ! cmp -s "$B/$n.ref" "$B/$n.app"; then
            bad=$((bad + 1)); [ -n "$stop" ] && { echo "  red at $n"; return 1; }
            echo "  DIFF $n"
        elif [ -z "$stop" ]; then
            echo "  $n: $(grep -c '^t ' "$B/$n.app") ticks identical; $(tail -1 "$B/$n.err" | sed 's/^apphost: //')"
        fi
    done
    return $((bad > 0))
}

ok=1
echo "== the app against babalcore"
check "$ROOT/build/apphost" || ok=0

PLANTS=(
    "babal_app.c|s/bb_input_steer_left_key(\&c->steer, 1, 1.0, down, t);/bb_input_steer_right_key(\&c->steer, 1, 1.0, down, t);/|4 steers right"
    "babal_app.h|s/#define BABAL_STEP_MAX_MS 100/#define BABAL_STEP_MAX_MS 50/|the step clamped at 50 ms"
    "babal_app.c|s/bb_instant t = (bb_instant)ms \* 1000000;/bb_instant t = (bb_instant)ms * 1000;/|key times in microseconds"
    "babal_app.c|s/BABAL_KEY_ENTER) \&\& down)/BABAL_KEY_ENTER) \&\& !down)/|a jump on the release"
    "babal_app.c|s/adapter a = {c, (bb_instant)c->now_ms \* 1000000};/adapter a = {c, (bb_instant)(c->now_ms + 1) * 1000000};/|the tick's clock a millisecond late"
    "babal_app.c|/^static void generate_ahead/,/^}/s/bb_level_generate(\&c->level, 1);/(void)0;/|no rows made ahead"
)
echo "== plants in the app (each must go red)"
for p in "${PLANTS[@]}"; do
    IFS='|' read -r file expr what <<< "$p"
    P=$B/plant
    rm -rf "$P" && mkdir -p "$P" && cp -r "$ROOT/app" "$P/app"
    sed -i "$expr" "$P/app/$file"
    if cmp -s "$P/app/$file" "$ROOT/app/$file"; then echo "PLANT DID NOT APPLY: $what"; ok=0; continue; fi
    # shellcheck disable=SC2086
    if ! cc $CFLAGS -I"$P/app" -I"$ROOT/babal" -I"$ROOT/oracle" -I"$FW" "$ROOT/tools/apphost.c" "$P"/app/*.c \
        "$FW/font5x7.c" "$ROOT/oracle/out.c" "$ROOT/oracle/game.c" "$ROOT"/babal/*.c -lm -o "$P/apphost" 2> "$P/cc.err"; then
        echo "PLANT DID NOT BUILD: $what"; head -3 "$P/cc.err"; ok=0; continue
    fi
    if check "$P/apphost" stop > "$P/result"; then echo "SURVIVED: $what"; ok=0; else echo "red: $what ($(sed 's/^  //' "$P/result"))"; fi
done
[ "$ok" = 1 ] && { echo "APP GATE GREEN"; exit 0; }
echo "APP GATE RED"; exit 1
