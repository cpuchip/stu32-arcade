#!/bin/bash
# SPDX-License-Identifier: MIT
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
CFLAGS="-O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off"
FW=$ROOT/build/firmware/firmware

# the host under test is built from the sources here, never a stale build/apphost (run directly, the
# script once checked an app older than the one in the tree, and only the sanitizer pass saw it)
make -s -C "$ROOT" build/apphost || exit 1
rm -rf "$B" && mkdir -p "$B"
python3 "$ROOT/oracle/gen_appscripts.py" "$B/scripts" "$N" || exit 1

# check HOST OUT [stop]: 0 when every script's ticks equal babalcore's; outputs go to OUT.
check() {
    local host=$1 O=$2 stop=${3:-} bad=0
    mkdir -p "$O"
    for s in "$B"/scripts/*.txt; do
        n=$(basename "$s" .txt)
        local rc=0
        rm -f "$O/$n".g*.oracle
        timeout 120 "$host" "$s" "$O/$n" "$O/$n.frames" > "$O/$n.app" 2> "$O/$n.err" || rc=$?
        if [ "$rc" != 0 ]; then
            [ "$rc" = 124 ] && echo "apphost: timed out (hung)" >> "$O/$n.err"
            bad=$((bad + 1)); [ -n "$stop" ] && { echo "  red at $n (apphost failed: $(tail -1 "$O/$n.err"))"; return 1; }
            echo "  APPHOST FAILED $n: $(tail -1 "$O/$n.err")"; continue
        fi
        : > "$O/$n.ref"
        for g in $(ls "$O/$n".g*.oracle 2>/dev/null | sed -E 's/.*\.g([0-9]+)\.oracle$/\1/' | sort -n); do
            echo "game $g" >> "$O/$n.ref"
            timeout 300 "$REF" game "$O/$n.g$g.oracle" | grep -E '^(t|s) ' | sed -E 's/ first=[0-9-]+ last=[0-9-]+ len=[0-9]+$//' >> "$O/$n.ref"
        done
        if ! cmp -s "$O/$n.ref" "$O/$n.app"; then
            bad=$((bad + 1)); [ -n "$stop" ] && { echo "  red at $n"; return 1; }
            echo "  DIFF $n"
        elif [ -z "$stop" ]; then
            echo "  $n: $(grep -c '^t ' "$O/$n.app") ticks identical to babalcore; $(tail -1 "$O/$n.err" | sed 's/^apphost: //')"
        fi
    done
    return $((bad > 0))
}

ok=1
echo "== the ctx limit: $(grep -m1 -o 'define APP_CTX_MAX ([0-9 *]*)' "$FW/apps.h") (apps.h at FIRMWARE_PIN)"
echo "== the app against babalcore"
check "$ROOT/build/apphost" "$B/run" || ok=0

# The same harness under ASan and UBSan (no recovery): undefined behaviour fails the gate.
echo "== under ASan and UBSan"
# shellcheck disable=SC2086
if cc $CFLAGS -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=undefined,float-cast-overflow -I"$ROOT/app" -I"$ROOT/babal" -I"$ROOT/oracle" -I"$FW" \
    "$ROOT/tools/apphost.c" "$ROOT/tools/render_ref.c" "$ROOT"/app/*.c "$FW/font5x7.c" "$ROOT/oracle/out.c" "$ROOT/oracle/game.c" \
    "$ROOT"/babal/*.c -lm -o "$B/apphost-san" 2> "$B/san-cc.err"; then
    for s in "$B"/scripts/*.txt; do
        if ! timeout 300 "$B/apphost-san" "$s" "$B/san" "$B/san.frames" > /dev/null 2> "$B/san.err"; then
            echo "  SANITIZER: $(basename "$s"): $(grep -m1 -E 'runtime error|ERROR' "$B/san.err" || tail -1 "$B/san.err")"; ok=0
        fi
    done
    [ "$ok" = 1 ] && echo "  clean on every script"
else
    echo "  sanitized build failed: $(head -2 "$B/san-cc.err")"; ok=0
fi

PLANTS=(
    "babal_app.c|s/bb_input_steer_left_key(\&c->steer, 1, 1.0, down, t);/bb_input_steer_right_key(\&c->steer, 1, 1.0, down, t);/|4 steers right"
    "babal_app.h|s/#define BABAL_STEP_MAX_MS 100/#define BABAL_STEP_MAX_MS 50/|the step clamped at 50 ms"
    "babal_app.c|s/bb_instant t = (bb_instant)ms \* 1000000;/bb_instant t = (bb_instant)ms * 1000;/|key times in microseconds"
    "babal_app.c|s/BABAL_KEY_ENTER) \&\& down)/BABAL_KEY_ENTER) \&\& !down)/|a jump on the release"
    "babal_app.c|s/adapter a = {c, (bb_instant)c->now_ms \* 1000000};/adapter a = {c, (bb_instant)(c->now_ms + 1) * 1000000};/|the tick's clock a millisecond late"
    "babal_app.c|/^static void generate_ahead/,/^}/s/bb_level_generate(\&c->level, 1);/(void)0;/|no rows made ahead"
    # From the stage-2 review (2026-10-07): the two defects put back, and the seven plants that survived
    "babal_app.c|/    bb_player_set_best_score(\&c->player, best_score(c));/d|the saved best not given to the rules"
    "babal_app.c|s/save\[2\] > BB_EXPERT)/0)/|a save with a bad skill byte taken"
    "babal_app.c|s/            c->skill = (bb_skill)(BB_NOOB + (key - APP_KEY_SOFT1));/            c->skill = (bb_skill)(BB_EXPERT - (key - APP_KEY_SOFT1));/|soft keys choose skills in reverse"
    "babal_app.c|s/c->seed = mix64(((uint64_t)c->games << 32) ^ ms);/c->seed = mix64(42 + 0 * ms);/|every game the same seed"
    "babal_app.c|/        if (e->kind == BB_EVENT_FINISH) { \/\* babalgame's _on_Ball_finish \*\//a\            c->new_record = true;|\"New record!\" on every finish"
    "babal_app.c|s/ms - c->done_ms >= 600/ms - c->done_ms >= 60000/|the finish screen held 60 s"
    "babal_app.c|s/{BB_EVENT_FALL, 150, 150}, //|the fall tone silenced"
    "render.c|s/            int32_t c = r % w;/            int32_t c = w - 1 - r % w;/|the track drawn mirrored"
    "render.c|s/int lift = (int)(p->position_vec.y \* PX_PER_UNIT \/ 100.0);/int lift = (int)(p->position_vec.y * 0.0);/|the ball never lifts"
    # From the second stage-2 review: what the app's own state could hide
    "babal_app.c|s/        if (e->kind == BB_EVENT_FINISH) {/        if (e->kind == BB_EVENT_FALL) {/|a game ends at its first fall"
    "babal_app.c|s/            c->skill = (bb_skill)((c->skill + 4) % 5);/            start_game(c, ms);/|4 at the title starts a game"
    "babal_app.c|s/    return BABAL_TICK_MS;/    return 0;/|tick asks for no more ticks"
    "babal_app.c|s/bb_player_set_best_score(\&c->player, best_score(c));/bb_player_set_best_score(\&c->player, c->games > 1 ? best_score(c) : 0);/|no best for the first game after power-on"
    "babal_app.c|s/save\[0\] != SAVE_MAGIC/0/|a save with the wrong magic taken"
    "babal_app.c|s/            if (b > 0)/            if (b > 1)/|a first-format best of 1 dropped"
    # Abacus's ruling (#4640): babalgame's best over all skills, its "New record!", its list
    "babal_app.c|s/bb_player_set_best_score(\&c->player, best_score(c));/bb_player_set_best_score(\&c->player, c->n_scores \&\& c->scores[0].skill == c->skill ? best_score(c) : 0);/|the best per skill again"
    "babal_app.c|/    if (c->state == BABAL_PLAY \&\& !c->score_saved) {/,/^    }/s/add_score(c, bb_player_score(\&c->player), c->skill);/(void)0;/|an unfinished game's score dropped on leaving"
    "babal_app.c|s/while (at > 0 \&\& c->scores\[at - 1\].points < points)/while (at > 0 \&\& c->scores[at - 1].points > points)/|the list kept lowest first"
    "babal_app.c|s/ \&\& (int32_t)get32(e) > (int32_t)get32(e - 5)))/ \&\& 0))/|a save with scores out of order taken"
)
echo "== plants in the app (each must go red)"
for p in "${PLANTS[@]}"; do
    IFS='|' read -r file expr what <<< "$p"
    P=$B/plant
    rm -rf "$P" && mkdir -p "$P" && cp -r "$ROOT/app" "$P/app"
    sed -i "$expr" "$P/app/$file"
    if cmp -s "$P/app/$file" "$ROOT/app/$file"; then echo "PLANT DID NOT APPLY: $what"; ok=0; continue; fi
    # shellcheck disable=SC2086
    if ! cc $CFLAGS -I"$P/app" -I"$ROOT/babal" -I"$ROOT/oracle" -I"$FW" "$ROOT/tools/apphost.c" "$ROOT/tools/render_ref.c" "$P"/app/*.c \
        "$FW/font5x7.c" "$ROOT/oracle/out.c" "$ROOT/oracle/game.c" "$ROOT"/babal/*.c -lm -o "$P/apphost" 2> "$P/cc.err"; then
        echo "PLANT DID NOT BUILD: $what"; head -3 "$P/cc.err"; ok=0; continue
    fi
    if check "$P/apphost" "$P/run" stop > "$P/result"; then echo "SURVIVED: $what"; ok=0; else echo "red: $what ($(sed 's/^  //' "$P/result"))"; fi
done
[ "$ok" = 1 ] && { echo "APP GATE GREEN"; exit 0; }
echo "APP GATE RED"; exit 1
