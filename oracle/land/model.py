"""Land's rules from docs/land.md alone, all integers: the physics, the contact test, the judge and the terrain.

Nothing here reads the C. tests/land_trace.c prints the same lines, and oracle/land_check.sh compares them.
"""
import math

W = 400
Q = 65536
G, A = 655, 1500
VY, VX = 26214, 13107
VMAX = 4 * Q
TOP = -200 * Q
FUEL0, CRASH_COST = 2000, 200
TURN_STEPS = 4
PADS = ((24, 5), (34, 3), (48, 2))
OUTLINE = ((-6, 6), (6, 6), (6, 1), (4, -5), (0, -8), (-4, -5), (-6, 1))
FEET = (0, 1)
SIN = [round(16384 * math.sin(math.radians(15 * i))) for i in range(24)]
COS = [round(16384 * math.cos(math.radians(15 * i))) for i in range(24)]

M64 = (1 << 64) - 1


def tdiv(a, b):
    """C's / on ints: rounds toward 0."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


class Draw:
    def __init__(self, seed, rnd):
        self.x = mix64((seed ^ mix64(rnd)) & M64)

    def __call__(self, n):
        self.x = mix64(self.x)
        return self.x % n


def make_terrain(seed, rnd):
    """(vertices [(x, y) px], pads [(x0, x1, y, mult)], start (x Q, vx Q)) for a round."""
    d = Draw(seed, rnd)
    order = list(PADS)
    for i in (2, 1):
        j = d(i + 1)
        order[i], order[j] = order[j], order[i]
    pads = []
    for k, (w, m) in enumerate(order):
        x0 = 133 * k + 12 + d(133 - 24 - w)
        pads.append((x0, x0 + w, 130 + d(80), m))
    verts = [(0, 150 + d(70))]
    for target in [p[0] for p in pads] + [W]:
        pad = next((p for p in pads if p[0] == target), None)
        while verts[-1][0] < target:
            cx, cy = verts[-1]
            nx = cx + 12 + d(17)
            if nx > target - 12:
                nx = target
            ny = min(226, max(130, cy + d(61) - 30))
            if pad is not None and nx == target:
                ny = pad[2]
            verts.append((nx, ny))
        if pad is not None:
            verts.append((pad[1], pad[2]))
    verts[-1] = (W, verts[0][1])                # the edges meet
    sx = (40 + d(320)) * Q
    speed = 19661 + d(19661)
    vx = -speed if d(2) == 0 else speed
    return verts, pads, (sx, vx)


def ground(verts, xq):
    xq %= W * Q
    for i in range(1, len(verts)):
        x1, y1 = verts[i]
        if xq < x1 * Q or i == len(verts) - 1:
            x0, y0 = verts[i - 1]
            return y0 * Q + tdiv((y1 - y0) * (xq - x0 * Q), x1 - x0)
    raise AssertionError


def outline(s):
    a = s['a']
    return [(s['x'] + (hx * COS[a] - hy * SIN[a]) * 4, s['y'] + (hx * SIN[a] + hy * COS[a]) * 4) for hx, hy in OUTLINE]


def vertex_inside(pts, verts):
    n = len(pts)
    for vx, vy in verts:
        for sh in (0, -W, W):
            tx, ty = (vx + sh) * Q, vy * Q
            cs = []
            for i in range(n):
                (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
                cs.append((bx - ax) * (ty - ay) - (by - ay) * (tx - ax))
            if all(c < 0 for c in cs) or all(c > 0 for c in cs):
                return True
    return False


def contact(s, verts):
    pts = outline(s)
    under = any(py >= ground(verts, px) for px, py in pts)
    poke = vertex_inside(pts, verts)
    return under or poke, poke


def judge(s, verts, pads):
    """('land', the pad's multiplier, its y) or ('crash', 0, 0)."""
    if s['a'] != 0 or abs(s['vy']) > VY or abs(s['vx']) > VX:
        return 'crash', 0, 0
    pts = outline(s)
    feet = [pts[f][0] % (W * Q) for f in FEET]
    for x0, x1, py, m in pads:
        if all(x0 * Q <= fx <= x1 * Q for fx in feet):
            return 'land', m, py
    return 'crash', 0, 0


def new_ship(start, fuel):
    sx, vx = start
    return dict(x=sx, y=32 * Q, vx=vx, vy=0, a=0, fuel=fuel, timer=0,
                held=set(), pressed=set())


def key(s, k, down):
    """k is '4', '6' or '8'."""
    if down:
        s['held'].add(k)
        s['pressed'].add(k)
    else:
        s['held'].discard(k)


def step(s, verts, pads):
    """One 20 ms step: None while flying, else ('land', mult) or ('crash', 0)."""
    on = lambda k: k in s['held'] or k in s['pressed']
    if s['timer'] > 0:
        s['timer'] -= 1
    if s['timer'] == 0:
        turn = int(on('6')) - int(on('4'))
        if turn:
            s['a'] = (s['a'] + turn) % 24
            s['timer'] = TURN_STEPS
        if turn or (on('4') and on('6')):
            s['pressed'].discard('4')
            s['pressed'].discard('6')
    if on('8') and s['fuel'] > 0:
        s['vx'] += tdiv(A * SIN[s['a']], 16384)
        s['vy'] -= tdiv(A * COS[s['a']], 16384)
        s['fuel'] -= 1
    s['pressed'].discard('8')
    s['vy'] += G
    s['vx'] = max(-VMAX, min(VMAX, s['vx']))
    s['vy'] = max(-VMAX, min(VMAX, s['vy']))
    s['x'] = (s['x'] + s['vx']) % (W * Q)
    s['y'] += s['vy']
    if s['y'] < TOP:
        s['y'], s['vy'] = TOP, 0
    hit, poke = contact(s, verts)
    if not hit:
        return None
    r = judge(s, verts, pads)
    if r[0] == 'land':
        s['y'] = r[2] * Q - 6 * Q
    return r[:2]


def state_line(t, s):
    return f"{t} a={s['a']} x={s['x']} y={s['y']} vx={s['vx']} vy={s['vy']} fuel={s['fuel']} timer={s['timer']}"


def run_script(lines):
    """The trace format tests/land_trace.c reads: 'round SEED N [FUEL]', 'key K 0|1', 'step N', 'case ...', 'end?'."""
    out = []
    s = verts = pads = None
    t = 0
    done = False
    for line in lines:
        w = line.split()
        if not w or w[0].startswith('#'):
            continue
        if w[0] == 'round':
            verts, pads, start = make_terrain(int(w[1]), int(w[2]))
            s = new_ship(start, int(w[3]) if len(w) > 3 else FUEL0)
            t, done = 0, False
            out.append('terrain ' + ' '.join(f'{x},{y}' for x, y in verts))
            out.append('pads ' + ' '.join(f'{a},{b},{c},{m}' for a, b, c, m in pads))
            out.append(state_line(t, s))
        elif w[0] == 'case':
            # case X Y VX VY A TERRAIN... : a hand-made terrain (x,y pairs), pads where two neighbours share y (x5)
            verts = [tuple(map(int, p.split(','))) for p in w[6:]]
            pads = [(verts[i][0], verts[i + 1][0], verts[i][1], 5) for i in range(len(verts) - 1)
                    if verts[i][1] == verts[i + 1][1]]
            s = new_ship((int(w[1]), int(w[3])), FUEL0)
            s['y'], s['vy'], s['a'] = int(w[2]), int(w[4]), int(w[5])
            t, done = 0, False
        elif w[0] == 'end?':
            if not done:
                out.append('flying')
        elif w[0] == 'key':
            key(s, w[1], w[2] == '1')
        elif w[0] == 'step':
            for _ in range(int(w[1])):
                if done:
                    break
                r = step(s, verts, pads)
                t += 1
                out.append(state_line(t, s))
                if r:
                    out.append(f'end {r[0]} {r[1]}')
                    done = True
    return out


if __name__ == '__main__':
    import sys
    with open(sys.argv[1]) as f:
        print('\n'.join(run_script(f.read().splitlines())))
