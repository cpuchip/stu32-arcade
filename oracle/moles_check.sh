#!/bin/bash
# SPDX-License-Identifier: MIT
# MOLES's checks (docs/moles.md, "The oracle"), all on the host:
#   1 the logic against oracle/moles/model.py (written from the doc alone): 300 fuzzed games and the model's own
#     script, every state line identical; and gentle and full unchanged by the levels: the logic at 1194a7f (from
#     git) and now give the same lines on the gentle and full games of the fuzz, the long games and the edges;
#   2 long games: the autopilot (oracle/moles/pilot.py) in gentle to the wait's floor, in full through the stay's
#     floor and two moles at once to the end, in middle through its stay's floor (40) to the end, and a full and a
#     middle game lost to getaways, the C and the model identical;
#   3 the rules at their edges (oracle/moles/edges.py): in the C and the model, against values worked out by hand, and
#     every state line of the C against the model's;
#   4 the app on the SDK runner: scripts from oracle/moles/appscript.py with their frame hashes (key frames too)
#     against oracle/moles/frames/, each tick's ask (-nexts), the save and the sound the rules give; a game left
#     running with no save and with six damaged ones that must be refused; with WASI_SDK, WAMRC and WAMR_TREE set,
#     the same through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red; a plant that does not build or changes nothing is red.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=moles APP=moles_app NAME=MOLES SCRIPTS="gentle middle full refuse midsave" MID=midsave
B=$ROOT/build/${GAME}check
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/$GAME
G=$ROOT/games/$GAME
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
PILOTS=("g 0 0 6000 3" "f 3141592653 1 6000 3" "f 2718281828 2 20000 -1" "f 7 3 20000 2" "m 1618033988 4 6000 3"
    "m 1414213562 5 20000 -1")
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
    local i
    for i in "${!PILOTS[@]}"; do "$1" "$B/pilot-$i.txt" | cmp -s - "$B/pilot-$i.py.txt" || return 1; done
    tail -1 "$B/pilot-0.py.txt" | grep -q ' wait=10 .* over=0 ' && tail -1 "$B/pilot-1.py.txt" | grep -q ' stay=16 gap=14 .* over=1 ' \
        && grep -qE 'holes=.*[RUB][0-9]+.*[RUB][0-9]+' "$B/pilot-1.py.txt" && tail -1 "$B/pilot-2.py.txt" | grep -q ' score=0 .* over=1 ' \
        && tail -1 "$B/pilot-3.py.txt" | grep -q ' over=1 ' && grep -q ' stay=40 ' "$B/pilot-4.py.txt" \
        && tail -1 "$B/pilot-4.py.txt" | grep -q ' stay=40 gap=30 .* over=1 ' \
        && ! grep -qE 'holes=.*[RUB][0-9]+.*[RUB][0-9]+' "$B/pilot-4.py.txt" \
        && tail -1 "$B/pilot-5.py.txt" | grep -q ' score=0 bonks=0 lives=0 .* over=1 ' || return 1
}
# gentle and full unchanged: the logic as it was before the levels (1194a7f), on the gentle and full games only
OLD=1194a7f
gf_only() { awk '/^game /{keep = ($2 != "m")} keep' "$1"; }
check1b() { # check1b TRACE: the old logic's lines against TRACE's
    local d="$B/before" s
    mkdir -p "$d" || return 1
    for f in games/moles/moles_logic.c games/moles/moles_logic.h tests/moles_trace.c; do
        git -C "$ROOT" show "$OLD:$f" > "$d/$(basename "$f")" 2> /dev/null || return 2
    done
    cc $STRICT -O2 -I"$d" -o "$d/trace" "$d/moles_trace.c" "$d/moles_logic.c" || return 1
    for s in fuzz pilot-0 pilot-1 pilot-2 pilot-3 edges; do
        gf_only "$B/$s.txt" > "$d/$s.txt"
        grep -q '^game [gf] ' "$d/$s.txt" || return 1
        "$d/trace" "$d/$s.txt" | cmp -s - <("$1" "$d/$s.txt") || return 1
    done
}
check3() { "$1" "$B/edges.txt" > "$B/edges.out" && (cd "$O" && python3 edges.py check "$B/edges.out") > /dev/null \
    && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

check1b "$B/trace_fast"; case $? in
    0) echo "  1 gentle and full unchanged: the logic at $OLD and now give the same lines on $(cat "$B"/before/*.txt | grep -c '^game') gentle and full games (the fuzz's, the long games', the edges')" ;;
    2) echo "  1 gentle and full unchanged: skipped ($OLD is not in this clone)" ;;
    *) red "1 gentle or full changed by the levels" ;;
esac
check1 "$B/trace" && echo "  1 logic: $(grep -c '^game' "$B/fuzz.txt") fuzzed games ($(grep -c ' over=1 ' "$B/fuzz.py.txt") ended) and the model's script, $(cat "$B/fuzz.py.txt" "$B"/own-*.py.txt | wc -l) state lines, identical to the model" \
    || red "1 logic: the C and the model differ"
if check2 "$B/trace_fast"; then
    echo "  2 long games: ${#PILOTS[@]} games, $(cat "$B"/pilot-*.py.txt | wc -l) steps, the C and the model identical; the wait's floor, the stay's, two moles at once, full games over"
else
    red "2 long games: a game differs, or did not reach its floors, two moles or its end"
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
# six damaged saves: the wrong magic, the wrong version, 19 bytes, the 16 bytes from before the levels, a non-zero
# last byte, the digits byte at 5
printf '%s' "58${good:2}" | xxd -r -p > "$B/bad-magic.load"
printf '%s' "${good:0:2}02${good:4}" | xxd -r -p > "$B/bad-version.load"
printf '%s' "${good:0:38}" | xxd -r -p > "$B/bad-short.load"
printf '%s' "${good:0:28}0100" | xxd -r -p > "$B/bad-old16.load"
printf '%s' "${good:0:38}01" | xxd -r -p > "$B/bad-tail.load"
printf '%s' "${good:0:36}05${good:38}" | xxd -r -p > "$B/bad-tail18.load"
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
        echo "  4 app $1: left running, the save as the rules give it (the best kept at each bonk), every tick's ask"; return ;;
    esac
    if [ "${BLESS:-0}" = 1 ]; then mkdir -p "$O/frames" && cp "$B/$1.$sfx.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.$sfx.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/$GAME/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.$sfx.hashes") frames as committed, $(wc -l < "$B/$1.$sfx.nexts") ticks' asks; $tunes tunes, $tones tones; save as expected"
}
APPRUNS="$SCRIPTS $MID-magic $MID-version $MID-short $MID-old16 $MID-tail $MID-tail18"
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
        "$4" "$B/p-$1"
        case "$?:$4" in
            0:*) red "5 plant: $5 passes $4" ;;
            2:check1b) echo "  5 plant: $5: skipped ($OLD is not in this clone)" ;;
            *) echo "  5 plant: $5: $4 red" ;;
        esac
    fi
}
L=${GAME}_logic.c
logic_plant swap17 $L 's/        return digit - 7;/        return digit == 7 ? 6 : digit - 7;/' check3 "1 and 7 in each other's holes"
logic_plant upside $L 's/    if \(digit >= 1 && digit <= 3\)\n        return digit \+ 5;/    if (digit >= 1 \&\& digit <= 3)\n        return digit - 1;/' check1 "the bottom row on the top"
logic_plant repeat $L 's/i != g->last\)/i >= 0)/' check3 "the same hole twice running"
logic_plant waitfloor $L 's/g->wait > 10\)/g->wait > 9)/' check3 "the wait's floor at 9"
logic_plant stayfloor $L 's/\{3, 50, 16, 20, 14, 2, MOLES_STUN\}/{3, 50, 15, 20, 14, 2, MOLES_STUN}/' check3 "the stay's floor at 15"
logic_plant gentlewait $L 's/        g->wait = 30;/        g->wait = 31;/' check1b "gentle's first wait changed"
logic_plant fullstay $L 's/\{3, 50, 16, 20, 14, 2, MOLES_STUN\}/{3, 51, 16, 20, 14, 2, MOLES_STUN}/' check1b "full's first stay changed"
logic_plant midfloor $L 's/\{5, 80, 40, 30, 30, 1, 0\}/{5, 80, 39, 30, 30, 1, 0}/' check3 "middle's stay floor at 39"
logic_plant midstay $L 's/\{5, 80, 40, 30, 30, 1, 0\}/{5, 81, 40, 30, 30, 1, 0}/' check3 "middle's stay starting at 81"
logic_plant midlives $L 's/\{5, 80, 40, 30, 30, 1, 0\}/{4, 80, 40, 30, 30, 1, 0}/' check3 "middle with 4 lives"
logic_plant midgap $L 's/\{5, 80, 40, 30, 30, 1, 0\}/{5, 80, 40, 30, 14, 1, 0}/' check3 "middle's gap cut to 14 at 20 bonks"
logic_plant midtwo $L 's/\{5, 80, 40, 30, 30, 1, 0\}/{5, 80, 40, 30, 30, 2, 0}/' check3 "two middle moles from 20 bonks"
logic_plant midstun $L 's/\{5, 80, 40, 30, 30, 1, 0\}/{5, 80, 40, 30, 30, 1, MOLES_STUN}/' check3 "the stun in middle"
logic_plant sinkhit $L 's/h->state == MOLES_RISE \|\| h->state == MOLES_UP\) \{\n            \*h = \(moles_hole\)\{MOLES_BONK/h->state == MOLES_RISE || h->state == MOLES_UP || h->state == MOLES_SINK) {\n            *h = (moles_hole){MOLES_BONK/' check3 "a sinking mole counted as a hit"
logic_plant nostun $L 's/                g->stun = timed\[t\].stun, stunned = started = true;/                (void)started;/' check3 "the stun removed"
logic_plant gentlestun $L 's/            if \(timing && timed\[t\].stun\)\n                g->stun = timed\[t\].stun/            if (1)\n                g->stun = MOLES_STUN/' check3 "the stun in gentle"
logic_plant extend $L 's/for \(int q = 0; q < g->nq && !stunned; q\+\+\) \{/for (int q = 0; q < g->nq; q++) { if (stunned) { g->stun = MOLES_STUN; continue; }/' check3 "a stun extended by the keys during it"
logic_plant fresh $L 's/            fresh \|= \(uint16_t\)\(1u << i\);\n//' check3 "a bonk counted down on its own step"
logic_plant bonkedfree $L 's/g->h\[i\].state == MOLES_UP \|\| g->h\[i\].state == MOLES_BONK;\n    return n;/g->h[i].state == MOLES_UP;\n    return n;/' check3 "a bonked mole not holding the next back"
logic_plant twolate $L 's/g->bonks >= 20 \? timed\[t\].two : 1/g->bonks >= 21 ? timed[t].two : 1/' check3 "two moles from the 21st bonk"
logic_plant repeatbonk $L 's/    if \(down && !was && g->nq < MOLES_QUEUE\)/    if (down \&\& (was || !was) \&\& g->nq < MOLES_QUEUE)/' check3 "a held key's repeat bonking"
logic_plant firstdrop $L 's/        bool waited = g->last >= 0;/        bool waited = true;/' check3 "the wait dropped at the first mole"
logic_plant threesteps $L 's/!g->over && n < 2\) \{/!g->over \&\& n < 3) {/' check3 "three steps in a tick (reached only by a hand-set accumulator)"
logic_plant endall $L 's/            if \(g->h\[i\].state == MOLES_RISE \|\| g->h\[i\].state == MOLES_UP \|\| g->h\[i\].state == MOLES_BONK\)\n                g->h\[i\] = \(moles_hole\)\{MOLES_SINK, 0\};/            if (g->h[i].state != MOLES_EMPTY)\n                g->h[i] = (moles_hole){MOLES_SINK, 0};/' check3 "a sinking mole's count reset at the end"
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
app_plant tailok 's/ \|\| save\[18\] > 1 \|\| save\[19\]\)/)/' $MID-tail "a save with a non-zero tail taken"
app_plant tail18 's/ \|\| save\[18\] > 1 \|\|/ ||/' $MID-tail18 "a save with the digits byte at 5 taken"
app_plant short19 's/if \(n < SAVE_BYTES \|\|/if (n < 19 ||/' $MID-short "a 19-byte save taken"
app_plant old16 's/if \(n < SAVE_BYTES \|\|/if (n < 16 ||/' $MID-old16 "the 16-byte save from before the levels taken"
app_plant nomagic 's/save\[0\] != SAVE_MAGIC \|\| //' $MID-magic "a save with the wrong magic taken"
app_plant ask50 's/return MOLES_STEP_MS - c->g.acc;/return MOLES_STEP_MS;/' gentle "a tick asking 50 ms whatever is held"
app_plant early 's/uint32_t dt = c->now - c->started < ms \? c->now - c->started : ms;/uint32_t dt = ms;/' gentle "the first tick counting time before ENTER"
app_plant bestlate 's/    if \(g->score > c->best\[g->level\]\)\n        c->best\[g->level\] = g->score;\n//' $MID "the best not kept at each bonk"
for d in 1 2 3 4 5 6 7 8 9; do
    app_plant "no$d" "s/    case APP_KEY_$d: return $d;\\n//" gentle "$d not bonking"
done
app_plant plusbonks 's/    default: return 0;/    case APP_KEY_PLUS: return 5;\n    default: return 0;/' gentle "+ bonking"
app_plant fullfirst 's/    c->state = TITLE;\n    c->level = MOLES_GENTLE;/    c->state = TITLE;\n    c->level = MOLES_MIDDLE;/' gentle "the title opening on middle"
app_plant nodigits 's/        c->digits = !c->digits;/        c->digits = c->digits;/' gentle "0 not turning the digits on"
app_plant digitsave 's/    save\[18\] = c->digits;\n//' refuse "the digits option not saved"
app_plant onemode 's/c->best\[g->level\] = g->score;/c->best[0] = g->score;/' full "full's best kept as gentle's"
app_plant midbest 's/c->best\[g->level\] = g->score;/c->best[g->level == MOLES_MIDDLE ? MOLES_FULL : g->level] = g->score;/' middle "middle's best kept as full's"
app_plant wrapright 's/        if \(c->level == MOLES_FULL\)\n            return false;\n        c->level\+\+;/        c->level = (uint8_t)((c->level + 1) % MOLES_LEVELS);/' full "the title's choice wrapping past full"
app_plant wrapleft 's/        if \(c->level == MOLES_GENTLE\)\n            return false;  \/\* the choice stops at the ends \*\/\n        c->level--;/        c->level = (uint8_t)((c->level + 2) % MOLES_LEVELS);/' full "the title's choice wrapping past gentle"
app_plant bonktune 's/static const uint16_t bonk\[\] = \{220, 30, 1320, 15\};/static const uint16_t bonk[] = {220, 30, 1320, 20};/' gentle "the bonk's second note 20 ms"
app_plant thunkcut 's/\} else if \(c->sound & 4 && c->now >= c->tune_until\) \{/} else if (c->sound \& 4) {/' gentle "a thunk cutting a tune short"
app_plant again300 's/#define AGAIN_MS 500/#define AGAIN_MS 300/' full "the end taking ENTER at 0.3 s"
app_plant enterplay 's/    if \(c->state == PLAY\) \{\n        moles_key/    if (c->state == PLAY \&\& down \&\& key == APP_KEY_ENTER) {\n        new_game(c, ms);\n        return true;\n    }\n    if (c->state == PLAY) {\n        moles_key/' gentle "ENTER restarting a game in play"
app_plant nodigit 's/        if \(h->state == MOLES_UP && c->digits\) \{/        if (h->state == MOLES_UP \&\& c->digits \&\& 0) {/' gentle "the digit not drawn"
app_plant nomole 's/        mole\(f, cx, cy, rows\);/        (void)rows;/' gentle "the moles not drawn"
app_plant endmoles 's/c->state == OVER         \? 0 /c->state == -1           ? 0 /' full "moles left out of their holes at the end"
app_plant shortdust 's/c->puff\[i\] = g->level == MOLES_FULL \? MOLES_STUN : PUFF_STEPS;/c->puff[i] = PUFF_STEPS;/' full "the dust gone before the stun"
app_plant middust 's/c->puff\[i\] = g->level == MOLES_FULL \? MOLES_STUN : PUFF_STEPS;/c->puff[i] = g->level != MOLES_GENTLE ? MOLES_STUN : PUFF_STEPS;/' middle "middle's dust as long as full's stun"

[ "$ok" = 1 ] && echo "${GAME}check: GREEN" || { echo "${GAME}check: RED"; exit 1; }
