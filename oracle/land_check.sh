#!/bin/bash
# SPDX-License-Identifier: MIT
# LAND's checks (docs/land.md, "The oracle"), all on the host:
#   1 the logic against oracle/land/model.py: fuzzed key scripts over 200 rounds, the terrains and every step;
#   2 every pad is landable: the autopilot lands every pad of 1,000 terrains within 700 fuel (oracle/land/fair.py),
#     a control with 100 fuel must fail, and 300 of its flights replayed through the C must land, step for step;
#   3 the landing rule at its edges (oracle/land/edges.py), in the C and the model, against ends written by hand;
#   4 the app on the SDK runner: two scripted games (oracle/land/appscript.py) with their frame hashes against
#     oracle/land/frames/, and the save, tunes and tones the script's rules give; with WASI_SDK, WAMRC and WAMR_TREE
#     set (as for oracle/wasm_check.sh), the same scripts through the .wasm and the bridge, which must give the same;
#   5 planted faults, each of which must turn its check red (26, and a plant that does not build is red too).
# The logic and the app build under ASan and UBSan and the frame guard. --bless rewrites the frame hashes.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build/landcheck
FW=$ROOT/build/firmware/firmware
O=$ROOT/oracle/land
G=$ROOT/games/land
STRICT="-std=c11 -Wall -Wextra -Wpedantic -Werror"
SAN="$STRICT -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
make -s -C "$ROOT" build/firmware/.pin || exit 1
rm -rf "$B" && mkdir -p "$B"
ok=1
red() { echo "RED: $*"; ok=0; }

for f in "$G/land_logic.c" "$G/land_app.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/land_trace.c" "$2/land_logic.c" || { red "build in $2"; return 1; }; }
build_app() { cc $SAN -DAPP_DEF=land_app -I"$2" -I"$FW" -o "$3" "$ROOT/sdk/runner/apprun.c" "$2/land_app.c" \
    "$2/land_logic.c" "$FW/draw.c" "$FW/font5x7.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" && build_app x "$G" "$B/app" \
    || { echo "RED: build"; exit 1; }

(cd "$O" && python3 fuzz.py 40 5) > "$B/fuzz.txt"
(cd "$O" && python3 model.py "$B/fuzz.txt") > "$B/fuzz.py.txt"
(cd "$O" && python3 fair.py scripts 100) > "$B/flights.txt"
(cd "$O" && python3 model.py "$B/flights.txt") > "$B/flights.py.txt"
(cd "$O" && python3 edges.py script) > "$B/edges.txt"
(cd "$O" && python3 edges.py expect) > "$B/edges.expect"
ends() { grep -E '^(end|flying)' | sed 's/^end //'; }

check1() { "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt"; }
check2_replay() { "$1" "$B/flights.txt" | cmp -s - "$B/flights.py.txt"; }
check3() { "$1" "$B/edges.txt" | ends | cmp -s - "$B/edges.expect"; }

check1 "$B/trace" && echo "  1 logic: $(grep -c '^terrain' "$B/fuzz.py.txt") fuzzed rounds, $(wc -l < "$B/fuzz.py.txt") lines, identical to the model" \
    || red "1 logic: the C and the model differ on the fuzzed scripts"
(cd "$O" && python3 fair.py check 1000 700) > "$B/fair.txt" && sed 's/^/  2 /' "$B/fair.txt" || { cat "$B/fair.txt"; red "2 fair: a pad the autopilot could not land"; }
(cd "$O" && python3 fair.py check 30 100) > "$B/fair-control.txt" && red "2 control: 100 fuel passed the fairness check" \
    || echo "  2 control: with 100 fuel, $(head -1 "$B/fair-control.txt" | sed 's/^fair: //'): the check fails, as it must"
if check2_replay "$B/trace"; then
    n_land=$(grep -c '^end land' "$B/flights.py.txt")
    [ "$n_land" = 300 ] && echo "  2 replay: the autopilot's 300 flights through the C, identical, all 300 landed" || red "2 replay: $n_land of 300 landed"
else
    red "2 replay: the C and the model differ on the autopilot's flights"
fi
(cd "$O" && python3 model.py "$B/edges.txt") | ends | cmp -s - "$B/edges.expect" || red "3 edges: the model"
check3 "$B/trace" && echo "  3 edges: $(wc -l < "$B/edges.expect") cases, the C and the model give the ends written by hand" || red "3 edges: the C"

# 4: the app; each script names its seed and, last, what it must end with
[ "${1:-}" = --bless ] && BLESS=1
for s in play refuse; do
    (cd "$O" && python3 appscript.py "$s") | cmp -s - "$O/scripts/app-$s.txt" || red "4 app: appscript.py $s differs from oracle/land/scripts/app-$s.txt"
done
xxd -r -p "$O/scripts/app-refuse.load.hex" > "$B/refuse.load"
app_run() { # app_run RUNNER NAME SUFFIX -> $B/NAME.SUFFIX.{hashes,save,out}
    local seed=5 load=()
    [ "$2" = refuse ] && seed=9 load=(-load "$B/refuse.load")
    "$1" "$O/scripts/app-$2.txt" -seed $seed "${load[@]}" -hashes "$B/$2.$3.hashes" -save "$B/$2.$3.save" > "$B/$2.$3.out" 2>&1
}
expect() { # expect NAME: the script's last line, '# expect save HEX tunes N tones N'
    local want t s
    read -r _ _ _ save _ tunes _ tones < <(tail -1 "$O/scripts/app-$1.txt")
    t=$(tail -1 "$B/$1.app.out")
    s=$(od -An -tx1 "$B/$1.app.save" | tr -d ' \n')
    case "$t" in *" $tones tones, $tunes tunes,"*) ;; *) red "4 app $1: '$t' (want $tunes tunes, $tones tones)"; return ;; esac
    [ "$s" = "$save" ] || { red "4 app $1: save $s, want $save"; return; }
    if [ "${BLESS:-0}" = 1 ]; then cp "$B/$1.app.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.app.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/land/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.app.hashes") frames as committed; $tunes tunes, $tones tones; save as expected"
}
for s in play refuse; do
    app_run "$B/app" "$s" app || red "4 app $s: the runner failed ($(tail -1 "$B/$s.app.out"))"
    expect "$s"
done
if [ -n "${WASI_SDK:-}" ] && [ -n "${WAMRC:-}" ] && [ -n "${WAMR_TREE:-}" ]; then
    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 -mcpu=mvp -mexec-model=reactor -O2 -std=c11 -Wall -Wextra -Werror \
        -Wno-missing-field-initializers -DAPP_DEF=land_app -I"$G" -I"$FW" -Wl,--allow-undefined -Wl,-z,stack-size=65536 \
        -o "$B/land.wasm" "$ROOT/sdk/wasm/guest.c" "$G/land_app.c" "$G/land_logic.c" "$FW/draw.c" "$FW/font5x7.c" \
        && "$WAMRC" --target=x86_64 -o "$B/land-x86.aot" "$B/land.wasm" > "$B/wamrc.log" 2>&1 || red "4 wasm: build"
    python3 - "$B/land-x86.aot" "$B/land_wasm_app.c" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    f.write('/* generated by oracle/land_check.sh */\n#include "bridge.h"\nstatic const uint8_t land_aot[] = {\n')
    for i in range(0, len(d), 16):
        f.write(" " + ",".join(str(b) for b in d[i:i + 16]) + ",\n")
    f.write('};\nWASM_GAME(land_wasm_app, "LAND", land_aot, land_aot + sizeof land_aot, 16);\n')
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
    cc $STRICT -O2 -DAPP_DEF=land_wasm_app -I"$ROOT/sdk/wasm" -I"$FW" -I"$WAMR_TREE/core/iwasm/include" -o "$B/wasm" \
        "$ROOT/sdk/runner/apprun.c" "$ROOT/sdk/wasm/bridge.c" "$B/land_wasm_app.c" "$B/vm/libvmlib.a" -lm -lpthread \
        || red "4 wasm: the runner"
    for s in play refuse; do
        app_run "$B/wasm" "$s" wasm
        if cmp -s "$B/$s.wasm.hashes" "$O/frames/$s.hashes" && cmp -s "$B/$s.wasm.save" "$B/$s.app.save"; then
            echo "  4 wasm $s: the .wasm through the bridge gives the same $(wc -l < "$O/frames/$s.hashes") frames and save"
        else red "4 wasm $s: frames or save differ from native"; fi
    done
else
    echo "  4 wasm: skipped (set WASI_SDK, WAMRC and WAMR_TREE to build LAND as a .wasm)"
fi

# 5: planted faults, each in a copy of the game's sources
plant() { # plant NAME FILE PERL -> $B/plant-NAME holding the games/land copy
    local d="$B/plant-$1"
    mkdir -p "$d" && cp "$G"/* "$d/" && perl -0pi -e "$3" "$d/$2"
    cmp -s "$d/$2" "$G/$2" && { red "5 plant $1: the substitution matched nothing"; return 1; }
    return 0
}
if plant nogravity land_logic.c 's/\n    s->vy \+= LAND_G;\n/\n/' && build_trace "$STRICT -O2" "$B/plant-nogravity" "$B/p1"; then
    check1 "$B/p1" && red "5 plant: no gravity passes check 1" || echo "  5 plant: gravity left out: check 1 red"
fi
if plant angle land_logic.c 's/if \(s->a != 0 \|\| /if (/' && build_trace "$STRICT -O2" "$B/plant-angle" "$B/p2"; then
    check3 "$B/p2" && red "5 plant: a judge ignoring the angle passes check 3" || echo "  5 plant: the judge ignoring the angle: check 3 red"
fi
if plant novertex land_logic.c 's/bool poke = vertex_inside\(pts, t\);/bool poke = vertex_inside(pts, t) \&\& false;/' \
    && build_trace "$STRICT -O2" "$B/plant-novertex" "$B/p3"; then
    check3 "$B/p3" && red "5 plant: no vertex test passes check 3" || echo "  5 plant: the vertex test left out: check 3 red"
fi
if plant tilt land_logic.c 's/t->v\[t->n\+\+\] = \(land_vert\)\{t->pad\[k\].x1, t->pad\[k\].y\};/t->v[t->n++] = (land_vert){t->pad[k].x1, (int16_t)(t->pad[k].y + 1)};/' \
    && build_trace "$STRICT -O2" "$B/plant-tilt" "$B/p4"; then
    check1 "$B/p4" && red "5 plant: a pad that is not flat passes check 1" || echo "  5 plant: a pad that is not flat: check 1 red"
fi
if plant nowrap land_logic.c 's/s->x = \(s->x \+ s->vx\) % WQ;\n    if \(s->x < 0\)\n        s->x \+= WQ;/s->x = s->x + s->vx;/' \
    && build_trace "$STRICT -O2" "$B/plant-nowrap" "$B/p5"; then
    check1 "$B/p5" && red "5 plant: no wrap passes check 1" || echo "  5 plant: no wrap at the edges: check 1 red"
fi

if plant noclamp land_logic.c 's/\n    s->vy = clamp\(s->vy, -VMAX, VMAX\);\n/\n/' && build_trace "$STRICT -O2" "$B/plant-noclamp" "$B/p7"; then
    check1 "$B/p7" && red "5 plant: no speed clamp passes check 1" || echo "  5 plant: the speed clamp left out: check 1 red (the dives)"
fi
if plant floor land_logic.c 's/return y0 \* LAND_Q \+ \(int32_t\)\(\(int64_t\)\(y1 - y0\) \* \(xq - x0 \* LAND_Q\) \/ \(x1 - x0\)\);/int64_t num = (int64_t)(y1 - y0) * (xq - x0 * LAND_Q), q = num \/ (x1 - x0);\n    return y0 * LAND_Q + (int32_t)(q * (x1 - x0) > num ? q - 1 : q);/' \
    && build_trace "$STRICT -O2" "$B/plant-floor" "$B/p8"; then
    check3 "$B/p8" && red "5 plant: a floored ground passes check 3" || echo "  5 plant: the ground floored, not rounded toward 0: check 3 red"
fi
if plant upward land_logic.c 's/ \|\| s->vy < -LAND_VY//' && build_trace "$STRICT -O2" "$B/plant-upward" "$B/p9"; then
    check3 "$B/p9" && red "5 plant: no upward speed limit passes check 3" || echo "  5 plant: the upward speed limit left out: check 3 red"
fi
if plant thin land_app.c 's/p->x1 - p->x0 \+ 1, 3, DRAW_DARK\)/p->x1 - p->x0 + 1, 1, DRAW_DARK)/' && build_app x "$B/plant-thin" "$B/p6"; then
    app_run "$B/p6" play p6
    cmp -s "$B/play.p6.hashes" "$O/frames/play.hashes" && red "5 plant: thin pads pass check 4" || echo "  5 plant: pads drawn 1 px thick: check 4 red"
fi
# the keys (docs/land.md, "The keys") and ENTER by the key's time: each against the app scripts' frames
app_plant() { # app_plant NAME PERL SCRIPT WHAT
    if plant "$1" land_app.c "$2" && build_app x "$B/plant-$1" "$B/p-$1"; then
        app_run "$B/p-$1" "$3" "p-$1"
        cmp -s "$B/$3.p-$1.hashes" "$O/frames/$3.hashes" && red "5 plant: $4 passes check 4" || echo "  5 plant: $4: check 4 red"
    fi
}
for k in 4:LEFT SOFT1:LEFT 6:RIGHT SOFT6:RIGHT 8:THRUST 7:THRUST 9:THRUST TIMES:THRUST GOLD:THRUST SOFT2:THRUST \
         SOFT5:THRUST; do
    app_plant "no${k%:*}" "s/\\{APP_KEY_${k%:*}, +LAND_KEY_${k#*:}\\},//" play "${k%:*} not reaching the ship"
done
app_plant release 's/if \(down \|\| !ship_held\(c, SHIP_KEYS\[i\].ship\)\)/if (down || !ship_held(c, 0))/' play \
    "either key's release letting go a ship key held by two"
app_plant firstdown 's/c->keys_down = \(uint16_t\)/bool was = ship_held(c, SHIP_KEYS[i].ship);\n        c->keys_down = (uint16_t)/; s/if \(down \|\| !ship_held\(c, SHIP_KEYS\[i\].ship\)\)/if (was != ship_held(c, SHIP_KEYS[i].ship))/' \
    play "a ship key's down only with the first of its keys"
app_plant mask8 's/uint16_t keys_down;/uint8_t keys_down; /; s/c->keys_down = \(uint16_t\)/c->keys_down = (uint8_t)/' play \
    "a key mask of 8 bits (GOLD, SOFT2 and SOFT5 lost)"
app_plant turnrelease 's/if \(down \|\| !ship_held\(c, SHIP_KEYS\[i\].ship\)\)/if (down || !ship_held(c, SHIP_KEYS[i].ship) || SHIP_KEYS[i].ship != LAND_KEY_THRUST)/' \
    play "either turn key's release letting go a turn held by two"
app_plant plus 's/\{APP_KEY_SOFT5, LAND_KEY_THRUST\},/{APP_KEY_SOFT5, LAND_KEY_THRUST}, {APP_KEY_PLUS, LAND_KEY_THRUST},/' play \
    "+ firing the engine"
app_plant tickenter 's/c->state == OVER \&\& ms - c->end_at/c->state == OVER \&\& c->now - c->end_at/' refuse \
    "ENTER judged by the last tick's time"

[ "$ok" = 1 ] && echo "landcheck: GREEN" || { echo "landcheck: RED"; exit 1; }
