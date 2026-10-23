#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write app scripts for tools/apphost.c: a session at the title (skill keys), a start, then play:
ticks of uneven length (some 0, some long enough to be clamped) with key presses, releases and jumps
at their own times between the ticks (both keys of a side, so a side is often held by two), until the
time runs out or MAX ticks.
Also app-far: a noob game played as the original is, weaving (4 and 6 held in turn) and jumping, to
go far down the track: the random sessions stop near row 85 at the finish, short of where rows must be
made ahead of the ball (row 100), which only luck in the seeds had reached (oracle/app_check.sh
checks that some game passes row 110).
Words: key K D MS | tick MS. Usage: gen_appscripts.py OUTDIR [N]"""
import os
import random
import sys

LEFT, RIGHT, JUMP, ENTER = 31, 33, 26, 18
SOFT1, SOFT6, NINE, TIMES, SEVEN, GOLD, SOFT2, SOFT5, PLUS = 0, 5, 27, 34, 25, 30, 1, 4, 46
# in play: SOFT1 left, SOFT6 right, PLUS nothing (dropped 2026-10-23), the rest jump


def session(rng, ticks, start=None):
    """The clock is the sum of the ticks' ms; each key falls inside the interval of the tick that
    follows it (at or after the last tick, at or before the next)."""
    out, clock = [], 0

    def tick(ms):
        nonlocal clock
        clock += ms
        out.append(f"tick {ms}")

    tick(rng.randrange(1, 50))
    for _ in range(rng.randrange(0, 6)):           # skill choice at the title
        ms = rng.randrange(50, 300)
        k = rng.choice([LEFT, RIGHT])
        a = clock + rng.randrange(0, ms // 2)
        out += [f"key {k} 1 {a}", f"key {k} 0 {a + rng.randrange(0, ms // 2)}"]
        tick(ms)
    ms = rng.randrange(50, 500)
    start = rng.choice([ENTER, JUMP, rng.randrange(0, 5)]) if start is None else start
    a = clock + rng.randrange(0, ms)
    out += [f"key {start} 1 {a}", f"key {start} 0 {a}"]
    tick(ms)
    held = {LEFT: False, RIGHT: False, SOFT1: False, SOFT6: False}
    for _ in range(ticks):
        r = rng.random()
        ms = 0 if r < 0.01 else rng.randrange(150, 400) if r < 0.03 else rng.randrange(20, 80)
        for at in sorted(clock + rng.randrange(0, ms + 1) for _ in range(rng.choice([0, 0, 0, 1, 1, 2]))):
            k = rng.choice([LEFT, RIGHT, LEFT, RIGHT, SOFT1, SOFT6, JUMP, ENTER, NINE, TIMES, SEVEN, GOLD, SOFT2, SOFT5, PLUS])
            if k in held:
                held[k] = not held[k]
                out.append(f"key {k} {int(held[k])} {at}")
            else:  # a jump key: released a little later, as a thumb does
                out += [f"key {k} 1 {at}", f"key {k} 0 {at + rng.randrange(0, 4)}"]
        tick(ms)
    return out


def far(ticks=6000, period=300, jump=700):
    """Weave every PERIOD ms (the original's accelerator: forward speed comes from steering) and jump
    every JUMP ms (speed while rising), 20 ms ticks, from soft key 0 (noob)."""
    out = ["tick 20", "key 0 1 30", "key 0 0 30", "tick 20"]
    clock, held = 40, LEFT
    out.append(f"key {held} 1 {clock}")
    swap = hop = clock
    for _ in range(ticks):
        clock += 20
        if clock - swap >= period:
            out += [f"key {held} 0 {clock - 20}", f"key {RIGHT if held == LEFT else LEFT} 1 {clock - 20}"]
            held, swap = RIGHT if held == LEFT else LEFT, clock
        if clock - hop >= jump:
            out += [f"key {JUMP} 1 {clock - 20}", f"key {JUMP} 0 {clock - 18}"]
            hop = clock
        out.append("tick 20")
    return out


def main():
    outdir = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    os.makedirs(outdir, exist_ok=True)
    rng = random.Random(20261008)
    for i in range(n):
        # the first: noob (soft key 0) for 8,000 ticks, long enough to reach the finish
        lines = session(rng, 8000, start=0) if i == 0 else session(rng, rng.choice([400, 1500, 4000]))
        with open(os.path.join(outdir, f"app-{i}.txt"), "w") as f:
            f.write("\n".join(lines) + "\n")
    with open(os.path.join(outdir, "app-far.txt"), "w") as f:
        f.write("\n".join(far()) + "\n")


if __name__ == "__main__":
    main()
