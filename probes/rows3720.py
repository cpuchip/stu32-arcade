"""Throwaway: changed rows a step for a wrap-around rock field on Rev A (0.42 ms a row, 52 B at 1 MHz).
A row changes when an object whose drawn integer position or shape changed covers it before or after.
Rocks drift in sub-pixels; the ship turns and flies; shots fly; bursts flash. Random play, not the rules."""
import random, sys

NFULL = 6  # 4 for wave 1; the doc's table ran both
TOP, H, W = 12, 228, 400


def run(mode, steps, seed):
    r = random.Random(seed)
    if mode == "gentle":
        rocks = [[r.uniform(0, W), r.uniform(0, H), r.choice([-1, 1]) * r.uniform(.25, .5),
                  r.choice([-1, 1]) * r.uniform(.25, .5), 24] for _ in range(4)]
        speed_small, rmax_shots = (.4, .7), 3
    else:
        rocks = [[r.uniform(0, W), r.uniform(0, H), r.choice([-1, 1]) * r.uniform(.5, 1.5),
                  r.choice([-1, 1]) * r.uniform(.5, 1.5), 20] for _ in range(NFULL)]
        speed_small, rmax_shots = (.8, 2.0), 4
    ship = [200., 114., 0., 0.]
    shots = []
    out = []
    prev = None
    for t in range(steps):
        # random play: thrust sometimes, turn sometimes, fire often
        turned = r.random() < .15
        if r.random() < .3:
            ship[2] += r.uniform(-.3, .3); ship[3] += r.uniform(-.3, .3)
        ship[2] = max(-3, min(3, ship[2] * .98)); ship[3] = max(-3, min(3, ship[3] * .98))
        ship[0] = (ship[0] + ship[2]) % W; ship[1] = (ship[1] + ship[3]) % H
        if r.random() < .4 and len(shots) < rmax_shots:
            a = r.uniform(0, 6.28)
            shots.append([ship[0], ship[1], 6 * __import__('math').cos(a), 6 * __import__('math').sin(a), 14])
        for s in shots:
            s[0] = (s[0] + s[2]) % W; s[1] = (s[1] + s[3]) % H; s[4] -= 1
        shots = [s for s in shots if s[4] > 0]
        for k in rocks:
            k[0] = (k[0] + k[2]) % W; k[1] = (k[1] + k[3]) % H
        # a hit now and then: split or vanish, keep the field populated
        if rocks and r.random() < .03:
            k = rocks.pop(r.randrange(len(rocks)))
            small = 12 if mode == "gentle" else k[4] // 2
            if (mode == "gentle" and k[4] == 24) or (mode == "full" and k[4] > 5):
                for _ in range(2):
                    rocks.append([k[0], k[1], r.choice([-1, 1]) * r.uniform(*speed_small),
                                  r.choice([-1, 1]) * r.uniform(*speed_small), small])
        if not rocks:
            n = 4 if mode == "gentle" else NFULL
            sz = 24 if mode == "gentle" else 20
            rocks = [[r.uniform(0, W), 0, r.uniform(-.5, .5), r.uniform(-.5, .5), sz] for _ in range(n)]
        # what is drawn: (id, x int, y int, half-height, shape tag)
        cur = {}
        for i, k in enumerate(rocks):
            cur[("rock", id(k))] = (int(k[0]), int(k[1]), k[4], 0)
        cur[("ship", 0)] = (int(ship[0]), int(ship[1]), 9, t if turned else 0)
        for s in shots:
            cur[("shot", id(s))] = (int(s[0]), int(s[1]), 1, 0)
        rows = set()
        if prev is not None:
            for key in set(cur) | set(prev):
                a, b = prev.get(key), cur.get(key)
                if a == b:
                    continue
                for o in (a, b):
                    if o:
                        for y in range(o[1] - o[2], o[1] + o[2] + 1):
                            rows.add(y % H)
        prev = cur
        out.append(len(rows))
    return out


for mode in ("gentle", "full"):
    allr = []
    for seed in range(6):
        allr += run(mode, 1000, seed)
    allr.sort()
    n = len(allr)
    mean = sum(allr) / n
    p95 = allr[int(.95 * n)]
    print(f"{mode}: rows mean {mean:.0f} p95 {p95} max {allr[-1]}  ms mean {mean*.42:.0f} p95 {p95*.42:.0f} max {allr[-1]*.42:.0f}"
          f"  steps over 50 ms {sum(1 for x in allr if x*.42 > 50)/n:.0%}")
