#!/bin/bash
# SPDX-License-Identifier: MIT
# SALVO's checks (docs/salvo.md, "The oracle"), all on the host:
#   1 the logic against oracle/salvo/model.py (written from the doc alone): 300 fuzzed games and the model's own
#     scripts, every state line identical;
#   2 long games: the autopilot (oracle/salvo/pilot.py) for three seeds of 20,000 steps and then to the end, the C
#     and the model identical, past wave 40, five ships held, the game over;
#   3 the rules at their edges (oracle/salvo/edges.py): in the C and the model, against values written by hand, and
#     every state line of the C against the model's;
#   4 the app on the SDK runner: scripts from oracle/salvo/appscript.py with their frame hashes (key frames too)
#     against oracle/salvo/frames/, each tick's ask (-nexts), the save and the sound the rules give; a game left
#     running with no save and with five damaged ones that must be refused; with WASI_SDK, WAMRC and WAMR_TREE set (as
#     for oracle/wasm_check.sh), the same through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red; a plant that does not build or changes nothing is red.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/salvocheck
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/salvo
G=$ROOT/games/salvo
STRICT="-std=c11 -Wall -Wextra -Wpedantic -Werror"
SAN="$STRICT -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
red() { echo "RED: $*"; ok=0; }

for f in "$G/salvo_logic.c" "$G/salvo_app.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/salvo_trace.c" "$2/salvo_logic.c" || { red "build in $2"; return 1; }; }
build_app() { cc $SAN -DAPP_DEF=salvo_app -I"$2" -I"$FW" -o "$3" "$ROOT/sdk/runner/apprun.c" "$2/salvo_app.c" \
    "$2/salvo_logic.c" "$FW/draw.c" "$FW/font5x7.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" && build_app x "$G" "$B/app" \
    || { echo "RED: build"; exit 1; }

# the model's outputs, once
(cd "$O" && python3 fuzz.py 20 15) > "$B/fuzz.txt"
(cd "$O" && python3 model.py "$B/fuzz.txt") > "$B/fuzz.py.txt"
OWN="smoke sweep sweep-long edges swept corner"
for s in $OWN; do (cd "$O" && python3 model.py "scripts/$s.txt") > "$B/own-$s.py.txt"; done
SEEDS="3141592653 2718281828 0"
for s in $SEEDS; do
    (cd "$O" && python3 pilot.py "$s" 1 20000) > "$B/pilot-$s.txt"
    (cd "$O" && python3 model.py "$B/pilot-$s.txt") > "$B/pilot-$s.py.txt"
done
(cd "$O" && python3 edges.py script) > "$B/edges.txt"
(cd "$O" && python3 model.py "$B/edges.txt") > "$B/edges.py.txt"

check1() {
    "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt" || return 1
    for s in $OWN; do "$1" "$O/scripts/$s.txt" | cmp -s - "$B/own-$s.py.txt" || return 1; done
}
check2() {
    local s w
    for s in $SEEDS; do
        "$1" "$B/pilot-$s.txt" | cmp -s - "$B/pilot-$s.py.txt" || return 1
        w=$(grep -oE ' wave=[0-9]+' "$B/pilot-$s.py.txt" | cut -d= -f2 | sort -n | tail -1)
        [ "$w" -gt 40 ] && grep -q ' lives=5 ' "$B/pilot-$s.py.txt" && tail -1 "$B/pilot-$s.py.txt" | grep -q ' over=1 ' \
            || return 1
    done
}
check3() { "$1" "$B/edges.txt" > "$B/edges.out" && (cd "$O" && python3 edges.py check "$B/edges.out") > /dev/null \
    && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

check1 "$B/trace" && echo "  1 logic: $(grep -c '^game' "$B/fuzz.txt") fuzzed games ($(grep -c ' over=1 ' "$B/fuzz.py.txt") ended) and the model's 6 scripts, $(cat "$B/fuzz.py.txt" "$B"/own-*.py.txt | wc -l) state lines, identical to the model" \
    || red "1 logic: the C and the model differ"
if check2 "$B/trace_fast"; then
    echo "  2 long games: 3 seeds, $(cat "$B"/pilot-*.py.txt | wc -l) steps, the C and the model identical; waves $(for s in $SEEDS; do grep -oE ' wave=[0-9]+' "$B/pilot-$s.py.txt" | cut -d= -f2 | sort -n | tail -1; done | tr '\n' ' ')reached, 5 ships held, each game over"
else
    red "2 long games: a seed differs, or did not pass wave 40, hold 5 ships and end"
fi
(cd "$O" && python3 edges.py check "$B/edges.py.txt") > "$B/edges.model.txt" || { red "3 edges: the model"; cat "$B/edges.model.txt"; }
if check3 "$B/trace"; then
    echo "  3 edges: $(grep -c '^# case' "$B/edges.txt") cases, the C and the model give the values written by hand, every state line identical"
else
    red "3 edges: the C"; (cd "$O" && python3 edges.py check "$B/edges.out") | grep -v '^ok'
fi

# 4: the app; each script names its seed, its save, and last what it must end with
[ "${1:-}" = --bless ] && BLESS=1
for s in play refuse midsave; do
    (cd "$O" && python3 appscript.py "$s") > "$B/app-$s.txt"
    (cd "$O" && python3 appscript.py nexts "$s") > "$B/app-$s.nexts"
done
good=$(cd "$O" && python3 appscript.py loadfile)
printf '%s' "$good" | xxd -r -p > "$B/refuse.load"
# five damaged saves: the wrong magic, the wrong version, 15 bytes, a non-zero byte first and last in the last six
printf '%s' "58${good:2}" | xxd -r -p > "$B/bad-magic.load"
printf '%s' "${good:0:2}02${good:4}" | xxd -r -p > "$B/bad-version.load"
printf '%s' "${good:0:30}" | xxd -r -p > "$B/bad-short.load"
printf '%s' "${good:0:30}01" | xxd -r -p > "$B/bad-tail.load"
printf '%s' "${good:0:20}05${good:22}" | xxd -r -p > "$B/bad-tail10.load"
script_of() { echo "$B/app-${1%%-*}.txt"; }
app_run() { # app_run RUNNER NAME SUFFIX -> $B/NAME.SUFFIX.{hashes,save,out,nexts}
    local seed=5 load=()
    [ "$2" = refuse ] && seed=9 load=(-load "$B/refuse.load")
    case "$2" in midsave*) seed=9 ;; esac
    case "$2" in midsave-*) load=(-load "$B/bad-${2#midsave-}.load") ;; esac
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
    case "$1" in midsave*)
        echo "  4 app $1: left running, the save as the rules give it (the best kept at each kill), every tick's ask"; return ;;
    esac
    if [ "${BLESS:-0}" = 1 ]; then mkdir -p "$O/frames" && cp "$B/$1.$sfx.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.$sfx.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/salvo/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.$sfx.hashes") frames as committed, $(wc -l < "$B/$1.$sfx.nexts") ticks' asks; $tunes tunes, $tones tones; save as expected"
}
for s in play refuse midsave midsave-magic midsave-version midsave-short midsave-tail midsave-tail10; do
    app_run "$B/app" "$s" app || red "4 app $s: the runner failed ($(tail -1 "$B/$s.app.out"))"
    expect "$s"
done
if [ -n "${WASI_SDK:-}" ] && [ -n "${WAMRC:-}" ] && [ -n "${WAMR_TREE:-}" ]; then
    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
        -Wno-missing-field-initializers -DAPP_DEF=salvo_app -I"$G" -I"$FW" -Wl,--allow-undefined -Wl,-z,stack-size=65536 \
        -o "$B/salvo.wasm" "$ROOT/sdk/wasm/guest.c" "$G/salvo_app.c" "$G/salvo_logic.c" "$FW/draw.c" "$FW/font5x7.c" \
        && "$WAMRC" --target=x86_64 -o "$B/salvo-x86.aot" "$B/salvo.wasm" > "$B/wamrc.log" 2>&1 || red "4 wasm: build"
    python3 - "$B/salvo-x86.aot" "$B/salvo_wasm_app.c" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    f.write('/* generated by oracle/salvo_check.sh */\n#include "bridge.h"\nstatic const uint8_t salvo_aot[] = {\n')
    for i in range(0, len(d), 16):
        f.write(" " + ",".join(str(b) for b in d[i:i + 16]) + ",\n")
    f.write('};\nWASM_GAME(salvo_wasm_app, "SALVO", salvo_aot, salvo_aot + sizeof salvo_aot, 16);\n')
PY
    mkdir -p "$B/vmsrc"
    cat > "$B/vmsrc/CMakeLists.txt" <<'CM'
cmake_minimum_required(VERSION 3.14)
project(vmlib C ASM)
set(WAMR_BUILD_PLATFORM "linux")
set(WAMR_BUILD_TARGET "X86_64")
set(WAMR_BUILD_INTERP 0)
set(WAMR_BUILD_AOT 1)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 0)
set(WAMR_BUILD_LIBC_WASI 0)
set(WAMR_ROOT_DIR ${WAMR_ROOT})
include(${WAMR_ROOT}/build-scripts/runtime_lib.cmake)
add_library(vmlib STATIC ${WAMR_RUNTIME_LIB_SOURCE})
CM
    cmake -S "$B/vmsrc" -B "$B/vm" -DWAMR_ROOT="$WAMR_TREE" -DCMAKE_BUILD_TYPE=Release > "$B/vm.log" 2>&1 \
        && cmake --build "$B/vm" -j 8 >> "$B/vm.log" 2>&1 || red "4 wasm: WAMR's host runtime"
    cc $STRICT -O2 -DAPP_DEF=salvo_wasm_app -I"$ROOT/sdk/wasm" -I"$FW" -I"$WAMR_TREE/core/iwasm/include" -o "$B/wasm" \
        "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/wasm/bridge.c" "$B/salvo_wasm_app.c" "$B/vm/libvmlib.a" -lm -lpthread \
        || red "4 wasm: the runner"
    for s in play refuse midsave midsave-tail; do
        app_run "$B/wasm" "$s" wasm
        if cmp -s "$B/$s.wasm.hashes" "$B/$s.app.hashes" && cmp -s "$B/$s.wasm.save" "$B/$s.app.save" \
            && cmp -s "$B/$s.wasm.nexts" "$B/$s.app.nexts"; then
            echo "  4 wasm $s: the .wasm through the bridge gives the same $(wc -l < "$B/$s.app.hashes") frames, asks and save"
        else red "4 wasm $s: frames, asks or save differ from native"; fi
    done
else
    echo "  4 wasm: skipped (set WASI_SDK, WAMRC and WAMR_TREE to build SALVO as a .wasm)"
fi

# 5: planted faults, each in a copy of the game's sources
plant() { # plant NAME FILE PERL -> $B/plant-NAME holding the games/salvo copy
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
logic_plant latch salvo_logic.c 's/g->latch_l = true;/(void)key;/' check3 "a left tap shorter than a step dropped"
logic_plant clamp salvo_logic.c 's/g->sx \+ 5 > 385 \? 385/g->sx + 5 > 384 ? 384/' check3 "the ship's clamp one short"
logic_plant shots3 salvo_logic.h 's/#define SALVO_SHOTS 2/#define SALVO_SHOTS 3/' check3 "a third shot"
logic_plant cool5 salvo_logic.c 's/g->cool = 6;/g->cool = 5;/' check3 "a cooldown of 5"
logic_plant swaylate salvo_logic.c 's/nfx > 208\)/nfx > 212)/' check3 "the sway's turn late"
logic_plant colorder salvo_logic.c 's/int col = \(c \+ j\) % SALVO_COLS;/int col = (c + SALVO_COLS - j) % SALVO_COLS;/' check1 "the dive's columns taken leftward"
logic_plant vxfloor salvo_logic.c 's/int vx = \(g->sx \+ 1 - a->x\) \/ 12;/int vx = (g->sx + 1 - a->x < 0 ? g->sx + 1 - a->x - 11 : g->sx + 1 - a->x) \/ 12;/' check3 "vx rounded down, not toward zero"
logic_plant firelimit salvo_logic.c 's/a->y < 180 &&/a->y <= 180 \&\&/' check3 "a diver firing at y 180"
logic_plant leftout salvo_logic.c 's/            g->a\[i\].state = SALVO_SLOT;\n    g->ship_t = 40;/            g->a[i].state = g->a[i].state;\n    g->ship_t = 40;/' check1 "a lost ship leaving its divers out"
logic_plant cap6 salvo_logic.c 's/if \(g->lives < 5\)/if (g->lives < 6)/' check3 "a sixth ship"
logic_plant nocap salvo_logic.c 's/g->acc \+= ms > 100 \? 100 : ms;/g->acc += ms;/' check3 "no 100 ms cap on a tick"
logic_plant eshotlate salvo_logic.c 's/p.y = \(int16_t\)\(p.y \+ 6\);\n        if \(p.y < 240\)/p.y = (int16_t)(p.y + 6);\n        if (p.y < 239)/' check1 "an enemy shot gone a row early"
logic_plant swept salvo_logic.c 's/s == fresh \? 6 : 6 \+ 12/s == fresh ? 6 : 6/' check3 "a moving shot tested only where it ends"
logic_plant hitbox salvo_logic.h 's/#define SALVO_HIT_DX 2/#define SALVO_HIT_DX 0/; s/#define SALVO_HIT_W 11/#define SALVO_HIT_W 15/' check3 "the ship's box as wide as its sprite"
logic_plant ramall salvo_logic.c 's/for \(int i = 0; i < SALVO_ALIENS && !lost; i\+\+\) \{\n            salvo_alien \*a = &g->a\[i\];\n            if \(a->state == SALVO_DIVING/for (int i = 0; i < SALVO_ALIENS; i++) {\n            salvo_alien *a = \&g->a[i];\n            if (a->state == SALVO_DIVING/' check3 "every diver on the ship dying"
logic_plant ecap5 salvo_logic.c 's/g->neshot < SALVO_ESHOTS\) \{/g->neshot < 5) {/' check3 "a cap of 5 enemy shots"
logic_plant refire salvo_logic.c 's/if \(a->k == a->fire_at &&/if (a->fire_at >= 0 \&\& a->k >= a->fire_at \&\&/' check3 "a refused enemy shot tried again"
logic_plant xclamp salvo_logic.c 's/x > 387 \? 387 : x/x > 380 ? 380 : x/' check3 "a diver's x clamped at 380"
logic_plant slotfirst salvo_logic.c 's/&& \(hit < 0 \|\| ay > hit_y\)\) \{/\&\& (hit < 0 || ay > hit_y + 1000)) {/' check3 "a shot over two aliens taking the first in slot order"
logic_plant topearly salvo_logic.c 's/        g->shot\[s\].y = \(int16_t\)\(g->shot\[s\].y - 12\);/        if ((g->shot[s].y -= 12) + 6 <= SALVO_TOP) g->shot[s].x = -100;/' check3 "a shot leaving at the top gone before the hits"
logic_plant firefew salvo_logic.c 's/k < SALVO_KEYS; k\+\+\)/k < SALVO_K7; k++)/' check1 "7, GOLD, SOFT2 and SOFT5 held not firing"
logic_plant drawskip salvo_logic.c 's/int c = \(int\)salvo_draw\(g, SALVO_COLS\);/int c = any_alien(g) ? (int)salvo_draw(g, SALVO_COLS) : 0;/' check2 "the column drawn only while an alien is left"
app_plant() { # app_plant NAME PERL RUN WHAT: a plant in salvo_app.c, judged by check 4's run RUN
    if plant "$1" salvo_app.c "$2" && build_app x "$B/plant-$1" "$B/pa-$1"; then
        local ok_before=$ok caught=0 BLESS=0
        app_run "$B/pa-$1" "$3" app
        ok=1
        expect "$3" > /dev/null
        [ "$ok" = 0 ] && caught=1
        ok=$ok_before
        [ "$caught" = 1 ] && echo "  5 plant: $4: check 4 red" || red "5 plant: $4 passes check 4"
    fi
}
app_plant tailok 's/        if \(save\[i\]\)\n            return;/        if (save[i] \&\& 0)\n            return;/' midsave-tail "a save with a non-zero tail taken"
app_plant tail11 's/for \(int i = 10; i < SAVE_BYTES; i\+\+\)/for (int i = 11; i < SAVE_BYTES; i++)/' midsave-tail10 "a save with byte 10 set taken"
app_plant short11 's/if \(n < SAVE_BYTES \|\|/if (n < 11 ||/' midsave-short "a 15-byte save taken"
app_plant nomagic 's/save\[0\] != SAVE_MAGIC \|\| //' midsave-magic "a save with the wrong magic taken"
app_plant ask50 's/return SALVO_STEP_MS - c->g.acc;/return SALVO_STEP_MS;/' play "a tick asking 50 ms whatever is held"
app_plant early 's/uint32_t dt = c->now - c->started < ms \? c->now - c->started : ms;/uint32_t dt = ms;/' play "the first tick counting time before ENTER"
app_plant bestlate 's/    if \(g->score > c->best\)\n        c->best = g->score;\n//' midsave "the best not kept at each kill"
app_plant softkey 's/    case APP_KEY_SOFT6: return SALVO_KS6;\n//' play "SOFT6 not steering"
# Michael's layout (2026-10-23): each new fire key, and + left out
for k in 7:K7 GOLD:KG SOFT2:KS2 SOFT5:KS5; do
    app_plant "no${k%:*}" "s/    case APP_KEY_${k%:*}: return SALVO_${k#*:};\\n//" play "${k%:*} not firing"
done
app_plant plusfires 's/    default: return -1;/    case APP_KEY_PLUS: return SALVO_K8;\n    default: return -1;/' play "+ firing"
app_plant killtone 's/app_beep\(880, 20\);/app_beep(880, 30);/' play "the kill's beep 30 ms"
app_plant nostars 's/    stars\(f\);/    (void)stars;/' play "the stars not drawn"
app_plant tunecut 's/\} else if \(c->now >= c->tune_until\) \{/} else if (c->tune_until \|\| 1) {/' play "a click cutting a tune short"
app_plant again300 's/#define AGAIN_MS 500/#define AGAIN_MS 300/' play "GAME OVER taking ENTER at 0.3 s"
app_plant enternow 's/new_game\(c, ms\);\n        return true;\n    \}\n    return false;/new_game(c, c->state == TITLE ? ms : c->now);\n        return true;\n    }\n    return false;/' play "a game from GAME OVER counting time before its ENTER"
app_plant blink20 's/else if \(g->ship_t > 0 && g->ship_t % 4 < 2\)/else if (g->ship_t > 20 \&\& g->ship_t % 4 < 2)/' play "the ship's burst shown only for 20 steps"
app_plant enterplay 's/    if \(c->state == PLAY\) \{\n        salvo_key/    if (c->state == PLAY \&\& down \&\& key == APP_KEY_ENTER) {\n        new_game(c, ms);\n        return true;\n    }\n    if (c->state == PLAY) {\n        salvo_key/' play "ENTER restarting a game in play"

[ "$ok" = 1 ] && echo "salvocheck: GREEN" || { echo "salvocheck: RED"; exit 1; }
