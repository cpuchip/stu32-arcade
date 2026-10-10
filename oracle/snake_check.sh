#!/bin/bash
# SPDX-License-Identifier: MIT
# SNAKE's checks (docs/snake.md, "The oracle"), all on the host:
#   1 the logic against oracle/snake/model.py: fuzzed key scripts over 200 games, every key and step identical;
#   2 a full game: the Hamiltonian-cycle autopilot (oracle/snake/cycle.py) for three seeds, the C and the model
#     identical and each won at length 880;
#   3 the rules at their edges (oracle/snake/edges.py), in the C and the model, against lines written by hand;
#   4 the app on the SDK runner: two scripted games (oracle/snake/appscript.py) with their frame hashes against
#     oracle/snake/frames/ and the save, tunes and tones the script's rules give, and a third that fills the grid in
#     the app, judged by those alone (210,367 ticks, too many frames to commit); with WASI_SDK, WAMRC and WAMR_TREE set
#     (as for oracle/wasm_check.sh), the same scripts through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red; a plant that does not build is red too.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/snakecheck
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/snake
G=$ROOT/games/snake
STRICT="-std=c11 -Wall -Wextra -Wpedantic -Werror"
SAN="$STRICT -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
red() { echo "RED: $*"; ok=0; }

for f in "$G/snake_logic.c" "$G/snake_app.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/snake_trace.c" "$2/snake_logic.c" || { red "build in $2"; return 1; }; }
build_app() { cc $SAN -DAPP_DEF=snake_app -I"$2" -I"$FW" -o "$3" "$ROOT/sdk/runner/apprun.c" "$2/snake_app.c" \
    "$2/snake_logic.c" "$FW/draw.c" "$FW/font5x7.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" && build_app x "$G" "$B/app" \
    || { echo "RED: build"; exit 1; }

(cd "$O" && python3 fuzz.py 40 5) > "$B/fuzz.txt"
(cd "$O" && python3 model.py "$B/fuzz.txt") > "$B/fuzz.py.txt"
SEEDS="$((3 << 32)) $((11 << 32)) 0"
for s in $SEEDS; do
    (cd "$O" && python3 cycle.py script "$s" 0) > "$B/cycle-$s.txt"
    (cd "$O" && python3 model.py "$B/cycle-$s.txt") > "$B/cycle-$s.py.txt"
done
(cd "$O" && python3 edges.py script) > "$B/edges.txt"
(cd "$O" && python3 edges.py expect) > "$B/edges.expect"
filt() { grep -E '^(key |end |flying|head |interval )' | sed -E 's/^(key [0-9] [a-z]+) queue=.*/\1/'; }

check1() { "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt"; }
check2() {
    for s in $SEEDS; do
        "$1" "$B/cycle-$s.txt" | cmp -s - "$B/cycle-$s.py.txt" || return 1
        tail -1 "$B/cycle-$s.py.txt" | grep -qx 'end won' || return 1
        tail -2 "$B/cycle-$s.py.txt" | head -1 | grep -q ' len=880 ' || return 1
    done
}
check3() { "$1" "$B/edges.txt" | filt | cmp -s - "$B/edges.expect"; }

check1 "$B/trace" && echo "  1 logic: $(grep -c '^0 ' "$B/fuzz.py.txt") fuzzed games, $(wc -l < "$B/fuzz.py.txt") lines, identical to the model" \
    || red "1 logic: the C and the model differ on the fuzzed scripts"
if check2 "$B/trace_fast"; then
    echo "  2 full game: 3 seeds, each won at length 880 (876 eaten), $(cat "$B"/cycle-*.py.txt | grep -c '^[0-9]') steps in all, the C and the model identical"
else
    red "2 full game: a seed differs, or did not end won at 880"
fi
(cd "$O" && python3 model.py "$B/edges.txt") | filt | cmp -s - "$B/edges.expect" || red "3 edges: the model"
check3 "$B/trace" && echo "  3 edges: $(grep -c '^# ' "$B/edges.txt") cases, the C and the model give the lines written by hand" || red "3 edges: the C"

# 4: the app; each script names its seed and, last, what it must end with
[ "${1:-}" = --bless ] && BLESS=1
for s in play refuse; do
    (cd "$O" && python3 appscript.py "$s") | cmp -s - "$O/scripts/app-$s.txt" || red "4 app: appscript.py $s differs from oracle/snake/scripts/app-$s.txt"
done
(cd "$O" && python3 appscript.py full) > "$B/app-full.txt"
xxd -r -p "$O/scripts/app-refuse.load.hex" > "$B/refuse.load"
script_of() { [ "$1" = full ] && echo "$B/app-full.txt" || echo "$O/scripts/app-$1.txt"; }
app_run() { # app_run RUNNER NAME SUFFIX -> $B/NAME.SUFFIX.{hashes,save,out}
    local seed=5 load=()
    [ "$2" = refuse ] && seed=9 load=(-load "$B/refuse.load")
    [ "$2" = full ] && seed=3
    "$1" "$(script_of "$2")" -seed $seed "${load[@]}" -hashes "$B/$2.$3.hashes" -save "$B/$2.$3.save" > "$B/$2.$3.out" 2>&1
}
expect() { # expect NAME: the script's last line, '# expect save HEX tunes N tones N'
    local t s
    read -r _ _ _ save _ tunes _ tones < <(tail -1 "$(script_of "$1")")
    t=$(tail -1 "$B/$1.app.out")
    s=$(od -An -tx1 "$B/$1.app.save" | tr -d ' \n')
    case "$t" in *" $tones tones, $tunes tunes,"*) ;; *) red "4 app $1: '$t' (want $tunes tunes, $tones tones)"; return ;; esac
    [ "$s" = "$save" ] || { red "4 app $1: save $s, want $save"; return; }
    if [ "$1" = full ]; then
        echo "  4 app full: $(wc -l < "$B/full.app.hashes") ticks to FULL; $tunes tune, $tones tones; save as expected (best 876)"
        return
    fi
    if [ "${BLESS:-0}" = 1 ]; then cp "$B/$1.app.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.app.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/snake/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.app.hashes") frames as committed; $tunes tunes, $tones tones; save as expected"
}
for s in play refuse full; do
    app_run "$B/app" "$s" app || red "4 app $s: the runner failed ($(tail -1 "$B/$s.app.out"))"
    expect "$s"
done
if [ -n "${WASI_SDK:-}" ] && [ -n "${WAMRC:-}" ] && [ -n "${WAMR_TREE:-}" ]; then
    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
        -Wno-missing-field-initializers -DAPP_DEF=snake_app -I"$G" -I"$FW" -Wl,--allow-undefined -Wl,-z,stack-size=65536 \
        -o "$B/snake.wasm" "$ROOT/sdk/wasm/guest.c" "$G/snake_app.c" "$G/snake_logic.c" "$FW/draw.c" "$FW/font5x7.c" \
        && "$WAMRC" --target=x86_64 -o "$B/snake-x86.aot" "$B/snake.wasm" > "$B/wamrc.log" 2>&1 || red "4 wasm: build"
    python3 - "$B/snake-x86.aot" "$B/snake_wasm_app.c" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    f.write('/* generated by oracle/snake_check.sh */\n#include "bridge.h"\nstatic const uint8_t snake_aot[] = {\n')
    for i in range(0, len(d), 16):
        f.write(" " + ",".join(str(b) for b in d[i:i + 16]) + ",\n")
    f.write('};\nWASM_GAME(snake_wasm_app, "SNAKE", snake_aot, snake_aot + sizeof snake_aot, 16);\n')
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
    cc $STRICT -O2 -DAPP_DEF=snake_wasm_app -I"$ROOT/sdk/wasm" -I"$FW" -I"$WAMR_TREE/core/iwasm/include" -o "$B/wasm" \
        "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/wasm/bridge.c" "$B/snake_wasm_app.c" "$B/vm/libvmlib.a" -lm -lpthread \
        || red "4 wasm: the runner"
    for s in play refuse full; do
        app_run "$B/wasm" "$s" wasm
        if cmp -s "$B/$s.wasm.hashes" "$B/$s.app.hashes" && cmp -s "$B/$s.wasm.save" "$B/$s.app.save"; then
            echo "  4 wasm $s: the .wasm through the bridge gives the same $(wc -l < "$B/$s.app.hashes") frames and save"
        else red "4 wasm $s: frames or save differ from native"; fi
    done
else
    echo "  4 wasm: skipped (set WASI_SDK, WAMRC and WAMR_TREE to build SNAKE as a .wasm)"
fi

# 5: planted faults, each in a copy of the game's sources
plant() { # plant NAME FILE PERL -> $B/plant-NAME holding the games/snake copy
    local d="$B/plant-$1"
    mkdir -p "$d" && cp "$G"/* "$d/" && perl -0pi -e "$3" "$d/$2"
    cmp -s "$d/$2" "$G/$2" && { red "5 plant $1: the substitution matched nothing"; return 1; }
    return 0
}
if plant tail snake_logic.c 's/if \(snake_has\(g, next\) && next != tail\)/if (snake_has(g, next))/' \
    && build_trace "$STRICT -O2" "$B/plant-tail" "$B/p1"; then
    check3 "$B/p1" && red "5 plant: the tail freed after the body test passes check 3" || echo "  5 plant: the tail freed after the body test: check 3 red"
fi
if plant reverse snake_logic.c 's/ \|\| dir == \(last \+ 2\) % 4\)/)/' && build_trace "$STRICT -O2" "$B/plant-reverse" "$B/p2"; then
    check3 "$B/p2" && red "5 plant: a reverse key taken passes check 3" || echo "  5 plant: a reverse key taken: check 3 red"
fi
if plant allcells snake_logic.c 's/if \(!snake_has\(g, c\) && k-- == 0\)/if (k-- == 0)/' \
    && build_trace "$STRICT -O2" "$B/plant-allcells" "$B/p3"; then
    check2 "$B/p3" && red "5 plant: food over all cells passes check 2" || echo "  5 plant: food counted over all cells: check 2 red"
fi
if plant queue1 snake_logic.h 's/#define SNAKE_QUEUE 2/#define SNAKE_QUEUE 1/' && build_trace "$STRICT -O2" "$B/plant-queue1" "$B/p4"; then
    check3 "$B/p4" && red "5 plant: a queue of 1 passes check 3" || echo "  5 plant: a queue of 1: check 3 red"
fi
if plant wall snake_logic.c 's/ \|\| x >= SNAKE_W \|\| / || /' && build_trace "$STRICT -O2" "$B/plant-wall" "$B/p5"; then
    check3 "$B/p5" && red "5 plant: no right wall passes check 3" || echo "  5 plant: the right wall missing: check 3 red"
fi
if plant interval snake_logic.c 's/return eaten >= 18 \? 60 : 150 - 5 \* eaten;/return 150 + 0 * eaten;/' \
    && build_trace "$STRICT -O2" "$B/plant-interval" "$B/p6"; then
    check3 "$B/p6" && red "5 plant: a fixed interval passes check 3" || echo "  5 plant: the interval never shortening: check 3 red"
fi

if plant nofood snake_app.c 's/if \(g->food >= 0\) \{/if (g->food >= 0 \&\& 0) {/' && build_app x "$B/plant-nofood" "$B/p7"; then
    app_run "$B/p7" play p7
    cmp -s "$B/play.p7.hashes" "$O/frames/play.hashes" && red "5 plant: food not drawn passes check 4" || echo "  5 plant: the food not drawn: check 4 red"
fi

[ "$ok" = 1 ] && echo "snakecheck: GREEN" || { echo "snakecheck: RED"; exit 1; }
