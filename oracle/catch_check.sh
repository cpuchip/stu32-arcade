#!/bin/bash
# SPDX-License-Identifier: MIT
# CATCH's checks (docs/catch.md, "The oracle"), all on the host:
#   1 the logic against oracle/catch/model.py (written from the doc alone): 300 fuzzed games and the model's own
#     script, every state line identical; and gentle and full changed by the levels only in the reach: the logic at
#     1194a7f (from git) and now with the reach turned off give the same lines (but for `last=`) on the gentle and
#     full games of the fuzz, the long games and the edges;
#   2 long games: the autopilot (oracle/catch/pilot.py) in full for two seeds to the end, in gentle for 12,000 steps,
#     in middle to the end, and in middle and full from near their limits, the C and the model identical, each
#     middle and full game over and the limits reached; the reach (oracle/catch/reach.py): the rule on its own in every
#     level with its two controls, the model's own draws in every level losing no star, and a perfect player in each
#     level catching every star to 2,000 catches, the C and the model identical;
#   3 the rules at their edges (oracle/catch/edges.py): in the C and the model, against values worked out by hand, and
#     every state line of the C against the model's;
#   4 the app on the SDK runner: scripts from oracle/catch/appscript.py with their frame hashes (key frames too)
#     against oracle/catch/frames/, each tick's ask (-nexts), the save and the sound the rules give; a game left
#     running with no save and with six damaged ones that must be refused; with WASI_SDK, WAMRC and WAMR_TREE set (as
#     for oracle/wasm_check.sh), the same through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red; a plant that does not build or changes nothing is red.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GAME=catch APP=catch_app NAME=CATCH SCRIPTS="gentle middle full refuse midsave" MID=midsave
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
PILOTS=("f 3141592653 0 20000" "f 2718281828 1 20000" "g 0 2 6000" "f 5 3 20000 score=75 fall=88 gap=12 max=5"
    "m 1618033988 4 20000" "m 5 5 20000 score=55 fall=44 gap=20 max=4")
for i in "${!PILOTS[@]}"; do
    (cd "$O" && python3 pilot.py ${PILOTS[$i]}) > "$B/pilot-$i.txt"
    (cd "$O" && python3 model.py "$B/pilot-$i.txt") > "$B/pilot-$i.py.txt"
done
# the perfect player in each level: about 2,000 catches (a star every gap steps at the limits)
PERFECT=("g 11 0 70000" "m 12 1 37000" "f 13 2 21000")
for i in "${!PERFECT[@]}"; do
    (cd "$O" && python3 reach.py pilot ${PERFECT[$i]}) > "$B/perfect-$i.txt"
    (cd "$O" && python3 model.py "$B/perfect-$i.txt") > "$B/perfect-$i.py.txt"
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
    tail -1 "$B/pilot-0.py.txt" | grep -q ' over=1 ' && tail -1 "$B/pilot-1.py.txt" | grep -q ' over=1 ' \
        && tail -1 "$B/pilot-3.py.txt" | grep -q ' fall=96 gap=9 max=6 .* over=1 ' \
        && ! grep -q ' over=1 ' "$B/pilot-2.py.txt" && tail -1 "$B/pilot-4.py.txt" | grep -q ' over=1 ' \
        && tail -1 "$B/pilot-5.py.txt" | grep -q ' fall=48 gap=18 max=4 .* over=1 ' || return 1
    for i in "${!PERFECT[@]}"; do
        "$1" "$B/perfect-$i.txt" | cmp -s - "$B/perfect-$i.py.txt" || return 1
        want=$(sed -n 's/^# expect caught //p' "$B/perfect-$i.txt")
        tail -1 "$B/perfect-$i.py.txt" | grep -q " score=$want " && [ "$want" -ge 2000 ] || return 1
    done
}
reach_model() { # every level's own draws: no star lost by a perfect player
    local m s
    for m in g m f; do for s in 1 2 3 4 5 6 7 8 9 10; do
        (cd "$O" && python3 reach.py model $m $s $s 6000) | grep -q '^lost 0 of [1-9]' || return 1
    done; done
}
# gentle and full otherwise unchanged: the logic before the levels (1194a7f) against DIR's with the reach turned off
OLD=1194a7f
gf_only() { awk '/^game /{keep = ($2 != "m")} keep' "$1"; }
check1b() { # check1b DIR: 0 the same, 1 not, 2 skipped (no 1194a7f here)
    local d="$B/before" n="$B/noreach-$(basename "$1")" s f
    mkdir -p "$d" "$n" || return 1
    for f in games/catch/catch_logic.c games/catch/catch_logic.h tests/catch_trace.c; do
        git -C "$ROOT" show "$OLD:$f" > "$d/$(basename "$f")" 2> /dev/null || return 2
    done
    cc $STRICT -O2 -I"$d" -o "$d/trace" "$d/catch_trace.c" "$d/catch_logic.c" || return 1
    cp "$1"/* "$n/" && perl -0pi -e 's/    if \(g->last < 0\)\n        return/    if (1)\n        return/' "$n/$L" \
        && ! cmp -s "$n/$L" "$1/$L" && build_trace "$STRICT -O2" "$n" "$n/trace" || return 1
    for s in fuzz pilot-0 pilot-1 pilot-2 pilot-3 edges; do
        gf_only "$B/$s.txt" > "$d/$s.txt"
        grep -q '^game [gf] ' "$d/$s.txt" || return 1
        "$d/trace" "$d/$s.txt" | cmp -s - <("$n/trace" "$d/$s.txt" | sed 's/ last=-\{0,1\}[0-9]*//') || return 1
    done
}
check3() { "$1" "$B/edges.txt" > "$B/edges.out" && (cd "$O" && python3 edges.py check "$B/edges.out") > /dev/null \
    && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

L=${GAME}_logic.c
check1b "$G"; case $? in
    0) echo "  1 gentle and full otherwise unchanged: the logic at $OLD and now with the reach off give the same lines on $(cat "$B"/before/*.txt | grep -c '^game') gentle and full games" ;;
    2) echo "  1 gentle and full otherwise unchanged: skipped ($OLD is not in this clone)" ;;
    *) red "1 gentle or full changed by the levels outside the reach" ;;
esac
check1 "$B/trace" && echo "  1 logic: $(grep -c '^game' "$B/fuzz.txt") fuzzed games ($(grep -c ' over=1 ' "$B/fuzz.py.txt") ended) and the model's script, $(cat "$B/fuzz.py.txt" "$B"/own-*.py.txt | wc -l) state lines, identical to the model" \
    || red "1 logic: the C and the model differ"
if check2 "$B/trace_fast"; then
    echo "  2 long games: ${#PILOTS[@]} games, $(cat "$B"/pilot-*.py.txt | wc -l) steps, the C and the model identical; full games over, gentle still playing, full's limits reached"
else
    red "2 long games: a game differs, or a game did not end, or the limits were not reached, or a perfect game missed"
fi
if (cd "$O" && python3 reach.py rule 20 4000) > "$B/reach-rule.txt"; then
    echo "  2 reach: the rule loses no star in any level under three draws, and its controls lose some:"
    sed 's/^/      /' "$B/reach-rule.txt"
else
    red "2 reach: the rule"; cat "$B/reach-rule.txt"
fi
reach_model && echo "  2 reach: the model's own draws, 10 games of 6,000 steps a level, no star lost by a perfect player" \
    || red "2 reach: a star the model drew can't be caught"
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
# last byte, a non-zero byte 18
printf '%s' "58${good:2}" | xxd -r -p > "$B/bad-magic.load"
printf '%s' "${good:0:2}02${good:4}" | xxd -r -p > "$B/bad-version.load"
printf '%s' "${good:0:38}" | xxd -r -p > "$B/bad-short.load"
printf '%s' "${good:0:28}0000" | xxd -r -p > "$B/bad-old16.load"
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
        echo "  4 app $1: left running, the save as the rules give it (the best kept at each step), every tick's ask"; return ;;
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
        "$4" "$B/p-$1" && red "5 plant: $5 passes $4" || echo "  5 plant: $5: $4 red"
    fi
}
L=${GAME}_logic.c
logic_plant latch $L 's/        g->latch_l = true;/        (void)key;/' check3 "a left tap shorter than a step dropped"
logic_plant clamp $L 's/bx > CATCH_BX_MAX \? CATCH_BX_MAX : bx;/bx > CATCH_BX_MAX - 1 ? CATCH_BX_MAX - 1 : bx;/' check3 "the right wall a pixel short"
logic_plant mouthl $L 's/s.x \+ CATCH_STAR > g->bx \+ 4/s.x + CATCH_STAR > g->bx + 5/' check3 "the mouth's left end a pixel in"
logic_plant mouthr $L 's/s.x < g->bx \+ 44/s.x <= g->bx + 44/' check3 "the mouth's right end a pixel out"
logic_plant endonly $L 's/if \(was < CATCH_BASKET_Y \* CATCH_SUB && now >= CATCH_BASKET_Y \* CATCH_SUB/if (was > -100000 \&\& now >= CATCH_BASKET_Y * CATCH_SUB \&\& now < (CATCH_BASKET_Y + 1) * CATCH_SUB/' check3 "the catch tested in a row where the star ends, not over its fall"
logic_plant fall96 $L 's/\{8, 3, 8, 96, 3, 9, 1, 6\}/{8, 3, 8, 104, 3, 9, 1, 6}/' check3 "the fall's limit at 104"
logic_plant gap9 $L 's/\{8, 3, 8, 96, 3, 9, 1, 6\}/{8, 3, 8, 96, 3, 8, 1, 6}/' check3 "the gap's limit at 8"
logic_plant max6 $L 's/\{8, 3, 8, 96, 3, 9, 1, 6\}/{8, 3, 8, 96, 3, 9, 1, 5}/' check3 "five at once at most"
logic_plant gentlelife $L 's/if \(g->level != CATCH_GENTLE && g->lives > 0\) \{/if (g->lives > 0 || g->level == CATCH_GENTLE) {/' check3 "a gentle miss costing a life"
logic_plant midlives $L 's/\{8, 5, 4, 48, 2, 18, 1, 4\}/{8, 4, 4, 48, 2, 18, 1, 4}/' check1 "middle with 4 lives"
logic_plant midfall $L 's/\{8, 5, 4, 48, 2, 18, 1, 4\}/{8, 5, 4, 52, 2, 18, 1, 4}/' check2 "middle's fall to 52"
logic_plant midgap $L 's/\{8, 5, 4, 48, 2, 18, 1, 4\}/{8, 5, 4, 48, 2, 17, 1, 4}/' check2 "middle's gap to 17"
logic_plant midmove $L 's/\{8, 5, 4, 48, 2, 18, 1, 4\}/{7, 5, 4, 48, 2, 18, 1, 4}/' check1 "middle's basket 7 px a step"
c1b_plant() { # c1b_plant NAME PERL WHAT: a plant judged by check1b
    if plant "$1" $L "$2"; then
        check1b "$B/plant-$1"
        case $? in
            0) red "5 plant: $3 passes check1b" ;;
            2) echo "  5 plant: $3: skipped ($OLD is not in this clone)" ;;
            *) echo "  5 plant: $3: check1b red" ;;
        esac
    fi
}
c1b_plant gentlemove 's/\{6, 0, 0, 32, 0, 30, 0, 3\}/{7, 0, 0, 32, 0, 30, 0, 3}/' "gentle's basket 7 px a step"
c1b_plant fulllives 's/\{8, 3, 8, 96, 3, 9, 1, 6\}/{8, 2, 8, 96, 3, 9, 1, 6}/' "full with 2 lives"
logic_plant noreach $L 's/    if \(g->last < 0\)\n        return/    if (1)\n        return/' check1 "every star anywhere (the reach off)"
logic_plant reachwide $L 's/int r = 3 \* level\[g->level\].move \* g->gap \/ 4;/int r = level[g->level].move * g->gap;/' check1 "the reach without its 3\/4"
logic_plant nolast $L 's/        g->last = star_x\(g\);\n        g->star\[g->n\+\+\] = \(catch_star\)\{g->last,/        int16_t x = star_x(g);\n        g->star[g->n++] = (catch_star){x,/' check1 "last not set at a spawn"
logic_plant reachedge $L 's/hi = g->last \+ r > 389 \? 389 : g->last \+ r;/hi = g->last + r > 388 ? 388 : g->last + r;/' check3 "the reach a pixel short of the right wall"
logic_plant firstwait $L 's/    g->max = 3;\n/    g->max = 3;\n    g->count = 30;\n/' check3 "the first star a gap late"
logic_plant noroom $L 's/        g->count = g->gap;\n    \}/        g->count = g->gap;\n    } else if (g->count == 0) {\n        g->count = g->gap;\n    }/' check3 "the counter reset when there is no room"
logic_plant bumpheld $L 's/g->bump = bx != g->bx && /g->bump = (left != right) \&\& /' check3 "a bump while pressed against the wall"
logic_plant tens $L 's/for \(uint32_t t = before \/ 10; t < g->score \/ 10; t\+\+\)/for (uint32_t t = before \/ 10; t < g->score \/ 10 \&\& g->score % 10 == 0; t++)/' check3 "the pace missed when a ten is passed, not landed on"
logic_plant nocap $L 's/g->acc \+= ms > 100 \? 100 : ms;/g->acc += ms;/' check1 "no 100 ms cap on a tick"
logic_plant lateover $L 's/                    g->over = true;\n                    g->n = 0;\n                    return;/                    g->over = true;/' check3 "the stars after a game-ending miss still tested"
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
app_plant tailok 's/ \|\| save\[18\] \|\| save\[19\]\)/)/' $MID-tail "a save with a non-zero tail taken"
app_plant tail18 's/ \|\| save\[18\] \|\|/ ||/' $MID-tail18 "a save with byte 18 set taken"
app_plant short19 's/if \(n < SAVE_BYTES \|\|/if (n < 19 ||/' $MID-short "a 19-byte save taken"
app_plant old16 's/if \(n < SAVE_BYTES \|\|/if (n < 16 ||/' $MID-old16 "the 16-byte save from before the levels taken"
app_plant nomagic 's/save\[0\] != SAVE_MAGIC \|\| //' $MID-magic "a save with the wrong magic taken"
app_plant ask50 's/return CATCH_STEP_MS - c->g.acc;/return CATCH_STEP_MS;/' gentle "a tick asking 50 ms whatever is held"
app_plant early 's/uint32_t dt = c->now - c->started < ms \? c->now - c->started : ms;/uint32_t dt = ms;/' gentle "the first tick counting time before ENTER"
app_plant bestlate 's/    if \(g->score > c->best\[g->level\]\)\n        c->best\[g->level\] = g->score;\n//' $MID "the best not kept at each catch"
for k in 4:K4 SOFT1:KS1 6:K6 SOFT6:KS6; do
    app_plant "no${k%:*}" "s/    case APP_KEY_${k%:*}: return CATCH_${k#*:};\\n//" gentle "${k%:*} not moving the basket"
done
app_plant plusmoves 's/    default: return -1;/    case APP_KEY_PLUS: return CATCH_K6;\n    default: return -1;/' gentle "+ moving the basket"
app_plant fullfirst 's/    c->state = TITLE;\n    c->level = CATCH_GENTLE;/    c->state = TITLE;\n    c->level = CATCH_MIDDLE;/' gentle "the title opening on middle"
app_plant onemode 's/c->best\[g->level\] = g->score;/c->best[0] = g->score;/' full "full's best kept as gentle's"
app_plant midbest 's/c->best\[g->level\] = g->score;/c->best[g->level == CATCH_MIDDLE ? CATCH_FULL : g->level] = g->score;/' middle "middle's best kept as full's"
app_plant wrapright 's/        if \(c->level == CATCH_FULL\)\n            return false;\n        c->level\+\+;/        c->level = (uint8_t)((c->level + 1) % CATCH_LEVELS);/' full "the title's choice wrapping past full"
app_plant wrapleft 's/        if \(c->level == CATCH_GENTLE\)\n            return false;  \/\* the choice stops at the ends \*\/\n        c->level--;/        c->level = (uint8_t)((c->level + 2) % CATCH_LEVELS);/' full "the title's choice wrapping past gentle"
app_plant middlefade 's/if \(g->ev_misses && g->level == CATCH_GENTLE\) \{/if (g->ev_misses \&\& g->level != CATCH_FULL) {/' middle "a middle miss blinking as a gentle one does"
app_plant chime 's/app_beep\(1047, 30\);/app_beep(1047, 40);/' gentle "the catch's chime 40 ms"
app_plant nobump 's/        else if \(c->sound & 16\)\n            app_beep\(100, 10\);\n//' gentle "the wall's bump silent"
app_plant missfirst 's/        else if \(c->sound & 16\)\n            app_beep\(100, 10\);\n        else if \(c->sound & 2\)\n            app_beep\(196, 30\);/        else if (c->sound \& 2)\n            app_beep(196, 30);\n        else if (c->sound \& 16)\n            app_beep(100, 10);/' gentle "a miss's note over the wall's bump"
app_plant tunecut 's/\} else if \(c->now >= c->tune_until\) \{/} else if (c->tune_until || 1) {/' full "a beep cutting a tune short"
app_plant again300 's/#define AGAIN_MS 500/#define AGAIN_MS 300/' full "the end taking ENTER at 0.3 s"
app_plant enterplay 's/    if \(c->state == PLAY\) \{\n        catch_key/    if (c->state == PLAY \&\& down \&\& key == APP_KEY_ENTER) {\n        new_game(c, ms);\n        return true;\n    }\n    if (c->state == PLAY) {\n        catch_key/' gentle "ENTER restarting a game in play"
app_plant nobasket 's/    basket\(f, g->bx \+ 4, CATCH_TOP/    (void)basket; if (0) basket(f, g->bx + 4, CATCH_TOP/' gentle "the basket not drawn"
app_plant widebasket 's/    basket\(f, g->bx \+ 4, CATCH_TOP \+ CATCH_BASKET_Y, CATCH_BASKET_W - 8,/    basket(f, g->bx, CATCH_TOP + CATCH_BASKET_Y, CATCH_BASKET_W,/' gentle "the basket drawn wider than it catches"

[ "$ok" = 1 ] && echo "${GAME}check: GREEN" || { echo "${GAME}check: RED"; exit 1; }
