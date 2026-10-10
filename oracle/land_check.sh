#!/bin/bash
# SPDX-License-Identifier: MIT
# LAND's checks (docs/land.md, "The oracle"), all on the host:
#   1 the logic against oracle/land/model.py: fuzzed key scripts over 200 rounds, the terrains and every step;
#   2 every pad is landable: the autopilot lands every pad of 1,000 terrains within 700 fuel (oracle/land/fair.py),
#     a control with 100 fuel must fail, and 300 of its flights replayed through the C must land, step for step;
#   3 the landing rule at its edges (oracle/land/edges.py), in the C and the model, against ends written by hand;
#   5 planted faults, each of which must turn its check red.
# The logic builds under ASan and UBSan and the frame guard.
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

for f in "$G/land_logic.c"; do
    cc $STRICT -O2 -Wframe-larger-than=1024 -I"$G" -I"$FW" -c "$f" -o /dev/null || red "frame guard: $f"
done
build_trace() { cc $1 -I"$2" -o "$3" "$ROOT/tests/land_trace.c" "$2/land_logic.c" || { red "build in $2"; return 1; }; }
build_trace "$SAN" "$G" "$B/trace" && build_trace "$STRICT -O2" "$G" "$B/trace_fast" || { echo "RED: build"; exit 1; }

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

[ "$ok" = 1 ] && echo "landcheck: GREEN" || { echo "landcheck: RED"; exit 1; }
