#!/usr/bin/env python3
"""3720 (rocks) reference model, written from docs/3720.md and oracle/rocks/TRACE.md alone.

Usage: python3 oracle/rocks/model.py [SCRIPT]   (stdin when no SCRIPT)

All game state is integers. The SIN/COS table is computed once from its definition
(round(16384 * sin(a * 15 deg))) and checked against the exact values; nothing after that
touches a float. Choices where the doc is silent are marked CHOICE.
"""
import math
import sys

Q = 65536
M64 = (1 << 64) - 1
W = 400 * Q          # field width in Q
H = 228 * Q          # field height in Q
NSLOT = 24


def _round_half_away(v):
    return int(math.floor(v + 0.5)) if v >= 0 else -int(math.floor(-v + 0.5))


SIN = [_round_half_away(16384 * math.sin(math.radians(a * 15))) for a in range(24)]
COS = [_round_half_away(16384 * math.cos(math.radians(a * 15))) for a in range(24)]
# The table's first quadrant, checked: no value sits near a .5 tie, so the rounding rule cannot move it.
assert SIN[:7] == [0, 4240, 8192, 11585, 14189, 15826, 16384], SIN[:7]
assert all(COS[a] == SIN[(a + 6) % 24] for a in range(24))


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def cdiv(a, b):
    """C's / : truncation toward zero."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def wrap_x(x):
    return x % W


def wrap_y(y):
    return y % H


def near(d, span):
    """Wrap a difference into [-span/2, span/2)."""
    d %= span
    if d >= span // 2:
        d -= span
    return d


def dist2_256(ax, ay, bx, by):
    """Squared distance in (1/256 px)^2 by the doc's rule."""
    dx = cdiv(near(ax - bx, W), 256)
    dy = cdiv(near(ay - by, H), 256)
    return dx * dx + dy * dy


def within(ax, ay, bx, by, r_px):
    r = r_px * 256
    return dist2_256(ax, ay, bx, by) < r * r


MODES = {
    'g': dict(T=3277, V=131072, shots=3, cool=4, life=20, shield=60,
              radius={'L': 24, 'M': 12}, nxt={'L': 'M', 'M': None}),
    'f': dict(T=6554, V=262144, shots=4, cool=3, life=16, shield=40,
              radius={'L': 20, 'M': 10, 'S': 5}, nxt={'L': 'M', 'M': 'S', 'S': None}),
}
FULL_POINTS = {'L': 10, 'M': 25, 'S': 50}

KEYJOB = {'4': 'L', 'S1': 'L', '6': 'R', 'S6': 'R',
          '7': 'T', '8': 'T', '9': 'T', 'S2': 'T',
          'G': 'F', '5': 'F', 'X': 'F', 'S5': 'F',
          'E': None, 'P': None}


class Game:
    def __init__(self, mode, seed, game):
        self.mode = mode
        self.m = MODES[mode]
        self.rng = mix64((seed ^ mix64(game)) & M64)
        self.step_n = 0
        self.acc = 0
        self.x, self.y = 200 * Q, 114 * Q
        self.vx = self.vy = 0
        self.a = 0
        self.turn = 0
        self.cool = 0
        self.shield = self.m['shield']
        self.lost = 0
        self.ships = 3
        self.score = 0
        self.next = 5000 if mode == 'f' else 0
        self.wave = 1
        self.pause = 0
        self.clock = 0
        self.over = 0
        self.shots = []                    # [x, y, vx, vy, life], oldest first
        self.rocks = [None] * NSLOT        # [size, x, y, vx, vy, outline]
        self.held = set()
        self.latch = set()
        self.make_wave()

    # ---- draws ----
    def draw(self, n):
        self.rng = mix64(self.rng)
        return self.rng % n

    def speed(self, size):
        if self.mode == 'g':
            if size == 'L':
                return 16384 + self.draw(16385)
            return 26214 + self.draw(19661)
        b = 32768 + 3277 * min(self.wave - 1, 10)
        if size == 'L':
            return b + self.draw(b // 2)
        if size == 'M':
            return (3 * b) // 2 + self.draw(b // 2)
        return 2 * b + self.draw(b // 2)

    @staticmethod
    def vel(s, h):
        return cdiv(s * SIN[h], 16384), cdiv(-s * COS[h], 16384)

    # ---- rocks ----
    def large_count(self):
        if self.mode == 'g':
            return 3 if self.wave == 1 else 4
        return min(6, 3 + self.wave)

    def first_empty(self):
        for i in range(NSLOT):
            if self.rocks[i] is None:
                return i
        return None

    def make_wave(self):
        r = self.m['radius']['L']
        for _ in range(self.large_count()):
            h = self.draw(24)
            s = self.speed('L')
            outline = self.draw(4)
            x = self.draw(400) * Q
            y = 0
            if within(x, y, self.x, self.y, 80 + r):
                x = wrap_x(x + 200 * Q)
            vx, vy = self.vel(s, h)
            slot = self.first_empty()
            if slot is not None:               # CHOICE: a full table drops the rock (draws already made)
                self.rocks[slot] = ['L', x, y, vx, vy, outline]

    def break_rock(self, i):
        size, x, y = self.rocks[i][0], self.rocks[i][1], self.rocks[i][2]
        nxt = self.m['nxt'][size]
        if nxt is None:
            self.rocks[i] = None
            return size
        self.rocks[i] = None
        h1 = self.draw(24)
        h2 = (h1 + 12) % 24
        # CHOICE: first child's speed then outline, then the second's speed then outline.
        s1 = self.speed(nxt)
        o1 = self.draw(4)
        vx, vy = self.vel(s1, h1)
        self.rocks[i] = [nxt, x, y, vx, vy, o1]
        s2 = self.speed(nxt)
        o2 = self.draw(4)
        slot = self.first_empty()
        if slot is not None:                   # CHOICE: no empty slot drops the second (draws made)
            vx, vy = self.vel(s2, h2)
            self.rocks[slot] = [nxt, x, y, vx, vy, o2]
        return size

    def add_score(self, size):
        if self.mode == 'g':
            self.score += 1
            return
        self.score += FULL_POINTS[size]
        while self.score >= self.next:
            self.next += 5000
            if self.ships < 5:
                self.ships += 1

    def shot_hits(self, s):
        for i in range(NSLOT):
            rk = self.rocks[i]
            if rk is not None and within(s[0], s[1], rk[1], rk[2], self.m['radius'][rk[0]]):
                size = self.break_rock(i)
                self.add_score(size)
                return True
        return False

    # ---- keys ----
    def key(self, k, down):
        if k not in KEYJOB:
            raise ValueError('unknown key ' + k)
        if down:
            self.held.add(k)
            if KEYJOB[k]:
                self.latch.add(KEYJOB[k])
        else:
            self.held.discard(k)

    def job(self, j):
        return j in self.latch or any(KEYJOB[k] == j for k in self.held)

    def in_play(self):
        return self.lost == 0 and not self.over

    # ---- the step ----
    def step(self):
        m = self.m
        self.step_n += 1
        left, right = int(self.job('L')), int(self.job('R'))
        thrust, fire = self.job('T'), self.job('F')

        # 1. the ship
        if not self.in_play():
            # Turn presses clear on any step the ship is not in play. CHOICE: judged at step 1, so the
            # return step clears them and the loss step does not (a press pending then dies next step).
            self.latch.discard('L')
            self.latch.discard('R')
        else:
            if self.turn > 0:
                self.turn -= 1
            if self.turn == 0:
                if right - left != 0:
                    self.a = (self.a + right - left) % 24
                    self.turn = 2
                # A turn step uses the turn presses: a turn, or both keys cancelling, clears them.
                # With neither key on there is no press to clear.
                self.latch.discard('L')
                self.latch.discard('R')
            if thrust:
                self.vx += cdiv(m['T'] * SIN[self.a], 16384)
                self.vy -= cdiv(m['T'] * COS[self.a], 16384)
            self.vx -= cdiv(self.vx, 32)
            self.vy -= cdiv(self.vy, 32)
            self.vx = max(-m['V'], min(m['V'], self.vx))
            self.vy = max(-m['V'], min(m['V'], self.vy))
            self.x = wrap_x(self.x + self.vx)
            self.y = wrap_y(self.y + self.vy)

        # 2. the rocks
        for rk in self.rocks:
            if rk is not None:
                rk[1] = wrap_x(rk[1] + rk[3])
                rk[2] = wrap_y(rk[2] + rk[4])

        # 3. the shots, oldest first, each in two halves; then one may fire
        kept = []
        for s in self.shots:
            hx, hy = cdiv(s[2], 2), cdiv(s[3], 2)       # CHOICE: first half v/2 (C's /), second v - v/2
            s[0], s[1] = wrap_x(s[0] + hx), wrap_y(s[1] + hy)
            if self.shot_hits(s):
                continue
            s[0], s[1] = wrap_x(s[0] + s[2] - hx), wrap_y(s[1] + s[3] - hy)
            if self.shot_hits(s):
                continue
            s[4] -= 1                                   # CHOICE: life counts down here, after its move
            if s[4] > 0:
                kept.append(s)
        self.shots = kept
        if self.in_play() and fire and self.cool == 0 and len(self.shots) < m['shots']:
            sa, ca = SIN[self.a], COS[self.a]
            self.shots.append([wrap_x(self.x + 32 * sa), wrap_y(self.y - 32 * ca),
                               28 * sa + self.vx, -28 * ca + self.vy, m['life']])
            self.cool = m['cool']

        # 4. the ship against the rocks
        lost_now = False
        if self.in_play() and self.shield == 0:
            for i in range(NSLOT):
                rk = self.rocks[i]
                if rk is not None and within(self.x, self.y, rk[1], rk[2], m['radius'][rk[0]] + 6):
                    self.break_rock(i)
                    self.lose_ship()
                    lost_now = True
                    break

        # 5. the timers
        if self.cool > 0:
            self.cool -= 1
        if self.shield > 0 and self.lost == 0:   # only while the ship is in play (fine print 11)
            self.shield -= 1
        if self.lost > 0 and not lost_now:      # CHOICE: the burst counts from the step after the loss
            self.lost -= 1
            if self.lost == 0:
                if self.ships == 0:
                    self.over = 1
                else:
                    self.x, self.y = 200 * Q, 114 * Q
                    self.vx = self.vy = 0
                    self.a = 0
                    self.shield = m['shield']
        if not self.over:
            if self.pause > 0:
                self.pause -= 1
                if self.pause == 0:
                    self.wave += 1
                    self.make_wave()
            elif self.lost == 0 and all(r is None for r in self.rocks):
                self.shots = []
                self.pause = 40
            if self.lost == 0:
                self.clock += 1

        # Thrust and fire latches clear after every step; turn presses wait for a turn step.
        self.latch.discard('T')
        self.latch.discard('F')
        print(self.line())

    def lose_ship(self):
        self.lost = 20
        if self.mode == 'g' and self.clock < 1200:
            return
        self.ships -= 1

    # ---- output ----
    def line(self):
        shots = ';'.join('%d,%d,%d,%d,%d' % tuple(s) for s in self.shots) or '-'
        rocks = '/'.join('-' if r is None else '%s,%d,%d,%d,%d,%d' % tuple(r) for r in self.rocks)
        return ('S step=%d acc=%d x=%d y=%d vx=%d vy=%d a=%d turn=%d cool=%d shield=%d lost=%d ships=%d '
                'score=%d next=%d wave=%d pause=%d clock=%d over=%d shots=%s rocks=%s' % (
                    self.step_n, self.acc, self.x, self.y, self.vx, self.vy, self.a, self.turn, self.cool,
                    self.shield, self.lost, self.ships, self.score, self.next, self.wave, self.pause,
                    self.clock, self.over, shots, rocks))


SCALARS = 'x y vx vy a turn cool shield lost ships score next wave pause clock acc'.split()


def parse_rock(code):
    if code == '-':
        return None
    p = code.split(',')
    if len(p) != 6 or p[0] not in ('L', 'M', 'S'):
        raise ValueError('bad rock code ' + code)
    return [p[0]] + [int(v) for v in p[1:]]


def run(lines):
    g = None
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        w = line.split()
        cmd = w[0]
        if cmd == 'game':
            g = Game(w[1], int(w[2]), int(w[3]))
        elif cmd in ('down', 'up'):
            g.key(w[1], cmd == 'down')
        elif cmd == 'tick':
            if g.over:
                continue
            g.acc += min(int(w[1]), 100)
            while g.acc >= 50 and not g.over:
                g.acc -= 50
                g.step()
        elif cmd == 'steps':
            for _ in range(int(w[1])):
                if g.over:
                    break
                g.step()
        elif cmd == 'set':
            if w[1] not in SCALARS:
                raise ValueError('unknown scalar ' + w[1])
            setattr(g, w[1], int(w[2]))
        elif cmd == 'rock':
            g.rocks[int(w[1])] = parse_rock(w[2])
        elif cmd == 'clear':
            if w[1] == 'rocks':
                g.rocks = [None] * NSLOT
            elif w[1] == 'shots':
                g.shots = []
            else:
                raise ValueError('bad clear ' + w[1])
        elif cmd == 'shot':
            g.shots.append([int(v) for v in w[1:6]])
        elif cmd == 'draw?':
            print('draw %d' % g.draw(1000))
        else:
            raise ValueError('unknown command ' + line)


def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            run(f.readlines())
    else:
        run(sys.stdin.readlines())


if __name__ == '__main__':
    main()
