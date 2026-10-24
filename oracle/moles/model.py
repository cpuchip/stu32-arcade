#!/usr/bin/env python3
"""MOLES reference model, written from docs/moles.md and oracle/moles/TRACE.md alone.

Usage: python3 oracle/moles/model.py [SCRIPT]   (stdin when no SCRIPT)

Integers only, standard library only. The choices this model makes where the rules are silent are written next to
the code that makes them (search for "CHOICE").
"""
import sys

M64 = (1 << 64) - 1

# the holes' order, 7 8 9 4 5 6 1 2 3: key -> hole index
KEY_HOLE = {'7': 0, '8': 1, '9': 2, '4': 3, '5': 4, '6': 5, '1': 6, '2': 7, '3': 8}
OTHER_KEYS = {'0', 'E', 'P'}

RISE_STEPS = 4
BONK_STEPS = 8
SINK_STEPS = 4
STUN_STEPS = 6

G_WAIT0, G_WAIT_MIN, FIRST_DUE = 30, 10, 20
F_STAY0, F_STAY_MIN = 50, 16
F_GAP0, F_GAP2, F_LIVES0, F_TWO_AT = 20, 14, 3, 20

SCALARS = ('wait', 'stay', 'gap', 'due', 'bonks', 'score', 'lives', 'stun', 'last', 'acc')


def mix64(z):
    z = (z + 0x9e3779b97f4a7c15) & M64
    z = ((z ^ (z >> 30)) * 0xbf58476d1ce4e5b9) & M64
    z = ((z ^ (z >> 27)) * 0x94d049bb133111eb) & M64
    return z ^ (z >> 31)


class Game:
    def __init__(self, mode, seed, game):
        self.full = (mode == 'f')
        self.x = mix64((seed ^ mix64(game & M64)) & M64)
        self.step = 0
        self.acc = 0
        self.score = 0
        self.bonks = 0
        self.lives = F_LIVES0 if self.full else 0
        # CHOICE: the other mode's parameters print as 0 (gentle has no stay or gap, full no wait).
        self.wait = 0 if self.full else G_WAIT0
        self.stay = F_STAY0 if self.full else 0
        self.gap = F_GAP0 if self.full else 0
        # CHOICE: the first mole is due 20 steps after the start in both modes (rises in step 20).
        self.due = FIRST_DUE
        self.stun = 0
        self.last = -1
        self.over = 0
        self.holes = [('E', 0)] * 9      # (state letter, steps so far)
        self.queue = []                  # holes of the downs since the last step, in order
        self.held = set()

    def draw(self, n):
        self.x = mix64(self.x)
        return self.x % n

    # ---- keys -------------------------------------------------------------------------------------------
    def down(self, k):
        if k in KEY_HOLE:
            # CHOICE: a down of a key already held (no up since) is a repeat and bonks nothing.
            if k in self.held:
                return
            self.held.add(k)
            self.queue.append(KEY_HOLE[k])
        # 0, E, P do nothing in play

    def up(self, k):
        self.held.discard(k)

    # ---- the step ---------------------------------------------------------------------------------------
    def active(self):
        return sum(1 for s, _ in self.holes if s in 'RUB')

    def run_step(self):
        self.step += 1
        fresh = set()          # holes bonked this step: their timer starts counting next step
        stun_fresh = False     # a stun started this step: it starts counting down next step

        # 1. the bonks, in the order they came
        keys, self.queue = self.queue, []
        for h in keys:
            if self.stun > 0:
                continue       # ignored, and does not extend the stun
            s, n = self.holes[h]
            if s in 'RU':
                self.holes[h] = ('B', 0)
                fresh.add(h)
                self.score += 1
                self.bonks += 1
                if self.full:
                    self.stay = max(F_STAY_MIN, self.stay - 1)
                    if self.bonks >= F_TWO_AT:
                        self.gap = F_GAP2
            elif self.full:
                self.stun = STUN_STEPS
                stun_fresh = True

        # 2. the timers: the due count, the holes in order, the stun
        if self.due > 0:
            self.due -= 1
        got_away = False
        for i in range(9):
            if i in fresh:
                continue
            s, n = self.holes[i]
            if s == 'E':
                continue
            n += 1
            if s == 'R':
                self.holes[i] = ('U', 0) if n >= RISE_STEPS else ('R', n)
            elif s == 'U':
                if self.full and n >= self.stay:
                    self.holes[i] = ('D', 0)
                    self.lives = max(0, self.lives - 1)
                    got_away = True
                else:
                    self.holes[i] = ('U', n)
            elif s == 'B':
                self.holes[i] = ('D', 0) if n >= BONK_STEPS else ('B', n)
            elif s == 'D':
                if n >= SINK_STEPS:
                    self.holes[i] = ('E', 0)
                    self.last = i
                    if not self.full:
                        self.due = self.wait
                else:
                    self.holes[i] = ('D', n)
        if self.stun > 0 and not stun_fresh:
            self.stun -= 1
        if self.full and got_away and self.lives == 0:
            for i in range(9):
                if self.holes[i][0] in 'RUB':
                    self.holes[i] = ('D', 0)
            self.over = 1
            return

        # 3. a new mole if one is due
        if self.due != 0:
            return
        if self.full:
            if self.active() >= (2 if self.bonks >= F_TWO_AT else 1):
                return         # CHOICE: due stays 0 and the mole rises on the first step it may
        else:
            if any(s != 'E' for s, _ in self.holes):
                return
        cands = [i for i in range(9) if self.holes[i][0] == 'E' and i != self.last]
        if not cands:
            return             # CHOICE: no draw, nothing rises, due stays 0
        h = cands[self.draw(len(cands))]
        self.holes[h] = ('R', 0)
        if self.full:
            self.due = self.gap
        elif self.last != -1:
            # CHOICE: W drops at each rise that ended a wait, i.e. every mole after the first
            self.wait = max(G_WAIT_MIN, self.wait - 1)

    def line(self):
        holes = '/'.join('E' if s == 'E' else '%s%d' % (s, n) for s, n in self.holes)
        return ('S step=%d acc=%d score=%d bonks=%d lives=%d wait=%d stay=%d gap=%d due=%d stun=%d last=%d '
                'over=%d holes=%s' % (self.step, self.acc, self.score, self.bonks, self.lives, self.wait, self.stay,
                                      self.gap, self.due, self.stun, self.last, self.over, holes))


def parse_hole(code):
    if code == 'E':
        return ('E', 0)
    if len(code) >= 2 and code[0] in 'RUBD':
        return (code[0], int(code[1:]))
    raise ValueError('bad hole code %r' % code)


def run(lines, out):
    g = None
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        w = line.split()
        cmd = w[0]
        try:
            if cmd == 'game':
                if w[1] not in ('g', 'f'):
                    raise ValueError('mode must be g or f')
                g = Game(w[1], int(w[2]), int(w[3]))
                continue
            if g is None:
                raise ValueError('no game yet')
            if cmd == 'down':
                if not g.over:
                    g.down(w[1])
            elif cmd == 'up':
                g.up(w[1])
            elif cmd == 'tick':
                if g.over:
                    continue
                g.acc += min(int(w[1]), 100)
                n = 0
                while g.acc >= 50 and n < 2 and not g.over:
                    g.acc -= 50
                    n += 1
                    g.run_step()
                    out.write(g.line() + '\n')
            elif cmd == 'steps':
                for _ in range(int(w[1])):
                    if g.over:
                        break
                    g.run_step()
                    out.write(g.line() + '\n')
            elif cmd == 'draw?':
                out.write('draw %d\n' % g.draw(1000))
            elif cmd == 'set':
                if w[1] not in SCALARS:
                    raise ValueError('unknown scalar %r' % w[1])
                setattr(g, w[1], int(w[2]))
            elif cmd == 'hole':
                g.holes[int(w[1])] = parse_hole(w[2])
            else:
                raise ValueError('unknown command %r' % cmd)
        except (IndexError, ValueError) as e:
            sys.stderr.write('line %d: %s: %s\n' % (lineno, line, e))
            sys.exit(2)


def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            lines = f.read().splitlines()
    else:
        lines = sys.stdin.read().splitlines()
    run(lines, sys.stdout)


if __name__ == '__main__':
    main()
