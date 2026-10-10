"""Every pad is landable: a reference autopilot, flown in the model through the keys, lands on every pad of every
terrain for seeds 0..N-1 within a fuel budget (docs/land.md, the oracle's check 2).

  fair.py check N [FUEL]       every seed's round 0, all three pads; exit 1 when any flight fails
  fair.py script SEED ROUND PAD [FUEL]   the autopilot's flight as a trace script (key and step lines) and its end
  fair.py scripts N [FUEL]     every seed's round 0, all three pads, as one trace script ('end?' after each)
"""
import math
import multiprocessing
import sys

import model as M

Q = M.Q


# the family of settings tried in turn: clearance over the path, the approach's reach, the hold above the pad (px)
VARIANTS = ((34, 30, 28), (48, 50, 40), (60, 40, 48), (40, 70, 34))


def autopilot(s, verts, pad, v=VARIANTS[0]):
    """(thrust, turn) wanted this step."""
    clear_px, reach_px, hold_px = v
    x0, x1, py, _ = pad
    cx = (x0 + x1) * Q // 2
    x, y, vx, vy = s['x'], s['y'], s['vx'], s['vy']
    span_q = M.W * Q
    dx = (cx - x + span_q // 2) % span_q - span_q // 2     # the short way, across the wrap if that is shorter
    lo, hi = sorted((x // Q, x // Q + dx // Q))
    top = min(M.ground(verts, (xx % M.W) * Q) for xx in range(lo - 14, hi + 15))
    slack = (x1 - x0) * Q // 2 - 6 * Q - Q // 2
    h = py * Q - y - 6 * Q
    if abs(dx) > reach_px * Q:
        vx_des = max(-Q, min(Q, dx // 50))
        vy_des = max(-Q // 2, min(Q // 2, (top - clear_px * Q - y) // 30))
        span = 6
    else:
        vx_des = max(-Q // 3, min(Q // 3, dx // 40))
        vy_des = max(2000, min(M.VY * 6 // 10, math.isqrt(max(0, (M.A - M.G) * h))))
        off = abs(dx) > slack - Q or abs(vx) > M.VX * 6 // 10
        if off:
            if h < (hold_px + 6) * Q:
                vy_des = max(-Q // 8, min(vy_des, (h - hold_px * Q) // 30))
            # off centre, the feet stay clear of the ground under the ship (a pad in a pit has rims)
            near = min(M.ground(verts, ((x // Q + xx) % M.W) * Q) for xx in range(-10, 11))
            clear = near - y - 6 * Q
            if clear < 12 * Q:
                vy_des = min(vy_des, (clear - 12 * Q) // 20)
        span = 2 if h > 14 * Q else 1 if off else 0
    ax = (vx_des - vx) // 6
    ay = (vy_des - vy) // 3 - M.G
    best, ba = ax * ax + ay * ay, None
    for d in range(-span, span + 1):
        aa = d % 24
        e = (M.A * M.SIN[aa] // 16384 - ax) ** 2 + (-M.A * M.COS[aa] // 16384 - ay) ** 2
        if e < best:
            best, ba = e, aa
    want = 0 if ba is None else ba
    thrust = ba is not None and abs((ba - s['a'] + 12) % 24 - 12) <= 1
    d = (want - s['a'] + 12) % 24 - 12
    return thrust, (d > 0) - (d < 0)


def fly(seed, rnd, padi, fuel, limit=4000, v=VARIANTS[0]):
    """(result, steps, fuel used, script lines)."""
    verts, pads, start = M.make_terrain(seed, rnd)
    s = M.new_ship(start, fuel)
    lines = [f'round {seed} {rnd} {fuel}']
    held = {'4': False, '6': False, '8': False}
    pending = 0
    for t in range(limit):
        thrust, turn = autopilot(s, verts, pads[padi], v)
        want = {'8': thrust, '4': turn < 0, '6': turn > 0}
        for k in '468':
            if want[k] != held[k]:
                if pending:
                    lines.append(f'step {pending}')
                    pending = 0
                lines.append(f'key {k} {int(want[k])}')
                M.key(s, k, want[k])
                held[k] = want[k]
        r = M.step(s, verts, pads)
        pending += 1
        if r:
            lines.append(f'step {pending}')
            return r, t + 1, fuel - s['fuel'], lines
    lines.append(f'step {pending}')
    return ('timeout', 0), limit, fuel - s['fuel'], lines


def fly_any(seed, rnd, padi, fuel):
    """The first variant's flight that lands on the pad, else the first variant's."""
    mult = M.make_terrain(seed, rnd)[1][padi][3]
    first = None
    for v in VARIANTS:
        got = fly(seed, rnd, padi, fuel, v=v)
        if got[0] == ('land', mult):
            return got
        first = first or got
    return first


def one(job):
    seed, padi, fuel = job
    r, t, f, _ = fly_any(seed, 0, padi, fuel)
    return seed, padi, r, t, f


def one_script(job):
    seed, padi, fuel = job
    return fly_any(seed, 0, padi, fuel)[3]


def main():
    if sys.argv[1] == 'check':
        n = int(sys.argv[2])
        fuel = int(sys.argv[3]) if len(sys.argv) > 3 else 700
        bad, used = [], []
        jobs = [(seed, padi, fuel) for seed in range(n) for padi in range(3)]
        with multiprocessing.Pool() as pool:
            for seed, padi, r, t, f in pool.map(one, jobs, chunksize=8):
                if r != ('land', M.make_terrain(seed, 0)[1][padi][3]):
                    bad.append((seed, padi, r, t))
                else:
                    used.append(f)
        used.sort()
        print(f'fair: {3 * n - len(bad)} of {3 * n} flights landed on their pad with {fuel} fuel'
              + (f'; fuel used median {used[len(used) // 2]}, most {used[-1]}' if used else ''))
        for b in bad[:10]:
            print('  failed: seed %d pad %d %s at step %d' % b)
        return 1 if bad else 0
    if sys.argv[1] == 'scripts':
        n = int(sys.argv[2])
        fuel = int(sys.argv[3]) if len(sys.argv) > 3 else 700
        with multiprocessing.Pool() as pool:
            for lines in pool.map(one_script, [(seed, padi, fuel) for seed in range(n) for padi in range(3)], chunksize=8):
                print('\n'.join(lines))
                print('end?')
        return 0
    if sys.argv[1] == 'script':
        seed, rnd, padi = map(int, sys.argv[2:5])
        fuel = int(sys.argv[5]) if len(sys.argv) > 5 else 700
        r, t, f, lines = fly_any(seed, rnd, padi, fuel)
        print('\n'.join(lines))
        print(f'# end {r[0]} {r[1]} at step {t}, fuel used {f}')
        return 0
    return 2


if __name__ == '__main__':
    sys.exit(main())
