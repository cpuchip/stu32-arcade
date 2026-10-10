#!/bin/bash
# SPDX-License-Identifier: MIT
# AIM's checks (docs/aim.md, "The oracle"), all on the host:
#   1 the solver against oracle/aim/reach.py, every set of four digits, targets 1-100 with their counts;
#   2 the rules against oracle/aim/rules.py: the hand script, 40 fuzzed seeds, and every SHOW answer (all must win);
#   3 every round the generator makes for seeds 0-9,999 at each level, judged by oracle/aim/rounds.py;
#   4 the app on the SDK runner: two scripts' frame hashes against oracle/aim/frames/, their tunes, tones and saves;
#     with WASI_SDK, WAMRC and WAMR_TREE set (as for oracle/wasm_check.sh), the same scripts through the .wasm and
#     the bridge, which must give the same frames;
#   5 planted faults, each of which must turn its check red.
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/aimcheck
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/aim
G=$ROOT/games/aim
STRICT="-std=c11 -Wall -Wextra -Wpedantic -Werror"
SAN="$STRICT -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
red() { echo "RED: $*"; ok=0; }

for f in "$G/aim_logic.c" "$G/aim_app.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
# trace: the logic's driver; trace_fast: the same at -O2, for the long checks
build_trace() { cc $1 -I"$2" -I"$FW" -o "$3" "$ROOT/tests/aim_trace.c" "$2/aim_logic.c"; }
build_app() { cc $SAN -DAPP_DEF=aim_app -I"$2" -I"$FW" -o "$3" "$ROOT/sdk/runner/apprun.c" "$2/aim_app.c" \
    "$2/aim_logic.c" "$FW/draw.c" "$FW/font5x7.c"; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" && build_app x "$G" "$B/app" \
    || { echo "RED: build"; exit 1; }
python3 "$O/reach.py" --lo 1 --hi 100 > "$B/reach.py.txt"
python3 "$O/rules.py" "$O/scripts/basic.txt" > "$B/basic.py.txt"

check1() { "$1" reach 1 100 | cmp -s - "$B/reach.py.txt"; }
check2_basic() { "$1" trace "$O/scripts/basic.txt" | cmp -s - "$B/basic.py.txt"; }
check3() { "$1" rounds 10000 1 | (cd "$O" && python3 rounds.py) > "$B/rounds.txt"; }

check1 "$B/trace" && echo "  1 solver: the C and reach.py agree on all 495 sets (targets 1-100, counts)" || red "1 solver"
check2_basic "$B/trace" && echo "  2 rules: the hand script, $(wc -l < "$B/basic.py.txt") keys, identical" || red "2 rules, hand script"
fz=0
for seed in $(seq 1 40); do
    python3 "$O/rules.py" --fuzz "$seed" 250 > "$B/fz.txt"
    "$B/trace" trace "$B/fz.txt" | cmp -s - <(python3 "$O/rules.py" "$B/fz.txt") || { fz=$((fz + 1)); echo "    fuzz seed $seed differs"; }
done
[ "$fz" = 0 ] && echo "  2 rules: 40 fuzzed seeds of 250 rounds, identical" || red "2 rules: $fz fuzzed seeds differ"
"$B/trace_fast" show 1 100 > "$B/show.txt"
python3 "$O/rules.py" "$B/show.txt" > "$B/show.py.txt"
n_show=$(grep -c '^round' "$B/show.txt")
won=$(python3 - "$B/show.txt" "$B/show.py.txt" <<'PY'
import sys
script = [l.split() for l in open(sys.argv[1])]
out = open(sys.argv[2]).read().splitlines()
won = k = 0
last = None
for w in script:
    if w[0] == "round":
        won += last or 0
        last = 0
    else:
        last = int(out[k].endswith("won=1"))
        k += 1
print(won + (last or 0))
PY
)
if [ "$won" = "$n_show" ] && "$B/trace" trace "$B/show.txt" | cmp -s - "$B/show.py.txt"; then
    echo "  2 rules: every SHOW answer ($n_show sets and targets) wins, in the C and in rules.py alike"
else red "2 rules: SHOW answers won $won of $n_show, or the C and rules.py differ on them"; fi
check3 "$B/trace_fast" && echo "  3 rounds: $(tail -1 "$B/rounds.txt")" || red "3 rounds: $(tail -1 "$B/rounds.txt")"

# 4: the app. win12 wins 12 rounds by their SHOW keys (seed 7); app-refuse presses each refused key once.
"$B/trace" appscript 7 12 > "$B/win12.txt"
cmp -s "$B/win12.txt" "$O/scripts/app-win12.txt" || red "4 app: aim_trace appscript 7 12 differs from oracle/aim/scripts/app-win12.txt"
app_run() { # app_run RUNNER NAME -> hashes, out, save in $B/NAME.*
    "$1" "$O/scripts/app-$2.txt" -seed 7 -v -hashes "$B/$2.hashes" -save "$B/$2.save" > "$B/$2.out" 2>&1
}
expect() { # expect NAME TUNES TONES SAVE_HEX
    local t s
    t=$(tail -1 "$B/$1.out")
    s=$(od -An -tx1 "$B/$1.save" | tr -d ' \n')
    case "$t" in *" $3 tones, $2 tunes,"*) ;; *) red "4 app $1: '$t' (want $2 tunes, $3 tones)"; return ;; esac
    [ "$s" = "$4" ] || { red "4 app $1: save $s, want $4"; return; }
    if [ "${BLESS:-0}" = 1 ]; then cp "$B/$1.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/aim/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.hashes") frames as committed; $2 tunes, $3 tones; save as expected"
}
[ "${1:-}" = --bless ] && BLESS=1 && mkdir -p "$O/frames"
for s in win12 refuse; do app_run "$B/app" "$s" || red "4 app $s: the runner failed ($(tail -1 "$B/$s.out"))"; done
expect win12 12 0 41010c000c0000000000000002000000
expect refuse 0 6 41010000000000000100000000000000
if [ -n "${WASI_SDK:-}" ] && [ -n "${WAMRC:-}" ] && [ -n "${WAMR_TREE:-}" ]; then
    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
        -Wno-missing-field-initializers -DAPP_DEF=aim_app -I"$G" -I"$FW" -Wl,--allow-undefined -Wl,-z,stack-size=65536 \
        -o "$B/aim.wasm" "$ROOT/sdk/wasm/guest.c" "$G/aim_app.c" "$G/aim_logic.c" "$FW/draw.c" "$FW/font5x7.c" \
        && "$WAMRC" --target=x86_64 -o "$B/aim-x86.aot" "$B/aim.wasm" > "$B/wamrc.log" 2>&1 || red "4 wasm: build"
    python3 - "$B/aim-x86.aot" "$B/aim_wasm_app.c" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    f.write('/* generated by oracle/aim_check.sh */\n#include "bridge.h"\nstatic const uint8_t aim_aot[] = {\n')
    for i in range(0, len(d), 16):
        f.write(" " + ",".join(str(b) for b in d[i:i + 16]) + ",\n")
    f.write('};\nWASM_GAME(aim_wasm_app, "AIM", aim_aot, aim_aot + sizeof aim_aot, 16);\n')
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
    cc $STRICT -O2 -DAPP_DEF=aim_wasm_app -I"$ROOT/sdk/wasm" -I"$FW" -I"$WAMR_TREE/core/iwasm/include" -o "$B/wasm" \
        "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/wasm/bridge.c" "$B/aim_wasm_app.c" "$B/vm/libvmlib.a" -lm -lpthread \
        || red "4 wasm: the runner"
    for s in win12 refuse; do
        "$B/wasm" "$O/scripts/app-$s.txt" -seed 7 -hashes "$B/$s.wasm.hashes" -save "$B/$s.wasm.save" > "$B/$s.wasm.out" 2>&1
        if cmp -s "$B/$s.wasm.hashes" "$O/frames/$s.hashes" && cmp -s "$B/$s.wasm.save" "$B/$s.save"; then
            echo "  4 wasm $s: the .wasm through the bridge gives the same $(wc -l < "$B/$s.hashes") frames and save"
        else red "4 wasm $s: frames or save differ from native"; fi
    done
else
    echo "  4 wasm: skipped (set WASI_SDK, WAMRC and WAMR_TREE to build AIM as a .wasm)"
fi

# 5: planted faults, each in a copy of the game's sources
plant() { # plant NAME FILE PERL -> $B/plant-NAME holding the games/aim copy
    local d="$B/plant-$1"
    mkdir -p "$d" && cp "$G"/* "$d/" && perl -0pi -e "$3" "$d/$2"
    cmp -s "$d/$2" "$G/$2" && { red "5 plant $1: the substitution matched nothing"; return 1; }
    return 0
}
if plant nodiv aim_logic.c 's/op <= AIM_DIVIDE; op\+\+\) \{\n(\s+)aim_frac v;/op <= AIM_TIMES; op++) {\n$1aim_frac v;/' \
    && build_trace "$STRICT -O2" "$B/plant-nodiv" "$B/p1"; then
    check1 "$B/p1" && red "5 plant: a solver that never divides passes check 1" || echo "  5 plant: a solver that never divides: check 1 red"
fi
if plant reuse aim_logic.c 's/if \(!\(r->used >> i & 1\) && r->tiles\[i\] == n\)/if (r->tiles[i] == n)/' \
    && build_trace "$STRICT -O2" "$B/plant-reuse" "$B/p2"; then
    check2_basic "$B/p2" && red "5 plant: a tile used twice passes check 2" || echo "  5 plant: a tile used twice: check 2 red"
fi
if plant range aim_logic.c 's/int hi = aim_level_hi\(level\), nc = 0;/int hi = 70, nc = 0;/' \
    && build_trace "$STRICT -O2" "$B/plant-range" "$B/p3"; then
    check3 "$B/p3" && red "5 plant: a generator past its range passes check 3" || echo "  5 plant: a generator past its range: check 3 red"
fi
if plant shade aim_app.c 's/if \(used\)\n(\s+)draw_fill\(f, x, TILE_Y, TILE_W, TILE_W, DRAW_HALF\);/if (0)\n$1draw_fill(f, x, TILE_Y, TILE_W, TILE_W, DRAW_HALF);/' \
    && build_app x "$B/plant-shade" "$B/p4"; then
    "$B/p4" "$O/scripts/app-win12.txt" -seed 7 -hashes "$B/p4.hashes" > /dev/null 2>&1
    cmp -s "$B/p4.hashes" "$O/frames/win12.hashes" && red "5 plant: a draw without the used-tile shade passes check 4" \
        || echo "  5 plant: a draw without the used-tile shade: check 4 red"
fi

[ "$ok" = 1 ] && echo "aimcheck: all green" || { echo "aimcheck: RED"; exit 1; }
