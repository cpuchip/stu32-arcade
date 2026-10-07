#!/bin/bash
# The stage 1 gate: babalcore (oracle/ref) and the C port (oracle/driver.c) print the same
# pins, levels and games byte for byte, and every planted divergence turns that red.
# Plants are applied to a scratch copy at run time (never kept), each checked to change its
# file and to build. Env: N_SEEDS (8), PER_SEED (5) scripts per seed.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
REF=$ROOT/oracle/ref/target/release/babal-ref
B=$ROOT/build/oracle
CFLAGS="-O2 -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -ffp-contract=off"
N_SEEDS=${N_SEEDS:-8}
PER_SEED=${PER_SEED:-5}
TIMEOUT=120

rm -rf "$B" && mkdir -p "$B/ref"
python3 "$ROOT/oracle/gen_scripts.py" "$B/scripts" "$N_SEEDS" "$PER_SEED" || exit 1

# The jobs: name and arguments, one per line.
{
    echo "pins pins"
    for sk in noob easy medium hard expert; do
        for seed in 0 1 18446744073709551615 9876543210; do echo "level-$sk-$seed level $seed $sk 300"; done
        echo "level-$sk-123-slide level 123 $sk 10350"
    done
    for s in "$B"/scripts/*.txt; do echo "game-$(basename "$s" .txt) game $s"; done
    for seed in $(seq 0 21); do echo "points-$seed points $seed"; done
    for seed in $(seq 0 15); do echo "inputs-$seed inputs $seed"; done
} > "$B/jobs"

while read -r name args; do
    # shellcheck disable=SC2086
    timeout $TIMEOUT "$REF" $args > "$B/ref/$name" || { echo "reference failed: $name"; exit 1; }
done < "$B/jobs"

# suite DRIVER [stop-at-first]: 0 when every job matches the reference.
suite() {
    local drv=$1 stop=${2:-} bad=0
    while read -r name args; do
        # shellcheck disable=SC2086
        # MALLOC_PERTURB_: glibc fills fresh heap with garbage, so memory the C forgets to
        # initialise shows (it is usually zero and would pass).
        if ! MALLOC_PERTURB_=165 timeout $TIMEOUT "$drv" $args > "$B/out" 2>/dev/null || ! cmp -s "$B/out" "$B/ref/$name"; then
            bad=$((bad + 1))
            [ -n "$stop" ] && { echo "  red at $name"; return 1; }
            echo "  DIFF $name"
        fi
    done < "$B/jobs"
    return $((bad > 0))
}

jobs=$(wc -l < "$B/jobs")
bytes=$(cat "$B"/ref/* | wc -c)
ticks=$(cat "$B"/ref/game-* | grep -c '^t ')
rows=$(cat "$B"/ref/level-* | grep -c '^r ')
echo "reference: $jobs jobs, $bytes bytes ($ticks game ticks, $rows level rows)"

# Plants: file, sed expression, what it breaks.
PLANTS=(
    "player.c|s/(JUMP_HEIGHT_SCALE \* 6.0)/(JUMP_HEIGHT_SCALE * 6.01)/|a constant (JUMP_GRAVITY)"
    "row_generator.c|s/double change = bb_chacha20_random_f64(rng);/double change = f;/|an RNG draw skipped"
    "chacha20.c|s/s\[14\] = 0;/s[14] = 1;/|the ChaCha stream word"
    "player.c|/BB_EVENT_CATCH_UP/d|an event dropped (catch_up at the end of a fall)"
    "player.c|s/    handle_boost(p, level, out);/    handle_forward(p, delta, level);/;t;s/    handle_forward(p, delta, level);/    handle_boost(p, level, out);/|two handlers swapped (boost, forward)"
    "level.c|s/bb_level_slab_kind(lvl, col, lvl->row0 + row, boost)/bb_level_slab_kind(lvl, col, row, boost)/|quirk 2 fixed in item"
    "level.c|s/size_t j = lvl->len;/size_t j = (size_t)lvl->row0 + lvl->len;/|quirk 1 fixed (absolute row seeds)"
    "timeval.c|s/(c == 0 && (q.lo & 1))/c == 0/|from_secs_f64 ties away from zero"
    "vec3.c|s/a.x \* one_t + b.x \* t/a.x + (b.x - a.x) * t/|lerp's formula in x"
    "points_counter.c|s/pc->total_points += INSTANT_SPEED_WEIGHT \* delta_best_dist \* instant_speed;/pc->total_points += INSTANT_SPEED_WEIGHT * (delta_best_dist * instant_speed);/|one product regrouped"
    # The plants that survived the first gate (fresh-eyes review, 2026-10-07):
    "points_counter.c|s/if (e->present \&\& e->key == key)/if (e->present \&\& e->key == key \&\& key < 256)/|scores from row 256 on never read back"
    "babal.h|s/#define BB_STEER_KEYS_MAX 8/#define BB_STEER_KEYS_MAX 1/|one steer key a side"
    "points_counter.c|s/> 200.0 ? 5 : 10/> 200.0 ? 4 : 10/|the long-level percent step 5 made 4"
    "input_jump.c|s/if (now >= ij->last_request)/if (now > ij->last_request)/|a jump popped at its own instant"
    "input_steer.c|/if (now < k->last_press)/{N;d}|the backwards-clock guard removed"
    "time_tracker.c|s/tt->now_sec_f64 >= BB_TIME_TRACKER_MAX_SEC/tt->now_sec_f64 > BB_TIME_TRACKER_MAX_SEC/|the time tracker's reset at exactly its maximum"
    "maths.c|/bb_heaviside_step(double x)/,/^}/s/return 0.5;/return 0.25;/|heaviside_step at 0"
    "time_tracker.c|s/%03/%3/|TimeTracker's Display padding"
    # From the second review:
    "points_counter.c|s/time_to_complete < 50.0 ?/time_to_complete <= 50.0 ?/|the short-level step edge at 50"
    "points_counter.c|s/time_to_complete > 200.0 ?/time_to_complete >= 200.0 ?/|the long-level step edge at 200"
)
# Known equivalent, so not a plant: backing_camera_blend's `now_sec <= begin_sec` made `<` gives
# (now - begin) / half_range = 0.0 at equality, the same 0.0.

echo "== the port"
if suite "$ROOT/build/driver"; then
    echo "  all $jobs identical"
    port_ok=1
else
    echo "  RED"
    port_ok=0
fi

echo "== plants (each must go red)"
plants_ok=1
for p in "${PLANTS[@]}"; do
    IFS='|' read -r file expr what <<< "$p"
    P=$B/plant
    rm -rf "$P" && mkdir -p "$P/oracle" && cp -r "$ROOT/babal" "$P/babal" && cp "$ROOT/oracle/driver.c" "$P/oracle/"
    sed -i "$expr" "$P/babal/$file"
    if cmp -s "$P/babal/$file" "$ROOT/babal/$file"; then
        echo "PLANT DID NOT APPLY: $what"; plants_ok=0; continue
    fi
    rm -f "$P/driver"
    # shellcheck disable=SC2086
    if ! cc $CFLAGS "$P/oracle/driver.c" "$P"/babal/*.c -lm -o "$P/driver" 2> "$P/cc.err"; then
        echo "PLANT DID NOT BUILD: $what"; head -3 "$P/cc.err"; plants_ok=0; continue
    fi
    if suite "$P/driver" stop > "$P/result"; then
        echo "SURVIVED: $what"; plants_ok=0
    else
        echo "red: $what ($(sed 's/^  red at //' "$P/result"))"
    fi
done

# babalcore's own tests (make test) must see a plant too.
P=$B/plant
rm -rf "$P" && mkdir -p "$P/tests" && cp -r "$ROOT/babal" "$P/babal" && cp "$ROOT/tests/babal_test.c" "$P/tests/"
sed -i 's/buf\[8 + i\] = (uint8_t)(j >> (8 \* i));/buf[8 + i] = (uint8_t)((j + 1) >> (8 * i));/' "$P/babal/row_generator.c"
# shellcheck disable=SC2086
if cmp -s "$P/babal/row_generator.c" "$ROOT/babal/row_generator.c"; then
    echo "PLANT DID NOT APPLY: tests, row seed off by one"; plants_ok=0
elif ! cc $CFLAGS "$P/tests/babal_test.c" "$P"/babal/*.c -lm -o "$P/babal_test" 2> "$P/cc.err"; then
    echo "PLANT DID NOT BUILD: tests, row seed off by one"; plants_ok=0
elif "$P/babal_test" > "$P/result"; then
    echo "SURVIVED: tests, row seed off by one"; plants_ok=0
else
    echo "red: tests, row seed off by one ($(grep -c FAIL "$P/result") checks failed)"
fi

# Not a gate: the same port with fused multiply-adds allowed, to show what the flag holds off.
P=$B/plant
rm -rf "$P" && mkdir -p "$P/oracle" && cp -r "$ROOT/babal" "$P/babal" && cp "$ROOT/oracle/driver.c" "$P/oracle/"
if cc -O2 -std=c11 -D_DEFAULT_SOURCE -march=native -ffp-contract=fast "$P/oracle/driver.c" "$P"/babal/*.c -lm -o "$P/driver" 2>/dev/null; then
    if suite "$P/driver" stop > "$P/result"; then echo "probe: -march=native -ffp-contract=fast stays identical"; else echo "probe: -march=native -ffp-contract=fast goes red ($(sed 's/^  red at //' "$P/result"))"; fi
fi

# The device windows (docs/stage2.md): the level holds its newest LW rows and the scores log its
# newest GW rows. Games and points must give the same output (less the final log and rows dumps,
# which show storage) with no read outside a window; windows below the deepest read must fail.
LW=512
GW=256
wsuite() {
    local drv=$1 lw=$2 gw=$3 stop=${4:-} bad=0
    while read -r name args; do
        case $name in game-*|points-*) ;; *) continue ;; esac
        sed '/^log /,$d' "$B/ref/$name" > "$B/ref.cut"
        # shellcheck disable=SC2086
        if ! BB_WINDOW=$lw BB_LOG_WINDOW=$gw MALLOC_PERTURB_=165 timeout $TIMEOUT "$drv" $args > "$B/out" 2>"$B/err" \
            || ! cmp -s "$B/out" "$B/ref.cut"; then
            bad=$((bad + 1))
            [ -n "$stop" ] && { echo "  red at $name ($(grep -m1 'limit\|depth' "$B/err" | tr -d '\n'))"; return 1; }
            echo "  DIFF $name"
        fi
    done < "$B/jobs"
    return $((bad > 0))
}
echo "== the device windows: level $LW rows, log $GW rows"
if wsuite "$ROOT/build/driver" $LW $GW; then echo "  all games and points identical, no read outside a window"; else echo "  RED"; port_ok=0; fi
for ctl in "150 $GW|a level window below the deepest read (298)" "$LW 64|a log window below the deepest lookup (103)"; do
    IFS='|' read -r sizes what <<< "$ctl"
    # shellcheck disable=SC2086
    if wsuite "$ROOT/build/driver" $sizes stop > "$B/result"; then echo "SURVIVED: $what"; plants_ok=0; else echo "red: $what ($(sed 's/^  red at //' "$B/result"))"; fi
done
P=$B/plant
rm -rf "$P" && mkdir -p "$P/oracle" && cp -r "$ROOT/babal" "$P/babal" && cp "$ROOT/oracle/driver.c" "$P/oracle/"
sed -i 's/if (e->present \&\& e->key == key)/if (e->present)/' "$P/babal/points_counter.c"
# shellcheck disable=SC2086
if cmp -s "$P/babal/points_counter.c" "$ROOT/babal/points_counter.c"; then echo "PLANT DID NOT APPLY: a log slot read without its key"; plants_ok=0
elif ! cc $CFLAGS "$P/oracle/driver.c" "$P"/babal/*.c -lm -o "$P/driver" 2> "$P/cc.err"; then echo "PLANT DID NOT BUILD: a log slot read without its key"; plants_ok=0
elif wsuite "$P/driver" $LW $GW stop > "$P/result"; then echo "SURVIVED: a log slot read without its key (window mode)"; plants_ok=0
else echo "red: a log slot read without its key, window mode ($(sed 's/^  red at //' "$P/result"))"; fi

if [ "$port_ok" = 1 ] && [ "$plants_ok" = 1 ]; then
    echo "GATE GREEN: $jobs jobs identical, and in the device windows; $((${#PLANTS[@]} + 4)) plants and controls red"
    exit 0
fi
echo "GATE RED"
exit 1
