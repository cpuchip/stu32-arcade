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
# middle (docs/moles.md, "Middle", its table, fine print 4, 5 and 8): gentle's pacing (a mole rises only when `due` is 0
# and all nine holes are empty; every hole that empties sets `due` to W) with W fixed at 30, never dropping; full's
# stay, getaway, lives and end with these numbers; no gap (it reads 0) and no stun
M_STAY0, M_STAY_MIN, M_WAIT, M_LIVES0 = 80, 40, 30, 5

MAX_DOWNS = 16          # fine print 13: at most 16 downs kept for one step

SCALARS = ('wait', 'stay', 'gap', 'due', 'bonks', 'score', 'lives', 'stun', 'last', 'acc')


def mix64(z):
    z = (z + 0x9e3779b97f4a7c15) & M64
    z = ((z ^ (z >> 30)) * 0xbf58476d1ce4e5b9) & M64
    z = ((z ^ (z >> 27)) * 0x94d049bb133111eb) & M64
    return z ^ (z >> 31)


class Game:
    def __init__(self, mode, seed, game):
        self.full = (mode == 'f')
        self.middle = (mode == 'm')
        # timed: middle and full, the levels where a mole gets away (fine print 5: middle follows full's rules for the
        # stay, the getaway, the lives and the end). Paced by the wait: gentle and middle (fine print 4 and 5); full is
        # paced by the gap (fine print 8).
        self.timed = self.full or self.middle
        self.paced = not self.full
        self.x = mix64((seed ^ mix64(game & M64)) & M64)
        self.step = 0
        self.acc = 0
        self.score = 0
        self.bonks = 0
        self.stay_min = F_STAY_MIN if self.full else M_STAY_MIN
        # fine print 5: the fields a level does not use read 0 (gentle's stay and gap, middle's gap, full's wait).
        if self.full:
            self.lives, self.wait, self.stay, self.gap = F_LIVES0, 0, F_STAY0, F_GAP0
        elif self.middle:
            self.lives, self.wait, self.stay, self.gap = M_LIVES0, M_WAIT, M_STAY0, 0
        else:
            self.lives, self.wait, self.stay, self.gap = 0, G_WAIT0, 0, 0
        # fine print 3: the first mole is due 20 steps after the start in every level (rises in step 20).
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
            # fine print 13: downs after the 16th in one step are dropped. CHOICE: a dropped down still marks the
            # key held, so a second down before its up is a repeat; a repeat is not kept, so it doesn't count.
            if len(self.queue) < MAX_DOWNS:
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
                if self.timed:
                    self.stay = max(self.stay_min, self.stay - 1)
                    # fine print 7: the gap's change at 20 bonks is full's only; middle has no gap
                    if self.full and self.bonks >= F_TWO_AT:
                        self.gap = F_GAP2
            elif self.full:
                # the stun is full's only; in gentle and middle a wrong key is dust and changes nothing here
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
                if self.timed and n >= self.stay:
                    self.holes[i] = ('D', 0)
                    if self.lives > 0:
                        self.lives -= 1
                        # fine print 11: only a getaway that brings the lives to 0 ends a game
                        if self.lives == 0:
                            got_away = True
                else:
                    self.holes[i] = ('U', n)
            elif s == 'B':
                self.holes[i] = ('D', 0) if n >= BONK_STEPS else ('B', n)
            elif s == 'D':
                if n >= SINK_STEPS:
                    self.holes[i] = ('E', 0)
                    self.last = i
                    # fine print 4 and 5: in gentle and middle every hole that empties sets due to W (after a bonk
                    # or a getaway alike). CHOICE: middle reads W from the wait field, as gentle does.
                    if self.paced:
                        self.due = self.wait
                else:
                    self.holes[i] = ('D', n)
        if self.stun > 0 and not stun_fresh:
            self.stun -= 1
        if self.timed and got_away and self.lives == 0:
            for i in range(9):
                if self.holes[i][0] in 'RUB':
                    self.holes[i] = ('D', 0)
            self.over = 1
            return

        # 3. a new mole if one is due
        if self.due != 0:
            return
        if self.full:
            limit = 2 if self.bonks >= F_TWO_AT else 1
            if self.active() >= limit:
                return         # fine print 8: due stays 0 and the mole rises on the first step it may
        else:
            # fine print 4 (gentle and middle): only when all nine holes are empty, sinking ones included
            if any(s != 'E' for s, _ in self.holes):
                return
        cands = [i for i in range(9) if self.holes[i][0] == 'E' and i != self.last]
        if not cands:
            return             # CHOICE: no draw, nothing rises, due stays 0
        h = cands[self.draw(len(cands))]
        self.holes[h] = ('R', 0)
        if self.full:
            self.due = self.gap
        elif not self.middle and self.last != -1:
            # CHOICE: W drops at each rise that ended a wait, i.e. every mole after the first. Gentle only: middle's
            # W is 30 and never drops (fine print 5); due stays 0 while the mole is out in both (fine print 4).
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
                if w[1] not in ('g', 'm', 'f'):
                    raise ValueError('mode must be g, m or f')
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
