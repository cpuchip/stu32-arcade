#!/bin/bash
# SPDX-License-Identifier: MIT
# SNAKE's checks (docs/snake.md, "The oracle"), all on the host:
#   1 the logic against oracle/snake/model.py: fuzzed key scripts over 200 games, every key and step identical;
#   2 a full game: the Hamiltonian-cycle autopilot (oracle/snake/cycle.py) for three seeds, the C and the model
#     identical and each won at length 880;
#   3 the rules at their edges (oracle/snake/edges.py), in the C and the model, against lines written by hand;
#   5 planted faults, each of which must turn its check red; a plant that does not build is red too.
# The logic builds under ASan and UBSan and the frame guard.
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

for f in "$G/snake_logic.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/snake_trace.c" "$2/snake_logic.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" || { echo "RED: build"; exit 1; }

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

[ "$ok" = 1 ] && echo "snakecheck: GREEN" || { echo "snakecheck: RED"; exit 1; }
