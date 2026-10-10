#!/bin/bash
# SPDX-License-Identifier: MIT
# MINES' checks (docs/mines.md, "The oracle"), all on the host, on each board (easy 9 x 9, medium 16 x 16, hard
# 30 x 16):
#   1 the logic against oracle/mines/model.py: fuzzed opens, flags and deals a few attempts at a time over 240 games
#     on every board and both rules, and 30 games won by opening only the complete solver's safe cells (oracle/mines/
#     proof.py play), every state line identical;
#   2 no board needs a guess: the C deals the same boards as the model for 400 seeds a board (their first opens), and
#     the complete solver clears every one; with classic rules every deal is the first attempt; built with the cap at 3
#     attempts, the C and the model deal the same boards, the fallback among them; the complete solver's
#     two ways of tabling a group agree; the 50/50 control must be reported as needing a guess; no random board the
#     game's solver clears may need one;
#   3 the rules at their edges (oracle/mines/edges.py), in the C and the model, against lines written by hand, and
#     every state line identical;
#   4 the app on the SDK runner: scripted games (oracle/mines/appscript.py) with their frame hashes against
#     oracle/mines/frames/ and the save, the sound (each tone and tune, as -v prints them) and what each tick asks for
#     (the runner's -nexts) the script's rules give, and a short game with no save and with damaged ones that must all
#     be refused; with WASI_SDK, WAMRC and
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
BOARDS="easy medium hard"
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

(cd "$O" && python3 fuzz.py 40 6 && for b in $BOARDS; do python3 proof.py play $b 10; done) > "$B/fuzz.txt"
(cd "$O" && python3 model.py "$B/fuzz.txt") > "$B/fuzz.py.txt"
(cd "$O" && python3 -c "
import model, proof
for b in '$BOARDS'.split():
    for s in range(400):
        print(f'game {s << 32} 0 {b} noguess'); print(f'open {proof.first_cell(model.BOARDS[b], s)}')
    for s in range(100):
        print(f'game {s << 32} 0 {b} classic'); print(f'open {proof.first_cell(model.BOARDS[b], s)}')") > "$B/deals.txt"
(cd "$O" && python3 model.py "$B/deals.txt") > "$B/deals.py.txt"
(cd "$O" && python3 edges.py script) > "$B/edges.txt"
(cd "$O" && python3 edges.py expect) > "$B/edges.expect"
(cd "$O" && python3 model.py "$B/edges.txt") > "$B/edges.py.txt"
filt() { grep -E '^(open|flag|cell|mine|hit) ' | sed -E 's/^(open|flag) [0-9]+ state=([a-z]+).*/\2/'; }

# each run of the C has a time limit, so a plant that loops goes red instead of hanging the check
check1() { timeout 120 "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt"; }
check2_deals() { timeout 60 "$1" "$B/deals.txt" | cmp -s - "$B/deals.py.txt"; }
check3() { timeout 60 "$1" "$B/edges.txt" > "$B/edges.out" && filt < "$B/edges.out" | cmp -s - "$B/edges.expect" && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

check1 "$B/trace" && echo "  1 logic: $(grep -c '^game' "$B/fuzz.txt") games ($(grep -c 'state=won' "$B/fuzz.py.txt") moves in won states, $(grep -c '^open.*state=lost' "$B/fuzz.py.txt") in lost, $(grep -c '^step' "$B/fuzz.txt") deal steps), $(wc -l < "$B/fuzz.py.txt") lines, identical to the model" \
    || red "1 logic: the C and the model differ"
if check2_deals "$B/trace_fast"; then
    echo "  2 deals: the C deals the model's boards for 400 seeds a board, and 100 a board with classic rules (mine checksums and attempts identical)"
else
    red "2 deals: the C deals other boards than the model"
fi
read -r classic first < <(paste -d'|' "$B/deals.txt" "$B/deals.py.txt" \
    | awk -F'|' 'prev ~ / classic$/ { n++; if ($2 ~ / attempts=1 dealt=1 /) k++ } { prev = $1 } END { print n + 0, k + 0 }')
[ "$classic" = 300 ] && [ "$first" = 300 ] && echo "  2 classic: all $classic classic deals are the first attempt" \
    || red "2 classic: $first of $classic classic deals are the first attempt"
# the fallback: real boards never need 400 attempts, so the C and the model are built with a cap of 3
cc $STRICT -O2 -DMINES_TRIES=3 -I"$G" -o "$B/trace_cap3" "$ROOT/tests/mines_trace.c" "$G/mines_logic.c" || red "build at a cap of 3"
(cd "$O" && python3 -c "
import model, proof
for s in range(100):
    print(f'game {s << 32} 0 hard noguess'); print(f'open {proof.first_cell(model.BOARDS[\"hard\"], s)}')") > "$B/cap3.txt"
(cd "$O" && MINES_TRIES=3 python3 model.py "$B/cap3.txt") > "$B/cap3.py.txt"
check2_cap() { timeout 60 "$1" "$B/cap3.txt" | cmp -s - "$B/cap3.py.txt"; }
hits=$(grep -c '^open .* attempts=3 ' "$B/cap3.py.txt")
check2_cap "$B/trace_cap3" && [ "$hits" -gt 50 ] \
    && echo "  2 fallback: at a cap of 3, the C deals the model's 100 HARD boards, $hits of them the third attempt" \
    || red "2 fallback: at a cap of 3 the C and the model differ ($hits deals at the cap)"
for b in $BOARDS; do
    (cd "$O" && python3 proof.py deals $b 400) > "$B/proof-$b.txt" && sed "s/^/  2 $b /" "$B/proof-$b.txt" \
        || { cat "$B/proof-$b.txt"; red "2 proof $b: a dealt board needs a guess, or the game's solver cleared one that does"; }
done
for b in easy:200 medium:200 hard:100; do
    (cd "$O" && python3 proof.py same ${b%:*} ${b#*:}) > "$B/same-${b%:*}.txt" && sed 's/^/  2 /' "$B/same-${b%:*}.txt" \
        || { cat "$B/same-${b%:*}.txt"; red "2 same ${b%:*}: the sweep and the listing differ"; }
done
(cd "$O" && python3 proof.py control control-5050.txt) > "$B/control.txt" && sed 's/^/  2 /' "$B/control.txt" \
    || { cat "$B/control.txt"; red "2 control: the 50/50 board was not reported as needing a guess"; }
check3 "$B/trace" && echo "  3 edges: $(grep -c '^# ' "$B/edges.txt") cases, the C and the model give the lines written by hand, every state line identical" \
    || red "3 edges: the C"
filt < "$B/edges.py.txt" | cmp -s - "$B/edges.expect" || red "3 edges: the model"

# 4: the app; each script names its seed and, last, what it must end with
[ "${1:-}" = --bless ] && BLESS=1
SCRIPTS="play refuse cursor settings hard quiet instant loadset"
for s in $SCRIPTS; do
    (cd "$O" && python3 appscript.py "$s") | cmp -s - "$O/scripts/app-$s.txt" || red "4 app: appscript.py $s differs from oracle/mines/scripts/app-$s.txt"
done
for s in $SCRIPTS; do (cd "$O" && python3 appscript.py nexts "$s") > "$B/$s.nexts.want"; done
(cd "$O" && python3 appscript.py badload) > "$B/app-badload.txt"
xxd -r -p "$O/scripts/app-refuse.load.hex" > "$B/refuse.load"
xxd -r -p "$O/scripts/app-loadset.load.hex" > "$B/loadset.load"
# the damaged saves start from loadset's, whose settings are not the defaults, so a load that took its settings
# before refusing the rest would show
good=$(cat "$O/scripts/app-loadset.load.hex")
printf '%s' "58${good:2}" | xxd -r -p > "$B/bad-magic.load"
printf '%s' "${good:0:2}01${good:4}" | xxd -r -p > "$B/bad-version.load"          # version 1 in a whole 80 bytes
printf '%s' "4d01d7390000020000000500000000000000" | xxd -r -p > "$B/bad-v1.load"  # a revision 1 save, 16 bytes
printf '%s' "${good:0:158}" | xxd -r -p > "$B/bad-short.load"                     # 79 bytes, each record whole
printf '%s' "${good:0:4}0f${good:6}" | xxd -r -p > "$B/bad-board.load"            # board 3
printf '%s' "${good:0:4}1e${good:6}" | xxd -r -p > "$B/bad-bits.load"             # HARD, classic, quiet and bit 4
printf '%s' "${good:0:4}8e${good:6}" | xxd -r -p > "$B/bad-bits7.load"            # and bit 7
printf '%s' "${good:0:134}09000000${good:142}" | xxd -r -p > "$B/bad-wins.load"   # the last record: 9 wins of 2 games
printf '%s' "${good:0:14}09000000${good:22}" | xxd -r -p > "$B/bad-wins0.load"    # the first record: 9 wins of 1 game
BADS="magic version v1 short board bits bits7 wins wins0"
script_of() { case "$1" in badload*) echo "$B/app-badload.txt" ;; *) echo "$O/scripts/app-$1.txt" ;; esac; }
app_run() { # app_run RUNNER NAME SUFFIX -> $B/NAME.SUFFIX.{hashes,save,out}
    local seed=5 load=()
    case "$1:$2" in *:refuse) seed=9 load=(-load "$B/refuse.load") ;; *:cursor) seed=3 ;; *:settings) seed=4 ;;
        *:loadset) seed=8 load=(-load "$B/loadset.load") ;;
        *:hard) seed=2441 ;; *:quiet) seed=6 ;; *:instant) seed=4976 ;; *:badload) seed=9 ;;
        *:badload-*) seed=9 load=(-load "$B/bad-${2#badload-}.load") ;; esac
    timeout 120 "$1" "$(script_of "$2")" -seed $seed "${load[@]}" -v -keyframes -hashes "$B/$2.$3.hashes" \
        -nexts "$B/$2.$3.nexts" -save "$B/$2.$3.save" > "$B/$2.$3.out" 2>&1
}
expect() { # expect NAME: the script's sound lines and its last line, '# expect save HEX tunes N tones N'
    local t s
    read -r _ _ _ save _ tunes _ tones < <(tail -1 "$(script_of "$1")")
    t=$(tail -1 "$B/$1.app.out")
    s=$(od -An -v -tx1 "$B/$1.app.save" | tr -d ' \n')
    case "$t" in *" $tones tones, $tunes tunes,"*) ;; *) red "4 app $1: '$t' (want $tunes tunes, $tones tones)"; return ;; esac
    [ "$s" = "$save" ] || { red "4 app $1: save $s, want $save"; return; }
    cmp -s <(grep -E '^(tone|tune) ' "$B/$1.app.out") <(grep '^# sound ' "$(script_of "$1")" | sed 's/^# sound //') \
        || { red "4 app $1: the sound differs from the script's"; return; }
    case "$1" in badload*) echo "  4 app $1: the save as expected (a fresh one: the damaged save refused)"; return ;; esac
    cmp -s "$B/$1.app.nexts" "$B/$1.nexts.want" || { red "4 app $1: a tick asks for other ms than the rules give"; return; }
    if [ "${BLESS:-0}" = 1 ]; then cp "$B/$1.app.hashes" "$O/frames/$1.hashes"; fi
    cmp -s "$B/$1.app.hashes" "$O/frames/$1.hashes" || { red "4 app $1: frame hashes differ from oracle/mines/frames"; return; }
    echo "  4 app $1: $(wc -l < "$B/$1.app.hashes") frames as committed; $tunes tunes, $tones tones, each as the script's; save and each tick's ask as expected"
}
for s in $SCRIPTS badload $(for d in $BADS; do echo badload-$d; done); do
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
    f.write('};\nWASM_GAME(mines_wasm_app, "MINES", mines_aot, mines_aot + sizeof mines_aot, 80);\n')
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
    for s in $SCRIPTS badload-wins; do
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
logic_plant() { # logic_plant NAME PERL CHECK WHAT: a plant in mines_logic.c, judged by CHECK (check1, check2_deals, check3)
    if plant "$1" mines_logic.c "$2" && build_trace "$STRICT -O2" "$B/plant-$1" "$B/pl-$1"; then
        "$3" "$B/pl-$1" && red "5 plant: $4 passes $3" || echo "  5 plant: $4: $3 red"
    fi
}
logic_plant floodflag 's/MINES_OPEN \| MINES_QUEUED \| stop/MINES_OPEN | MINES_QUEUED | (stop \& 0)/' check3 "a flood that opens flagged cells"
logic_plant chordany 's/        if \(flags != g->num\[c\]\) \/\*/        if (flags < g->num[c]) \/*/' check3 "a chord with too many flags"
logic_plant firstmine 's/            banned \|= c == nb\[i\];/            banned |= 0 \&\& c == nb[i];/' check3 "mines allowed beside the first open"
logic_plant noB 's/for \(int a = 0; a < g->cells && !done; a\+\+\)/for (int a = g->cells; a < g->cells \&\& !done; a++)/' check2_deals "the game's solver without rule B"
logic_plant loose 's/\|\| mines_solve\(g, f, NULL\)\)/|| 1)/' check2_deals "boards dealt without the solver's test"
logic_plant winconst 's/\} else if \(g->opened == g->cells - g->mines\) \{/} else if (g->opened == g->cells - g->n) {/' check3 "the win counted against the board's mines whatever the field"
logic_plant chordzero 's/        if \(flags != g->num\[c\]\)/        if (g->num[c] == 0 || flags != g->num[c])/' check3 "a chord on an opened 0 that does nothing"
logic_plant hitlast 's/                if \(g->hit < 0\)\n                    g->hit = nb\[i\];/                g->hit = nb[i];/' check3 "a chord's last mine shown as the one opened"
logic_plant loops 's/        if \(done\)\n            continue;/        if (done || 1)\n            continue;/' check2_deals "a solver that never stops (timed out, not hung)"
logic_plant wrap 's/if \(nx >= 0 && nx < g->w && ny >= 0/if (nx >= 0 \&\& nx <= g->w \&\& ny >= 0/' check3 "a neighbour across the row's end"
if plant cap mines_logic.h 's/#define MINES_TRIES 400/#define MINES_TRIES 100/' && build_trace "$STRICT -O2" "$B/plant-cap" "$B/pl-cap"; then
    check2_deals "$B/pl-cap" && red "5 plant: the fallback after 100 attempts passes check2_deals" || echo "  5 plant: the fallback after 100 attempts: check2_deals red"
fi
logic_plant noclassic 's/if \(g->classic \|\| g->attempts == MINES_TRIES/if (g->attempts == MINES_TRIES/' check2_deals "classic rules dealt as no guess"
logic_plant stepmore 's/for \(int i = 0; i < d && !g->dealt/for (int i = 0; i <= d \&\& !g->dealt/' check1 "a deal step making one attempt more"
logic_plant flagdeal 's/ \|\| \(g->cell\[f\] & MINES_FLAG\)\)\n        return false;/)\n        return false;/' check3 "a deal started on a flagged cell"

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
app_plant edge 's/    if \(y >= 0 && y < c->g.h\)\n        c->cy = y;/    if (y >= 0 \&\& y <= c->g.h)\n        c->cy = y;/' cursor "the cursor past the bottom edge"
app_plant bestworse 's/if \(r->best == 0 \|\| t < r->best\)/if (r->best == 0 || t > r->best)/' refuse "a slower time taken as the best"
app_plant besttie 's/if \(r->best == 0 \|\| t < r->best\)/if (r->best == 0 || t <= r->best)/' refuse "a tie taken as a new best"
app_plant nowrong 's/        if \(lost && !\(st & MINES_MINE\)\) \{ \/\* a wrong flag/        if (0) { \/* a wrong flag/' play "no cross on a wrong flag"
app_plant t0prev 's/        c->t0 = c->now; \/\*/        c->t0 = c->now - ms; \/*/' refuse "the time from the tick before the deal's"
app_plant enterlate 's/if \(c->state == OVER && ms - c->t_end < AGAIN_MS\)/if (c->state == OVER \&\& ms - c->t_end <= AGAIN_MS)/' play "ENTER refused at exactly 0.5 s"
app_plant upother 's/        if \(cursor && key == c->held\)\n            c->held = 0;/        if (cursor)\n            c->held = 0;/' cursor "any cursor key's up stopping the repeat"
app_plant noquiet 's/        if \(c->g.opened != before\)\n            beep\(c, 440, 10\);/        if (c->g.opened != before || 1)\n            beep(c, 440, 10);/' play "a tick for every open, even one that opens nothing"
app_plant dealstep 's/#define DEAL_STEP 4 /#define DEAL_STEP 5 /' hard "5 attempts a tick while dealing"
app_plant dealkeys 's/    case DEALING:\n        return false;\n//; s/    case PLAY:\n/    case PLAY:\n    case DEALING:\n/' hard "keys taken while the board is dealt"
app_plant loudbeep 's/    if \(!c->quiet\)\n        app_beep/    if (!c->quiet || 1)\n        app_beep/' quiet "tones with the sound off"
app_plant loudtune 's/    if \(!c->quiet\)\n        app_tune/    if (!c->quiet || 1)\n        app_tune/' settings "tunes with the sound off"
app_plant nowrap 's/            c->board = \(c->board \+ 1\) % MINES_BOARDS;/            c->board = c->board < MINES_BOARDS - 1 ? c->board + 1 : c->board;/' settings "5 on HARD not wrapping to EASY"
app_plant oneslot 's/    return &c->rec\[c->board \* 2 \+ c->classic\];/    return \&c->rec[c->board * 2];/' settings "classic games kept in the no-guess record"
app_plant zerobest 's/    if \(t == 0\)\n        t = 1;/    if (0)\n        t = 1;/' instant "a 0 ms win kept as no best"
app_plant winsgames 's/        if \(get32\(save \+ 3 \+ 12 \* i \+ 4\) > get32\(save \+ 3 \+ 12 \* i \+ 8\)\)\n            return;/        (void)0;/' badload-wins "a save with more wins than games taken"
app_plant shortlen 's/    if \(n < SAVE_BYTES \|\| /    if (n < 70 || /' badload-short "a 79-byte save taken"
app_plant anyversion 's/ \|\| save\[1\] != SAVE_VERSION//' badload-version "a version 1 save of 80 bytes taken"
app_plant board3 's/ \|\| \(save\[2\] & 3\) == 3//' badload-board "a save with board 3 taken"
app_plant highbits 's/ \|\| \(save\[2\] & 0xf0\)\)/)/' badload-bits "a save with a settings bit above bit 3 taken"

# the review's survivors (2026-10-10), each now with a check that sees it
if plant nocap mines_logic.c 's/ \|\| g->attempts == MINES_TRIES \|\| / || /' && cc $STRICT -O2 -DMINES_TRIES=3 -I"$B/plant-nocap" -o "$B/pl-nocap" "$ROOT/tests/mines_trace.c" "$B/plant-nocap/mines_logic.c"; then
    check2_cap "$B/pl-nocap" && red "5 plant: no fallback at the cap passes the cap check" || echo "  5 plant: no fallback at the cap: the cap check red"
fi
app_plant loadedflag 's/    c->loaded = true;\n//' loadset "a taken save's board reset to MEDIUM"
app_plant loadclassic 's/    c->classic = save\[2\] & SET_CLASSIC;\n//' loadset "a taken save's classic rules dropped"
app_plant loadquiet 's/    c->quiet = save\[2\] & SET_QUIET;\n//' loadset "a taken save's sound setting dropped"
app_plant earlyset 's/    for \(int i = 0; i < RECORDS; i\+\+\)\n        if \(get32/    c->loaded = true;\n    c->board = save[2] \& 3;\n    c->classic = save[2] \& SET_CLASSIC;\n    c->quiet = save[2] \& SET_QUIET;\n    for (int i = 0; i < RECORDS; i++)\n        if (get32/' badload-wins "the settings taken before the records are refused"
app_plant gamesatdeal 's/    rec\(c\)->games\+\+;\n//; s/            c->state = DEALING;\n/            c->state = DEALING;\n            rec(c)->games++;\n/' loadset "a game counted at its deal, not at ENTER"
app_plant dealtick 's/        if \(!mines_deal_step\(&c->g, DEAL_STEP\)\)\n            return 1;/        if (!mines_deal_step(\&c->g, DEAL_STEP))\n            return 100;/' hard "100 ms asked for while dealing"
app_plant playtick 's/        c->repeat_at \+= REPEAT_NEXT;\n    \}\n    return 20;/        c->repeat_at += REPEAT_NEXT;\n    }\n    return 100;/' play "100 ms asked for in a game"
app_plant easydot 's/        int d = px\(7, s\)/        int d = px(7, 13)/' instant "the mine's dot not scaled to the 22 px cell"
app_plant drawrec 's/    const record \*r = &c->rec\[c->board \* 2 \+ c->classic\];\n    if \(r->best\) \{\n        if \(over/    const record *r = \&c->rec[c->board * 2];\n    if (r->best) {\n        if (over/' loadset "the game's BEST from the no-guess record under classic rules"
app_plant titlerec 's/        const record \*r = &c->rec\[c->board \* 2 \+ c->classic\];/        const record *r = \&c->rec[c->board * 2];/' loadset "the title's BEST from the no-guess record under classic rules"
app_plant bitsmask 's/ \|\| \(save\[2\] & 0xf0\)\)/ || (save[2] \& 0x10))/' badload-bits7 "only bit 4 refused among the high settings bits"
app_plant winsloop 's/    for \(int i = 0; i < RECORDS; i\+\+\)\n        if \(get32\(save/    for (int i = 1; i < RECORDS; i++)\n        if (get32(save/' badload-wins0 "the first record's wins never checked"
app_plant setzero 's/        else if \(key == APP_KEY_ENTER\)\n            c->state = TITLE;/        else if (key == APP_KEY_ENTER || key == APP_KEY_0)\n            c->state = TITLE;/' settings "0 leaving the settings page"
app_plant flagone 's/        \} else if \(key == APP_KEY_0\) \{\n            mines_flag/        } else if (key == APP_KEY_0 || key == APP_KEY_1) {\n            mines_flag/' loadset "1 flagging in a game"
app_plant dealheld 's/            c->state = DEALING;\n            c->held = 0;/            c->state = DEALING;/' hard "a held key repeating after the deal it was held across"
app_plant nocap999 's/    return \(int32_t\)\(s < 999 \? s : 999\);/    return (int32_t)s;/' quiet "the top line's time past 999 s"
app_plant flaggone 's/    if \(lost && \(st & MINES_MINE\) && !\(st & MINES_FLAG\)\) \{/    if (lost \&\& (st \& MINES_MINE)) {/' instant "a flag on a mine hidden at a loss"

[ "$ok" = 1 ] && echo "minescheck: GREEN" || { echo "minescheck: RED"; exit 1; }
