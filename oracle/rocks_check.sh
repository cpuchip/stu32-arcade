#!/bin/bash
# SPDX-License-Identifier: MIT
# 3720's checks (docs/3720.md, "The oracle"), all on the host; the C names say rocks:
#   1 the logic against oracle/rocks/model.py (written from the doc alone): 300 fuzzed games and the model's own
#     script, every state line identical;
#   2 long games: the autopilot (oracle/rocks/pilot.py) in gentle and full for three seeds to the end, the C and the
#     model identical, past wave 5 in gentle and wave 4 (six large) in full, an extra ship, each game over;
#   3 the rules at their edges (oracle/rocks/edges.py): in the C and the model, against values worked out by hand, and
#     every state line of the C against the model's;
#   4 the app on the SDK runner: scripts from oracle/rocks/appscript.py with their frame hashes (key frames too)
#     against oracle/rocks/frames/, each tick's ask (-nexts), the save and the sound the rules give; a game left
#     running with no save and with five damaged ones that must be refused; with WASI_SDK, WAMRC and WAMR_TREE set,
#     the same through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red; a plant that does not build or changes nothing is red.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=rocks APP=rocks_app NAME=3720 SCRIPTS="gentle full refuse midsave" MID=midsave
B=$ROOT/build/${GAME}check
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/$GAME
G=$ROOT/games/3720
K=$ROOT/games/kids
STRICT="-std=c11 -Wall -Wextra -Wpedantic -Werror"
SAN="$STRICT -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
red() { echo "RED: $*"; ok=0; }

for f in "$G/${GAME}_logic.c" "$G/${GAME}_app.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$K" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/${GAME}_trace.c" "$2/${GAME}_logic.c" || { red "build in $2"; return 1; }; }
build_app() { cc $SAN -DAPP_DEF=$APP -I"$2" -I"$K" -I"$FW" -o "$3" "$ROOT/sdk/runner/apprun.c" "$2/${GAME}_app.c" \
    "$2/${GAME}_logic.c" "$FW/draw.c" "$FW/font5x7.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" && build_app x "$G" "$B/app" \
    || { echo "RED: build"; exit 1; }

# the model's outputs, once
(cd "$O" && python3 fuzz.py 30 10) > "$B/fuzz.txt"
(cd "$O" && python3 model.py "$B/fuzz.txt") > "$B/fuzz.py.txt"
OWN="smoke"
for s in $OWN; do (cd "$O" && python3 model.py "scripts/$s.txt") > "$B/own-$s.py.txt"; done
PILOTS=("g 3141592653 0 20000" "f 2718281828 1 20000" "f 0 2 20000")
for i in "${!PILOTS[@]}"; do
    (cd "$O" && python3 pilot.py ${PILOTS[$i]}) > "$B/pilot-$i.txt"
    (cd "$O" && python3 model.py "$B/pilot-$i.txt") > "$B/pilot-$i.py.txt"
done
(cd "$O" && python3 edges.py script) > "$B/edges.txt"
(cd "$O" && python3 model.py "$B/edges.txt") > "$B/edges.py.txt"

check1() {
    "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt" || return 1
    for s in $OWN; do "$1" "$O/scripts/$s.txt" | cmp -s - "$B/own-$s.py.txt" || return 1; done
}
check2() {
    local i w
    for i in "${!PILOTS[@]}"; do
        "$1" "$B/pilot-$i.txt" | cmp -s - "$B/pilot-$i.py.txt" || return 1
        tail -1 "$B/pilot-$i.py.txt" | grep -q ' over=1 ' || return 1
    done
    w=$(grep -oE ' wave=[0-9]+' "$B/pilot-0.py.txt" | cut -d= -f2 | sort -n | tail -1); [ "$w" -gt 5 ] || return 1
    w=$(grep -oE ' wave=[0-9]+' "$B/pilot-1.py.txt" | cut -d= -f2 | sort -n | tail -1); [ "$w" -gt 4 ] || return 1
    grep -q ' ships=4 ' "$B/pilot-1.py.txt" || return 1
}
check3() { "$1" "$B/edges.txt" > "$B/edges.out" && (cd "$O" && python3 edges.py check "$B/edges.out") > /dev/null \
    && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

check1 "$B/trace" && echo "  1 logic: $(grep -c '^game' "$B/fuzz.txt") fuzzed games ($(grep -c ' over=1 ' "$B/fuzz.py.txt") ended) and the model's script, $(cat "$B/fuzz.py.txt" "$B"/own-*.py.txt | wc -l) state lines, identical to the model" \
    || red "1 logic: the C and the model differ"
if check2 "$B/trace_fast"; then
    echo "  2 long games: ${#PILOTS[@]} games, $(cat "$B"/pilot-*.py.txt | wc -l) steps, the C and the model identical; waves $(for i in "${!PILOTS[@]}"; do grep -oE ' wave=[0-9]+' "$B/pilot-$i.py.txt" | cut -d= -f2 | sort -n | tail -1; done | tr '\n' ' ')reached, an extra ship, each game over"
else
    red "2 long games: a game differs, or did not reach its waves, an extra ship or its end"
fi
(cd "$O" && python3 edges.py check "$B/edges.py.txt") > "$B/edges.model.txt" || { red "3 edges: the model"; cat "$B/edges.model.txt"; }
if check3 "$B/trace"; then
    echo "  3 edges: $(grep -c '^# case' "$B/edges.txt") cases, the C and the model give the values worked out by hand, every state line identical"
else
    red "3 edges: the C"; (cd "$O" && python3 edges.py check "$B/edges.out") | grep -v '^ok'
fi

# 4: the app; each script names its seed, its save, and last what it must end with
[ "${1:-}" = --bless ] && BLESS=1
for s in $SCRIPTS; do
    (cd "$O" && python3 appscript.py "$s") > "$B/app-$s.txt"
    (cd "$O" && python3 appscript.py nexts "$s") > "$B/app-$s.nexts"
done
good=$(cd "$O" && python3 appscript.py loadfile)
printf '%s' "$good" | xxd -r -p > "$B/refuse.load"
# five damaged saves: the wrong magic, the wrong version, 15 bytes, a non-zero last byte, a non-zero byte 14
printf '%s' "58${good:2}" | xxd -r -p > "$B/bad-magic.load"
printf '%s' "${good:0:2}02${good:4}" | xxd -r -p > "$B/bad-version.load"
printf '%s' "${good:0:30}" | xxd -r -p > "$B/bad-short.load"
printf '%s' "${good:0:30}01" | xxd -r -p > "$B/bad-tail.load"
printf '%s' "${good:0:28}05${good:30}" | xxd -r -p > "$B/bad-tail14.load"
script_of() { echo "$B/app-${1%%-*}.txt"; }
app_run() { # app_run RUNNER NAME SUFFIX -> $B/NAME.SUFFIX.{hashes,save,out,nexts}
    local seed=5 load=()
    [ "$2" = refuse ] && seed=9 load=(-load "$B/refuse.load")
    case "$2" in $MID*) seed=9 ;; esac
    case "$2" in $MID-*) load=(-load "$B/bad-${2#$MID-}.load") ;; esac
    "$1" "$(script_of "$2")" -seed $seed "${load[@]}" -v -keyframes -hashes "$B/$2.$3.hashes" -nexts "$B/$2.$3.nexts" \
        -save "$B/$2.$3.save" > "$B/$2.$3.out" 2>&1
}
expect() { # expect NAME SUFFIX: the script's last line, '# expect save HEX tunes N tones N', the sound and the asks
    local t s sfx=${2:-app}
    read -r _ _ _ save _ tunes _ tones < <(tail -1 "$(script_of "$1")")
    t=$(tail -1 "$B/$1.$sfx.out")
    s=$(od -An -v -tx1 "$B/$1.$sfx.save" | tr -d ' \n')
    case "$t" in *" $tones tones, $tunes tunes,"*) ;; *) red "4 app $1: '$t' (want $tunes tunes, $tones tones)"; return ;; esac
    [ "$s" = "$save" ] || { red "4 app $1: save $s, want $save"; return; }
    cmp -s <(grep -E '^(tone|tune) ' "$B/$1.$sfx.out") <(grep '^# sound ' "$(script_of "$1")" | sed 's/^# sound //') \
        || { red "4 app $1: the sound differs from the script's"; return; }
    cmp -s "$B/$1.$sfx.nexts" "$B/app-${1%%-*}.nexts" || { red "4 app $1: a tick's ask differs from the rules'"; return; }
    case "$1" in $MID*)
        echo "  4 app $1: left running, the save as the rules give it (the best kept as the score passes it), every tick's ask"; return ;;
    esac
    if [ "${BLESS:-0}" = 1 ]; then mkdir -p "$O/frames" && cp "$B/$1.$sfx.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.$sfx.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/$GAME/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.$sfx.hashes") frames as committed, $(wc -l < "$B/$1.$sfx.nexts") ticks' asks; $tunes tunes, $tones tones; save as expected"
}
APPRUNS="$SCRIPTS $MID-magic $MID-version $MID-short $MID-tail $MID-tail14"
for s in $APPRUNS; do
    app_run "$B/app" "$s" app || red "4 app $s: the runner failed ($(tail -1 "$B/$s.app.out"))"
    expect "$s"
done
. "$ROOT/oracle/kids_wasm.sh"
wasm_leg

# 5: planted faults, each in a copy of the game's sources
plant() { # plant NAME FILE PERL -> $B/plant-NAME holding the games/$GAME copy
    local d="$B/plant-$1"
    mkdir -p "$d" && cp "$G"/* "$d/" && perl -0pi -e "$3" "$d/$2"
    cmp -s "$d/$2" "$G/$2" && { red "5 plant $1: the substitution matched nothing"; return 1; }
    return 0
}
logic_plant() { # logic_plant NAME FILE PERL CHECK WHAT
    if plant "$1" "$2" "$3" && build_trace "$STRICT -O2" "$B/plant-$1" "$B/p-$1"; then
        "$4" "$B/p-$1" && red "5 plant: $5 passes $4" || echo "  5 plant: $5: $4 red"
    fi
}
L=${GAME}_logic.c
logic_plant thrustlatch $L 's/        g->latch_t = true;/        g->latch_t = false;/' check1 "a thrust tap shorter than a step dropped"
logic_plant turnclear $L 's/    g->latch_t = g->latch_f = false;\n/    g->latch_t = g->latch_f = g->latch_l = g->latch_r = false;\n/' check3 "the turn taps cleared every step"
logic_plant repeat1 $L 's/                g->turn = 2;/                g->turn = 1;/' check3 "a held turn every step"
logic_plant dragfloor $L 's/        g->vx -= g->vx \/ 32;/        g->vx -= g->vx >> 5;/' check3 "drag rounded down, not toward zero"
logic_plant fullonly $L 's/        if \(hit\(g, &s\)\)\n            continue;\n        s.x = wrap\(s.x \+ \(s.vx/        s.x = wrap(s.x + (s.vx/' check3 "a shot tested only where its move ends"
logic_plant usedslot $L 's/        if \(g->r\[k\].size == ROCKS_NONE\)\n            j = k;/        if (k == 1 || g->r[k].size == ROCKS_NONE)\n            j = k;/' check3 "a split's second piece into a used slot"
logic_plant seam $L 's/    if \(d >= m \/ 2\)\n        d -= m;/    if (d >= m)\n        d -= m;/' check3 "the distance across a seam taken the long way"
logic_plant minute $L 's/g->clock >= ROCKS_FREE/g->clock > ROCKS_FREE/' check3 "the first minute's end a step late"
logic_plant noshield $L 's/    if \(in_play && g->shield == 0\) \{/    if (in_play) {/' check3 "the shield ignored"
logic_plant cap6 $L 's/if \(g->ships < 5\)/if (g->ships < 6)/' check3 "a sixth ship"
logic_plant onedge $L 's/return dx \* dx \+ dy \* dy < r \* r;/return dx * dx + dy * dy <= r * r;/' check3 "a hit at exactly r"
logic_plant nospeedcap $L 's/        g->vy = g->vy > v \? v : g->vy < -v \? -v : g->vy;\n//' check3 "no speed cap going up and down"
logic_plant lostpause $L 's/    if \(g->pause == 0 && g->lost == 0 && !any_rock\(g\)\) \{/    if (g->pause == 0 \&\& !any_rock(g)) {/' check3 "a pause while the ship is lost"
logic_plant wave5 $L 's/\(3 \+ g->wave < 6 \? 3 \+ g->wave : 6\)/(3 + g->wave < 5 ? 3 + g->wave : 5)/' check3 "full waves of at most five large"
logic_plant gentle4 $L 's/\(g->wave == 1 \? 3 : 4\)/(4)/' check3 "gentle's first wave of four"
logic_plant returnclock $L 's/    if \(g->lost == 0\)\n        g->clock\+\+;/    if (in_play \&\& g->lost == 0)\n        g->clock++;/' check3 "the play clock not counting the return's step"
logic_plant sameway $L 's/\(h1 \+ 12\) % 24/h1/' check1 "a split's pieces flying the same way"
logic_plant nocap $L 's/g->acc \+= ms > 100 \? 100 : ms;/g->acc += ms;/' check1 "no 100 ms cap on a tick"
logic_plant ramscore $L 's/                split\(g, i\);\n                g->lost = 20;/                split(g, i);\n                score(g, g->r[i].size ? g->r[i].size : ROCKS_S);\n                g->lost = 20;/' check3 "a rock that hits the ship scoring"
app_plant() { # app_plant NAME PERL RUN WHAT: a plant in the app, judged by check 4's run RUN
    if plant "$1" ${GAME}_app.c "$2" && build_app x "$B/plant-$1" "$B/pa-$1"; then
        local ok_before=$ok caught=0 BLESS=0
        app_run "$B/pa-$1" "$3" app
        ok=1
        expect "$3" > /dev/null
        [ "$ok" = 0 ] && caught=1
        ok=$ok_before
        [ "$caught" = 1 ] && echo "  5 plant: $4: check 4 red" || red "5 plant: $4 passes check 4"
    fi
}
app_plant tailok 's/ \|\| save\[14\] \|\| save\[15\]\)/)/' $MID-tail "a save with a non-zero tail taken"
app_plant tail14 's/ \|\| save\[14\] \|\|/ ||/' $MID-tail14 "a save with byte 14 set taken"
app_plant short15 's/if \(n < SAVE_BYTES \|\|/if (n < 15 ||/' $MID-short "a 15-byte save taken"
app_plant nomagic 's/save\[0\] != SAVE_MAGIC \|\| //' $MID-magic "a save with the wrong magic taken"
app_plant ask50 's/return ROCKS_STEP_MS - c->g.acc;/return ROCKS_STEP_MS;/' gentle "a tick asking 50 ms whatever is held"
app_plant early 's/uint32_t dt = c->now - c->started < ms \? c->now - c->started : ms;/uint32_t dt = ms;/' gentle "the first tick counting time before ENTER"
app_plant bestlate 's/    if \(g->score > c->best\[g->full\]\)\n        c->best\[g->full\] = g->score;\n//' $MID "the best not kept as the score passes it"
for k in 4:K4 SOFT1:KS1 6:K6 SOFT6:KS6 7:K7 8:K8 9:K9 SOFT2:KS2 GOLD:KG 5:K5 TIMES:KX SOFT5:KS5; do
    app_plant "no${k%:*}" "s/    case APP_KEY_${k%:*}: return ROCKS_${k#*:};\\n//" gentle "${k%:*} doing nothing"
done
app_plant plusfires 's/    case APP_KEY_PLUS: return ROCKS_KP;/    case APP_KEY_PLUS: return ROCKS_KG;/' gentle "+ firing"
app_plant enterfires 's/    case APP_KEY_ENTER: return ROCKS_KE;/    case APP_KEY_ENTER: return ROCKS_K5;/' gentle "ENTER firing"
app_plant fullfirst 's/    c->state = TITLE;\n    c->full = false;/    c->state = TITLE;\n    c->full = true;/' gentle "the title opening on full"
app_plant onemode 's/c->best\[g->full\] = g->score;/c->best[0] = g->score;/' full "full's best kept as gentle's"
app_plant click 's/app_beep\(1500, 8\);/app_beep(1400, 8);/' gentle "the shot's click at 1,400 Hz"
app_plant notick 's/        else if \(c->sound & 256\)\n            app_beep\(110, 15\);\n//' gentle "the engine's tick silent"
app_plant noshieldtune 's/        app_tune\(shield, 3\);/        (void)shield;/' gentle "the shield's end silent"
app_plant nochime 's/        app_tune\(extra, 2\);/        (void)extra;/' full "an extra ship silent"
app_plant tunecut 's/\} else if \(c->now >= c->tune_until\) \{/} else if (c->tune_until || 1) {/' gentle "a beep cutting a tune short"
app_plant again300 's/#define AGAIN_MS 500/#define AGAIN_MS 300/' gentle "the end taking ENTER at 0.3 s"
app_plant enterplay 's/    if \(c->state == PLAY\) \{\n        rocks_key/    if (c->state == PLAY \&\& down \&\& key == APP_KEY_ENTER) {\n        new_game(c, ms);\n        return true;\n    }\n    if (c->state == PLAY) {\n        rocks_key/' gentle "ENTER restarting a game in play"
app_plant noship 's/                ship_at\(f, g->x \+ ox \* ROCKS_Q/                if (0) ship_at(f, g->x + ox * ROCKS_Q/' gentle "the ship not drawn"
app_plant nowrap 's/    for \(int ox = -ROCKS_W; ox <= ROCKS_W; ox \+= ROCKS_W\)/    for (int ox = 0; ox <= 0; ox += ROCKS_W)/' gentle "nothing drawn again across the x seam"

[ "$ok" = 1 ] && echo "${GAME}check: GREEN" || { echo "${GAME}check: RED"; exit 1; }
