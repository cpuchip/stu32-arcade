#!/bin/bash
# SPDX-License-Identifier: MIT
# MINES' checks (docs/mines.md, "The oracle"), all on the host:
#   1 the logic against oracle/mines/model.py: fuzzed opens and flags over 200 games, and 30 games won by opening only
#     the complete solver's safe cells (oracle/mines/proof.py play), every state line identical;
#   2 no board needs a guess: the C deals the same boards as the model for 400 seeds (their first opens), and the
#     complete solver clears every one; the 50/50 control must be reported as needing a guess; no random board the
#     game's solver clears may need one;
#   3 the rules at their edges (oracle/mines/edges.py), in the C and the model, against lines written by hand, and
#     every state line identical;
#   4 the app on the SDK runner: three scripted games (oracle/mines/appscript.py) with their frame hashes against
#     oracle/mines/frames/ and the save and the sound (each tone and tune, as -v prints them) the script's rules give,
#     and a short game with no save and with four damaged ones that must all be refused; with WASI_SDK, WAMRC and
#     WAMR_TREE set (as for oracle/wasm_check.sh), the same through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red; a plant that does not build is red too.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/minescheck
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/mines
G=$ROOT/games/mines
STRICT="-std=c11 -Wall -Wextra -Wpedantic -Werror"
SAN="$STRICT -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
red() { echo "RED: $*"; ok=0; }

for f in "$G/mines_logic.c" "$G/mines_app.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/mines_trace.c" "$2/mines_logic.c" || { red "build in $2"; return 1; }; }
build_app() { cc $SAN -DAPP_DEF=mines_app -I"$2" -I"$FW" -o "$3" "$ROOT/sdk/runner/apprun.c" "$2/mines_app.c" \
    "$2/mines_logic.c" "$FW/draw.c" "$FW/font5x7.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" && build_app x "$G" "$B/app" \
    || { echo "RED: build"; exit 1; }

(cd "$O" && python3 fuzz.py 40 5 && python3 proof.py play 30) > "$B/fuzz.txt"
(cd "$O" && python3 model.py "$B/fuzz.txt") > "$B/fuzz.py.txt"
(cd "$O" && python3 -c "
import proof
for s in range(400):
    print(f'game {s << 32} 0'); print(f'open {proof.first_cell(s)}')") > "$B/deals.txt"
(cd "$O" && python3 model.py "$B/deals.txt") > "$B/deals.py.txt"
(cd "$O" && python3 edges.py script) > "$B/edges.txt"
(cd "$O" && python3 edges.py expect) > "$B/edges.expect"
(cd "$O" && python3 model.py "$B/edges.txt") > "$B/edges.py.txt"
filt() { grep -E '^(open|flag|cell|mine|hit) ' | sed -E 's/^(open|flag) [0-9]+ state=([a-z]+).*/\2/'; }

# each run of the C has a time limit, so a plant that loops goes red instead of hanging the check
check1() { timeout 60 "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt"; }
check2_deals() { timeout 20 "$1" "$B/deals.txt" | cmp -s - "$B/deals.py.txt"; }
check3() { timeout 60 "$1" "$B/edges.txt" > "$B/edges.out" && filt < "$B/edges.out" | cmp -s - "$B/edges.expect" && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

check1 "$B/trace" && echo "  1 logic: $(grep -c '^game' "$B/fuzz.txt") games ($(grep -c 'state=won' "$B/fuzz.py.txt") moves in won states, $(grep -c '^open.*state=lost' "$B/fuzz.py.txt") in lost), $(wc -l < "$B/fuzz.py.txt") lines, identical to the model" \
    || red "1 logic: the C and the model differ"
if check2_deals "$B/trace_fast"; then
    echo "  2 deals: the C deals the model's boards for 400 seeds (mine checksums and attempts identical)"
else
    red "2 deals: the C deals other boards than the model"
fi
(cd "$O" && python3 proof.py deals 400) > "$B/proof.txt" && sed 's/^/  2 /' "$B/proof.txt" || { cat "$B/proof.txt"; red "2 proof: a dealt board needs a guess, or the game's solver cleared one that does"; }
(cd "$O" && python3 proof.py control control-5050.txt) > "$B/control.txt" && sed 's/^/  2 /' "$B/control.txt" \
    || { cat "$B/control.txt"; red "2 control: the 50/50 board was not reported as needing a guess"; }
check3 "$B/trace" && echo "  3 edges: $(grep -c '^# ' "$B/edges.txt") cases, the C and the model give the lines written by hand, every state line identical" \
    || red "3 edges: the C"
filt < "$B/edges.py.txt" | cmp -s - "$B/edges.expect" || red "3 edges: the model"

# 4: the app; each script names its seed and, last, what it must end with
[ "${1:-}" = --bless ] && BLESS=1
for s in play refuse cursor; do
    (cd "$O" && python3 appscript.py "$s") | cmp -s - "$O/scripts/app-$s.txt" || red "4 app: appscript.py $s differs from oracle/mines/scripts/app-$s.txt"
done
(cd "$O" && python3 appscript.py badload) > "$B/app-badload.txt"
xxd -r -p "$O/scripts/app-refuse.load.hex" > "$B/refuse.load"
good=$(cat "$O/scripts/app-refuse.load.hex")
printf '%s' "58${good:2}" | xxd -r -p > "$B/bad-magic.load"
printf '%s' "${good:0:2}02${good:4}" | xxd -r -p > "$B/bad-version.load"
printf '%s' "${good:0:28}" | xxd -r -p > "$B/bad-short.load"    # 14 bytes: wins and games whole, so only the length refuses it
printf '%s' "${good:0:12}09000000${good:20}" | xxd -r -p > "$B/bad-wins.load"
script_of() { case "$1" in badload*) echo "$B/app-badload.txt" ;; *) echo "$O/scripts/app-$1.txt" ;; esac; }
app_run() { # app_run RUNNER NAME SUFFIX -> $B/NAME.SUFFIX.{hashes,save,out}
    local seed=5 load=()
    case "$1:$2" in *:refuse) seed=9 load=(-load "$B/refuse.load") ;; *:cursor) seed=3 ;; *:badload) seed=9 ;;
        *:badload-*) seed=9 load=(-load "$B/bad-${2#badload-}.load") ;; esac
    timeout 120 "$1" "$(script_of "$2")" -seed $seed "${load[@]}" -v -keyframes -hashes "$B/$2.$3.hashes" -save "$B/$2.$3.save" > "$B/$2.$3.out" 2>&1
}
expect() { # expect NAME: the script's sound lines and its last line, '# expect save HEX tunes N tones N'
    local t s
    read -r _ _ _ save _ tunes _ tones < <(tail -1 "$(script_of "$1")")
    t=$(tail -1 "$B/$1.app.out")
    s=$(od -An -tx1 "$B/$1.app.save" | tr -d ' \n')
    case "$t" in *" $tones tones, $tunes tunes,"*) ;; *) red "4 app $1: '$t' (want $tunes tunes, $tones tones)"; return ;; esac
    [ "$s" = "$save" ] || { red "4 app $1: save $s, want $save"; return; }
    cmp -s <(grep -E '^(tone|tune) ' "$B/$1.app.out") <(grep '^# sound ' "$(script_of "$1")" | sed 's/^# sound //') \
        || { red "4 app $1: the sound differs from the script's"; return; }
    case "$1" in badload*) echo "  4 app $1: the save as expected (a fresh one: the damaged save refused)"; return ;; esac
    if [ "${BLESS:-0}" = 1 ]; then cp "$B/$1.app.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.app.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/mines/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.app.hashes") frames as committed; $tunes tunes, $tones tones, each as the script's; save as expected"
}
for s in play refuse cursor badload badload-magic badload-version badload-short badload-wins; do
    app_run "$B/app" "$s" app || red "4 app $s: the runner failed ($(tail -1 "$B/$s.app.out"))"
    expect "$s"
done
if [ -n "${WASI_SDK:-}" ] && [ -n "${WAMRC:-}" ] && [ -n "${WAMR_TREE:-}" ]; then
    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
        -Wno-missing-field-initializers -DAPP_DEF=mines_app -I"$G" -I"$FW" -Wl,--allow-undefined -Wl,-z,stack-size=65536 \
        -o "$B/mines.wasm" "$ROOT/sdk/wasm/guest.c" "$G/mines_app.c" "$G/mines_logic.c" "$FW/draw.c" "$FW/font5x7.c" \
        && "$WAMRC" --target=x86_64 -o "$B/mines-x86.aot" "$B/mines.wasm" > "$B/wamrc.log" 2>&1 || red "4 wasm: build"
    python3 - "$B/mines-x86.aot" "$B/mines_wasm_app.c" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    f.write('/* generated by oracle/mines_check.sh */\n#include "bridge.h"\nstatic const uint8_t mines_aot[] = {\n')
    for i in range(0, len(d), 16):
        f.write(" " + ",".join(str(b) for b in d[i:i + 16]) + ",\n")
    f.write('};\nWASM_GAME(mines_wasm_app, "MINES", mines_aot, mines_aot + sizeof mines_aot, 16);\n')
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
    cc $STRICT -O2 -DAPP_DEF=mines_wasm_app -I"$ROOT/sdk/wasm" -I"$FW" -I"$WAMR_TREE/core/iwasm/include" -o "$B/wasm" \
        "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/wasm/bridge.c" "$B/mines_wasm_app.c" "$B/vm/libvmlib.a" -lm -lpthread \
        || red "4 wasm: the runner"
    for s in play refuse cursor badload-wins; do
        app_run "$B/wasm" "$s" wasm
        if cmp -s "$B/$s.wasm.hashes" "$B/$s.app.hashes" && cmp -s "$B/$s.wasm.save" "$B/$s.app.save"; then
            echo "  4 wasm $s: the .wasm through the bridge gives the same $(wc -l < "$B/$s.app.hashes") frames and save"
        else red "4 wasm $s: frames or save differ from native"; fi
    done
else
    echo "  4 wasm: skipped (set WASI_SDK, WAMRC and WAMR_TREE to build MINES as a .wasm)"
fi

# 5: planted faults, each in a copy of the game's sources
plant() { # plant NAME FILE PERL -> $B/plant-NAME holding the games/mines copy
    local d="$B/plant-$1"
    mkdir -p "$d" && cp "$G"/* "$d/" && perl -0pi -e "$3" "$d/$2"
    cmp -s "$d/$2" "$G/$2" && { red "5 plant $1: the substitution matched nothing"; return 1; }
    return 0
}
if plant floodflag mines_logic.c 's/MINES_OPEN \| MINES_QUEUED \| stop/MINES_OPEN | MINES_QUEUED | (stop \& 0)/' && build_trace "$STRICT -O2" "$B/plant-floodflag" "$B/p1"; then
    check3 "$B/p1" && red "5 plant: a flood through flags passes check 3" || echo "  5 plant: a flood that opens flagged cells: check 3 red"
fi
if plant chordany mines_logic.c 's/        if \(flags != g->num\[c\]\) \/\*/        if (flags < g->num[c]) \/*/' \
    && build_trace "$STRICT -O2" "$B/plant-chordany" "$B/p2"; then
    check3 "$B/p2" && red "5 plant: a chord with too many flags passes check 3" || echo "  5 plant: a chord with too many flags: check 3 red"
fi
if plant firstmine mines_logic.c 's/            banned \|= c == nb\[i\];/            banned |= 0 \&\& c == nb[i];/' \
    && build_trace "$STRICT -O2" "$B/plant-firstmine" "$B/p3"; then
    check3 "$B/p3" && red "5 plant: mines beside the first open pass check 3" || echo "  5 plant: mines allowed beside the first open: check 3 red"
fi
if plant noB mines_logic.c 's/for \(int a = 0; a < MINES_CELLS && !done; a\+\+\)/for (int a = MINES_CELLS; a < MINES_CELLS \&\& !done; a++)/' && build_trace "$STRICT -O2" "$B/plant-noB" "$B/p4"; then
    check2_deals "$B/p4" && red "5 plant: a solver without rule B passes check 2" || echo "  5 plant: the game's solver without rule B: check 2 red (other boards dealt)"
fi
if plant loose mines_logic.c 's/if \(mines_solve\(g, f, NULL\) \|\| t == MINES_TRIES\)/if (1)/' && build_trace "$STRICT -O2" "$B/plant-loose" "$B/p5"; then
    check2_deals "$B/p5" && red "5 plant: dealing without the test passes check 2" || echo "  5 plant: boards dealt without the solver's test: check 2 red"
fi
if plant winconst mines_logic.c 's/g->opened == MINES_CELLS - g->mines\)/g->opened == MINES_CELLS - MINES_N)/' \
    && build_trace "$STRICT -O2" "$B/plant-winconst" "$B/p6"; then
    check3 "$B/p6" && red "5 plant: a win counted against 48 always passes check 3" || echo "  5 plant: the win counted against 48 whatever the field: check 3 red"
fi

app_plant() { # app_plant NAME PERL RUN WHAT: a plant in mines_app.c, judged by check 4's run RUN (it cannot bless)
    if plant "$1" mines_app.c "$2" && build_app x "$B/plant-$1" "$B/pa-$1"; then
        local ok_before=$ok caught=0 BLESS=0
        app_run "$B/pa-$1" "$3" app
        ok=1
        expect "$3" > /dev/null
        [ "$ok" = 0 ] && caught=1
        ok=$ok_before
        if [ -f "$O/frames/$3.hashes" ] && ! cmp -s "$B/$3.app.hashes" "$O/frames/$3.hashes"; then caught=1; fi
        [ "$caught" = 1 ] && echo "  5 plant: $4: check 4 red" || red "5 plant: $4 passes check 4"
    fi
}
app_plant repeat 's/#define REPEAT_NEXT 80 /#define REPEAT_NEXT 100 /' cursor "the cursor repeating every 100 ms"
app_plant edge 's/    if \(y >= 0 && y < MINES_H\)\n        c->cy = y;/    if (y >= 0 \&\& y <= MINES_H)\n        c->cy = y;/' cursor "the cursor past the bottom edge"
app_plant bestworse 's/if \(c->best == 0 \|\| t < c->best\)/if (c->best == 0 || t > c->best)/' refuse "a slower time taken as the best"
app_plant nowrong 's/        if \(over && g->state == MINES_LOST\) \{ \/\* a wrong flag \*\//        if (0) { \/* a wrong flag *\//' play "no cross on a wrong flag"
app_plant winsgames 's/ \|\| get32\(save \+ 6\) > get32\(save \+ 10\)\)/)/' badload-wins "a save with more wins than games taken"
if plant chordzero mines_logic.c 's/        if \(flags != g->num\[c\]\)/        if (g->num[c] == 0 || flags != g->num[c])/' \
    && build_trace "$STRICT -O2" "$B/plant-chordzero" "$B/p7"; then
    check3 "$B/p7" && red "5 plant: a chord on an opened 0 doing nothing passes check 3" || echo "  5 plant: a chord on an opened 0 that does nothing: check 3 red"
fi
if plant hitlast mines_logic.c 's/                if \(g->hit < 0\)\n                    g->hit = nb\[i\];/                g->hit = nb[i];/' \
    && build_trace "$STRICT -O2" "$B/plant-hitlast" "$B/p8"; then
    check3 "$B/p8" && red "5 plant: the last mine shown as hit passes check 3" || echo "  5 plant: a chord's last mine shown as the one opened: check 3 red"
fi
if plant loops mines_logic.c 's/        if \(done\)\n            continue;/        if (done || 1)\n            continue;/' \
    && build_trace "$STRICT -O2" "$B/plant-loops" "$B/p9"; then
    check2_deals "$B/p9" && red "5 plant: a solver that never stops passes check 2" || echo "  5 plant: a solver that never stops: check 2 red (timed out, not hung)"
fi
app_plant t0now 's/        c->t0 = ms; \/\* the time starts at the first open \*\//        c->t0 = c->now;/' refuse "the time from the tick, not the key"
app_plant noguard 's/    return t > 0 \? \(uint32_t\)t \/ 1000 : 0;/    return (uint32_t)t \/ 1000;/' play "a first open's frame with no guard on the time"
app_plant enterlate 's/ms - c->t_end >= AGAIN_MS/ms - c->t_end > AGAIN_MS/' play "ENTER refused at exactly 0.5 s"
app_plant besttie 's/if \(c->best == 0 \|\| t < c->best\)/if (c->best == 0 || t <= c->best)/' refuse "a tie taken as a new best"
app_plant upother 's/        if \(cursor && key == c->held\)\n            c->held = 0;/        if (cursor)\n            c->held = 0;/' cursor "any cursor key's up stopping the repeat"
app_plant shortlen 's/    if \(n < SAVE_BYTES \|\| /    if (n < 10 || /' badload-short "a 14-byte save taken"
app_plant noquiet 's/        if \(c->g.opened != opened\)\n            app_beep\(440, 10\);/        if (c->g.opened != opened || 1)\n            app_beep(440, 10);/' play "a tick for every open, even one that opens nothing"

[ "$ok" = 1 ] && echo "minescheck: GREEN" || { echo "minescheck: RED"; exit 1; }
