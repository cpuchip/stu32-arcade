#!/usr/bin/env python3
"""CATCH reference model, written from docs/catch.md and oracle/catch/TRACE.md alone (plus SALVO's clock, latch
and draw conventions from docs/salvo.md and oracle/salvo/TRACE.md). Not derived from the C.

Usage: python3 oracle/catch/model.py [SCRIPT]   (stdin when no script is named)

Units: bx and star x in px; star y in 1/16 px (the state line prints it so); fall in 1/16 px a step.
The choices this model makes where the doc is silent are marked CHOICE below.
"""
import sys

M64 = (1 << 64) - 1

STEP_MS = 50
TICK_CAP = 100

FIELD_H = 228          # px, the field's height under the 12 px top line
MOUTH_ROW = 202        # px, the basket's top row
BASKET_MAX = 352       # 400 - 48
BX_START = 176
STAR_W = 11
STAR_SPAWN_Y = -11 * 16
STAR_X_DRAW = 390      # the first star of a game: x = draw(390), so x is 0-389
STAR_X_MAX = 389       # the reach clamps to 0-389 (hi = min(389, last + r))

# the table in docs/catch.md "Stars": (start, each 10 catches, limit) for the levels that pace; gentle never paces
FALL_START = 32
GAP_START = 30
MAX_START = 3
PACE = {
    'm': {'fall': (32, 4, 48), 'gap': (30, -2, 18), 'max': (3, 1, 4)},
    'f': {'fall': (32, 8, 96), 'gap': (30, -3, 9), 'max': (3, 1, 6)},
}
LIVES = {'g': 0, 'm': 5, 'f': 3}
MOVE = {'g': 6, 'm': 8, 'f': 8}

LEFT_KEYS = ('4', 'S1')
RIGHT_KEYS = ('6', 'S6')
ALL_KEYS = ('4', 'S1', '6', 'S6', 'E', 'P')


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


class Catch:
    def __init__(self, mode, seed, game):
        if mode not in ('g', 'm', 'f'):
            raise ValueError('mode must be g, m or f: %r' % mode)
        self.mode = mode
        self.x = mix64((seed ^ mix64(game & M64)) & M64)
        self.step_n = 0
        self.acc = 0
        self.bx = BX_START
        self.score = 0
        self.lives = LIVES[mode]
        self.fall = FALL_START
        self.gap = GAP_START
        self.max = MAX_START
        # the gap counter starts at 0, so the first star comes on the game's first step (fine print 2)
        self.count = 0
        # the reach's last: the x of the last star that came, -1 while unset (fine print 11, TRACE.md)
        self.last = -1
        self.bump = 0
        self.over = 0
        self.stars = []            # [x px, y 1/16 px], oldest first
        self.held = set()
        self.latch_left = False
        self.latch_right = False

    def draw(self, n):
        if n <= 0:
            # only reachable through a `set` that makes no sense (a negative gap, last out of range)
            raise ValueError('draw of %d' % n)
        self.x = mix64(self.x)
        return self.x % n

    # keys
    def down(self, k):
        self.held.add(k)
        if k in LEFT_KEYS:
            self.latch_left = True
        elif k in RIGHT_KEYS:
            self.latch_right = True

    def up(self, k):
        self.held.discard(k)       # the latch stays until the next step

    # the clock
    def tick(self, ms, out):
        if self.over:
            return
        self.acc += min(max(ms, 0), TICK_CAP)
        while self.acc >= STEP_MS and not self.over:
            self.acc -= STEP_MS
            self.step()
            out(self.line())

    def steps(self, n, out):
        for _ in range(n):
            if self.over:
                return
            self.step()
            out(self.line())

    # one step
    def step(self):
        self.step_n += 1
        self.bump = 0

        # 1. the basket moves
        left = self.latch_left or any(k in self.held for k in LEFT_KEYS)
        right = self.latch_right or any(k in self.held for k in RIGHT_KEYS)
        self.latch_left = self.latch_right = False
        d = 0
        if left and not right:
            d = -MOVE[self.mode]
        elif right and not left:
            d = MOVE[self.mode]
        if d:
            old = self.bx
            nb = min(max(old + d, 0), BASKET_MAX)
            self.bx = nb
            # the bump: the move ended at a wall it was not already at (arrival, once)
            if nb != old and nb in (0, BASKET_MAX):
                self.bump = 1

        # 2. the stars fall, oldest first, each tested
        score_before = self.score
        keep = []
        stars = self.stars
        i = 0
        while i < len(stars):
            sx, sy = stars[i]
            i += 1
            ny = sy + self.fall
            bot_before = sy + STAR_W * 16
            bot_after = ny + STAR_W * 16
            if bot_before < MOUTH_ROW * 16 <= bot_after:
                if sx < self.bx + 44 and sx + STAR_W > self.bx + 4:
                    self.score += 1          # caught: gone
                    continue
            if ny >= FIELD_H * 16:            # a miss: gone
                if self.mode != 'g':
                    self.lives -= 1
                    if self.lives <= 0:
                        # every star is gone and the game is over; nothing else in this step runs
                        self.stars = []
                        self.over = 1
                        return
                continue
            keep.append([sx, ny])
        self.stars = keep

        # 3. the gap counter counts down (never below 0); at 0 a star comes if there is room
        if self.count > 0:
            self.count -= 1
        if self.count <= 0 and len(self.stars) < self.max:
            sx = self.reach_x()
            self.stars.append([sx, STAR_SPAWN_Y])
            self.last = sx
            self.count = self.gap

        # 4. the pace (middle and full): once for each multiple of 10 the score reached in this step, each number
        # to its own limit
        if self.mode in PACE:
            pace = PACE[self.mode]
            for _ in range(self.score // 10 - score_before // 10):
                self.fall = self.paced(self.fall, pace['fall'])
                self.gap = self.paced(self.gap, pace['gap'])
                self.max = self.paced(self.max, pace['max'])

    @staticmethod
    def paced(v, rule):
        _, d, lim = rule
        return min(v + d, lim) if d > 0 else max(v + d, lim)

    def reach_x(self):
        """The reach (docs/catch.md "The reach", fine print 11): one draw a star. With last unset, draw(390);
        else within r = 3 * move * gap / 4 of last, clamped to 0-389. gap is the current gap, before this step's
        pace (step 3 runs before step 4)."""
        if self.last < 0:
            return self.draw(STAR_X_DRAW)
        r = 3 * MOVE[self.mode] * self.gap // 4
        lo = max(0, self.last - r)
        hi = min(STAR_X_MAX, self.last + r)
        return lo + self.draw(hi - lo + 1)

    def line(self):
        stars = ';'.join('%d,%d' % (sx, sy) for sx, sy in self.stars) or '-'
        return ('S step=%d acc=%d bx=%d score=%d lives=%d fall=%d gap=%d max=%d count=%d last=%d bump=%d over=%d '
                'stars=%s'
                % (self.step_n, self.acc, self.bx, self.score, self.lives, self.fall, self.gap, self.max,
                   self.count, self.last, self.bump, self.over, stars))


SETTABLE = ('bx', 'score', 'lives', 'fall', 'gap', 'max', 'count', 'last', 'acc')


def run(lines, out):
    g = None
    for n, raw in enumerate(lines, 1):
        s = raw.strip()
        if not s or s.startswith('#'):
            continue
        w = s.split()
        cmd = w[0]
        try:
            if cmd == 'game':
                g = Catch(w[1], int(w[2], 0), int(w[3], 0))
                continue
            if g is None:
                raise ValueError('no game yet')
            if cmd in ('down', 'up'):
                if w[1] not in ALL_KEYS:
                    raise ValueError('unknown key %r' % w[1])
                (g.down if cmd == 'down' else g.up)(w[1])
            elif cmd == 'tick':
                g.tick(int(w[1]), out)
            elif cmd == 'steps':
                g.steps(int(w[1]), out)
            elif cmd == 'draw?':
                out('draw %d' % g.draw(1000))
            elif cmd == 'set':
                if w[1] not in SETTABLE:
                    raise ValueError('cannot set %r' % w[1])
                v = int(w[2])
                # CHOICE: last takes -1 (unset: the next star is draw(390)) or a star's x 0-389; anything else is
                # a script error rather than a draw the doc never defines
                if w[1] == 'last' and not -1 <= v <= STAR_X_MAX:
                    raise ValueError('last must be -1 or 0-%d' % STAR_X_MAX)
                setattr(g, w[1], v)
            elif cmd == 'star':
                g.stars.append([int(w[1]), int(w[2])])     # last is not changed (TRACE.md)
            elif cmd == 'clear' and w[1:] == ['stars']:
                g.stars = []                               # CHOICE: last is not changed either
            else:
                raise ValueError('unknown command')
        except (ValueError, IndexError) as e:
            sys.stderr.write('line %d: %s: %s\n' % (n, s, e))
            sys.exit(2)


def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            lines = f.read().splitlines()
    else:
        lines = sys.stdin.read().splitlines()
    buf = []
    run(lines, buf.append)
    sys.stdout.write('\n'.join(buf) + ('\n' if buf else ''))


if __name__ == '__main__':
    main()
