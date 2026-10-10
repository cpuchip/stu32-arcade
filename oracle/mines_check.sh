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
#   5 planted faults, each of which must turn its check red; a plant that does not build is red too.
# The logic builds under ASan and UBSan and the frame guard.
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

for f in "$G/mines_logic.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/mines_trace.c" "$2/mines_logic.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" || { echo "RED: build"; exit 1; }

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
filt() { grep -E '^(open|flag|cell|mine) ' | sed -E 's/^(open|flag) [0-9]+ state=([a-z]+).*/\2/'; }

check1() { "$1" "$B/fuzz.txt" | cmp -s - "$B/fuzz.py.txt"; }
check2_deals() { "$1" "$B/deals.txt" | cmp -s - "$B/deals.py.txt"; }
check3() { "$1" "$B/edges.txt" > "$B/edges.out" && filt < "$B/edges.out" | cmp -s - "$B/edges.expect" && cmp -s "$B/edges.out" "$B/edges.py.txt"; }

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
if plant chordany mines_logic.c 's/if \(g->num\[c\] == 0 \|\| flags != g->num\[c\]\)/if (g->num[c] == 0 || flags < g->num[c])/' \
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

[ "$ok" = 1 ] && echo "minescheck: GREEN" || { echo "minescheck: RED"; exit 1; }
