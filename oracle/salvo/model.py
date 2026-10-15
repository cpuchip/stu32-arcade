"""Salvo's rules from docs/salvo.md alone, all integers: the clock, the keys, the ship, the shots, the formation,
the dives, the enemy shots, the hits and the timers.

Nothing here reads the C. tests/salvo_trace.c prints the same lines; the format is oracle/salvo/TRACE.md.
Script lines (blank lines and lines starting with # are skipped):
  game SEED GAME        a new game: SEED app_seed(), GAME the games started before it; the draw state is
                        mix64(SEED ^ mix64(GAME)), then the wave's start
  down K / up K         a key's down or up, K one of 4 S1 6 S6 8 E 9 X P (4, SOFT1, 6, SOFT6, 8, ENTER, 9, x, +)
  tick MS               MS ms (at most 100) into the accumulator; a step runs while it holds 50
  steps N               N steps straight away, the accumulator untouched
  set NAME VALUE        a scalar by its trace name (sx lives score wave over ship wavet fx fy fdir sway dive cool
                        next acc)
  alien I CODE          alien I (slot order, r * 8 + c) to S, X, D<x>,<y>,<k>,<vx>,<f> or R<x>,<y>
  shot X Y / eshot X Y  a shot or an enemy shot added as the newest
  clear shots / clear eshots
  draw?                 prints 'draw N', N the next draw(1000), consuming it
Every step prints its state line. Once the game is over, ticks and steps run nothing and print nothing.

Where the doc still leaves a choice, the line says 'reading:'.
"""
import sys

M64 = (1 << 64) - 1

STEP_MS, TICK_MAX = 50, 100
TOP, BOTTOM = 12, 240

SHIP_Y, SHIP_W, SHIP_H = 228, 15, 8
BOX_DX, BOX_Y, BOX_W, BOX_H = 2, 230, 11, 6     # the ship for hits: (sx + 2, 230, 11, 6)
SX0, SX_MAX, SHIP_DX = 192, 385, 5
LIVES0, LIVES_MAX, EXTRA = 3, 5, 10000
SHIP_TIMER = 40
RETURN_DIVE = 20

SHOT_W, SHOT_H, SHOT_DY = 1, 6, 12
SWEPT_H = SHOT_H + SHOT_DY      # a moved shot's path, new top to old bottom: (x, y, 1, 18)
SHOT_DX0, SHOT_Y0 = 7, 222
MAX_SHOTS, COOL = 2, 6

ROWS, COLS = 4, 8
ALIEN_W, ALIEN_H = 13, 10
COL_DX, ROW_DY = 24, 16
FX0, FY0, FY_HOME, ENTRY_DY = 108, -60, 20, 4
SWAY_EVERY, SWAY_DX, FX_MIN, FX_MAX = 4, 4, 8, 208
SCORES = (150, 80, 50, 50)          # by row: flagship, escort, drone, drone

DIVE_DY, VX_EVERY, VX_DIV, VX_MAX, DIVER_X_MAX = 5, 4, 12, 3, 387
FIRE_Y_MAX, MAX_ESHOTS = 180, 6
RET_Y, RET_DY = -10, 4

ESHOT_W, ESHOT_H, ESHOT_DY = 2, 4, 6
ESHOT_DX, ESHOT_DYOFF = 6, 10
WAVE_TIMER = 30

LEFT_KEYS = ('4', 'S1')
RIGHT_KEYS = ('6', 'S6')
FIRE_KEYS = ('8', 'E', '9', 'X', 'P')
KEYS = LEFT_KEYS + RIGHT_KEYS + FIRE_KEYS

SCALARS = ('sx', 'lives', 'score', 'wave', 'over', 'ship', 'wavet', 'fx', 'fy', 'fdir', 'sway', 'dive', 'cool',
           'next', 'acc')


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def tdiv(a, b):
    """C's / on ints: truncated toward zero."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def overlap(x1, y1, w1, h1, x2, y2, w2, h2):
    return x1 < x2 + w2 and x2 < x1 + w1 and y1 < y2 + h2 and y2 < y1 + h1


def dive_gap(wave):
    return max(12, 40 - 4 * (wave - 1))


def dive_limit(wave):
    return min(4, 1 + wave)


class Salvo:
    """One game. An alien is ['S'], ['X'], ['D', x, y, k, vx, f] (f -1 once fired) or ['R', x, y]."""

    def __init__(self, seed, game):
        self.x = mix64((seed ^ mix64(game)) & M64)
        self.step_n = 0
        self.acc = 0
        self.sx = SX0
        self.lives = LIVES0
        self.score = 0
        self.next = EXTRA
        self.wave = 1
        self.over = 0
        self.ship = 0
        self.wavet = 0
        self.cool = 0
        self.shots = []
        self.eshots = []
        self.held = set()
        self.latched = set()
        self.ship_hit = False
        self.fired = False              # a shot appeared this step (the newest)
        self.wave_start()

    # ---- the draws -------------------------------------------------------------------------------------------
    def draw(self, n):
        self.x = mix64(self.x)
        return self.x % n

    # ---- the formation's places ------------------------------------------------------------------------------
    def slot_x(self, i):
        return self.fx + COL_DX * (i % COLS)

    def slot_y(self, i):
        return self.fy + ROW_DY * (i // COLS)

    def alien_rect(self, i):
        a = self.aliens[i]
        if a[0] == 'S':
            return self.slot_x(i), self.slot_y(i)
        return a[1], a[2]

    def alive(self):
        return self.ship == 0 and not self.over

    def wave_start(self):
        """A wave's start: fx 108, fy -60, rightward, the sway's count and the dive counter 0, every alien in its
        slot, its entry from the next step. Shots already flying are kept."""
        self.fx, self.fy, self.fdir = FX0, FY0, 1
        self.sway = 0
        self.dive = 0
        self.aliens = [['S'] for _ in range(ROWS * COLS)]

    # ---- the keys ----------------------------------------------------------------------------------------------
    def key_down(self, k):
        self.held.add(k)
        self.latched.add(k)

    def key_up(self, k):
        self.held.discard(k)

    def on(self, keys):
        return any(k in self.held or k in self.latched for k in keys)

    # ---- one step ------------------------------------------------------------------------------------------------
    def step(self):
        self.step_n += 1
        self.ship_hit = False
        self.fired = False
        left, right, fire = self.on(LEFT_KEYS), self.on(RIGHT_KEYS), self.on(FIRE_KEYS)
        self.the_ship(left, right)
        self.the_shots(fire)
        swayed = self.the_formation()
        self.the_aliens(swayed)
        self.the_enemy_shots()
        self.the_hits()
        self.the_timers()
        self.latched.clear()

    # 1. the ship moves (if it is alive)
    def the_ship(self, left, right):
        if not self.alive():
            return
        if left and not right:
            self.sx = clamp(self.sx - SHIP_DX, 0, SX_MAX)
        elif right and not left:
            self.sx = clamp(self.sx + SHIP_DX, 0, SX_MAX)

    # 2. the shots move, then one may be fired
    def the_shots(self, fire):
        # the cooldown counts down at the start of the shots (never below 0), so the firing step shows 6
        if self.cool > 0:
            self.cool -= 1
        # a shot whose bottom reaches the top line stays until after the hits (step 6), so it counts here
        self.shots = [(x, y - SHOT_DY) for (x, y) in self.shots]
        if self.alive() and fire and self.cool == 0 and len(self.shots) < MAX_SHOTS:
            self.shots.append((self.sx + SHOT_DX0, SHOT_Y0))
            self.cool = COOL
            self.fired = True

    # 3. the formation: the entry, or the sway. Returns True when the sway ran (the entry has ended).
    def the_formation(self):
        if self.fy < FY_HOME:
            self.fy += ENTRY_DY
            if self.fy >= FY_HOME:          # '20 or more' ends the entry
                self.sway = 0
                self.dive = dive_gap(self.wave)
            return False
        self.sway += 1
        if self.sway % SWAY_EVERY == 0:
            nx = self.fx + SWAY_DX * self.fdir
            if nx < FX_MIN or nx > FX_MAX:
                self.fdir = -self.fdir
                nx = self.fx + SWAY_DX * self.fdir
            self.fx = nx
        return True

    # 4. the aliens: each returning one, then each diving one, by slot order; then the dive counter
    def the_aliens(self, swayed):
        for i, a in enumerate(self.aliens):
            if a[0] == 'R':
                self.returning(i, a)
        for i, a in enumerate(self.aliens):
            if a[0] == 'D':
                self.diving(i, a)
        # the counter runs in each later step of the wave after its entry (steps where the sway ran),
        # while the ship is alive
        if swayed and self.alive():
            self.dive_counter()

    def returning(self, i, a):
        a[1] = self.slot_x(i)
        a[2] += RET_DY
        if a[2] >= self.slot_y(i):
            self.aliens[i] = ['S']

    def diving(self, i, a):
        _, x, y, k, vx, f = a
        if k % VX_EVERY == 0:
            vx = clamp(tdiv(self.sx + 1 - x, VX_DIV), -VX_MAX, VX_MAX)
        x = clamp(x + vx, 0, DIVER_X_MAX)
        y += DIVE_DY
        # the fire test uses y after this step's move; a refused shot is not tried again
        if k == f and y < FIRE_Y_MAX and len(self.eshots) < MAX_ESHOTS:
            self.eshots.append((x + ESHOT_DX, y + ESHOT_DYOFF))
            f = -1
        k += 1
        if y >= BOTTOM:
            self.aliens[i] = ['R', self.slot_x(i), RET_Y]
        else:
            self.aliens[i] = ['D', x, y, k, vx, f]

    def dive_counter(self):
        self.dive -= 1
        if self.dive > 0:                   # reading: 'at 0' as 'at or below 0', so a hand-set 0 cannot stall it
            return
        self.dive = dive_gap(self.wave)
        diving = sum(1 for a in self.aliens if a[0] == 'D')   # returning ones do not count
        if diving >= dive_limit(self.wave):
            return
        c = self.draw(COLS)                 # drawn whenever a dive may start, even with no alien in a slot
        for dc in range(COLS):
            col = (c + dc) % COLS
            for r in range(ROWS - 1, -1, -1):
                i = r * COLS + col
                if self.aliens[i][0] == 'S':
                    f = 6 + self.draw(10)
                    self.aliens[i] = ['D', self.slot_x(i), self.slot_y(i), 0, 0, f]
                    return

    # 5. the enemy shots move
    def the_enemy_shots(self):
        # the one just fired in step 4 moves here too
        self.eshots = [(x, y + ESHOT_DY) for (x, y) in self.eshots if y + ESHOT_DY < BOTTOM]

    # 6. the hits, then the shots gone at the top are removed
    def the_hits(self):
        kept = []
        last = len(self.shots) - 1
        for n, (x, y) in enumerate(self.shots):
            h = SHOT_H if (self.fired and n == last) else SWEPT_H   # a new shot is its own 1 x 6
            if not self.shot_hits(x, y, h):
                kept.append((x, y))
        self.shots = kept
        if self.alive():
            self.ship_hits()
        self.shots = [(x, y) for (x, y) in self.shots if y + SHOT_H > TOP]   # gone once its bottom is at or above 12

    def shot_hits(self, x, y, h):
        """Of the aliens the box overlaps, the lowest on the screen (largest y; a tie to the first in slot order)."""
        best = None
        for i, a in enumerate(self.aliens):
            if a[0] == 'X':
                continue
            ax, ay = self.alien_rect(i)
            if overlap(x, y, SHOT_W, h, ax, ay, ALIEN_W, ALIEN_H) and (best is None or ay > best[0]):
                best = (ay, i)
        if best is None:
            return False
        i = best[1]
        pts = SCORES[i // COLS]
        if self.aliens[i][0] == 'D':        # doubled for a diving one, not a returning one
            pts *= 2
        self.aliens[i] = ['X']
        self.add_score(pts)
        return True

    def add_score(self, pts):
        self.score += pts
        while self.score >= self.next:      # 'reaches'; at 5 lives the ship is not kept for later
            self.next += EXTRA
            if self.lives < LIVES_MAX:
                self.lives += 1

    def ship_hits(self):
        bx = self.sx + BOX_DX
        for (x, y) in self.eshots:
            if overlap(x, y, ESHOT_W, ESHOT_H, bx, BOX_Y, BOX_W, BOX_H):
                self.lose_ship()
                return
        for i, a in enumerate(self.aliens):
            if a[0] == 'D' and overlap(a[1], a[2], ALIEN_W, ALIEN_H, bx, BOX_Y, BOX_W, BOX_H):
                self.aliens[i] = ['X']      # dies too, scoring nothing
                self.lose_ship()
                return

    def lose_ship(self):
        """A lost ship: lives down, every shot and enemy shot gone, every diver and returner back in its slot."""
        self.lives -= 1
        self.shots = []
        self.eshots = []
        for i, a in enumerate(self.aliens):
            if a[0] in ('D', 'R'):
                self.aliens[i] = ['S']
        self.ship = SHIP_TIMER
        self.ship_hit = True

    # 7. the timers: the ship's, then the wave's
    def the_timers(self):
        if self.ship > 0 and not self.ship_hit:
            self.ship -= 1
            if self.ship == 0:
                if self.lives > 0:
                    self.sx = SX0
                    self.dive = RETURN_DIVE
                else:
                    self.over = 1
                    return
        if self.wavet > 0:
            self.wavet -= 1
            if self.wavet == 0:
                self.wave += 1
                self.wave_start()
        elif self.ship == 0 and all(a[0] == 'X' for a in self.aliens):
            self.shots = []
            self.eshots = []
            self.wavet = WAVE_TIMER

    # ---- the trace ---------------------------------------------------------------------------------------------
    def line(self):
        def pairs(ps):
            return ';'.join(f'{x},{y}' for (x, y) in ps) or '-'

        def code(a):
            if a[0] == 'D':
                return 'D' + ','.join(str(v) for v in a[1:])
            if a[0] == 'R':
                return f'R{a[1]},{a[2]}'
            return a[0]

        return (f'S step={self.step_n} acc={self.acc} sx={self.sx} lives={self.lives} score={self.score} '
                f'next={self.next} wave={self.wave} over={self.over} ship={self.ship} wavet={self.wavet} '
                f'fx={self.fx} fy={self.fy} fdir={self.fdir} sway={self.sway} dive={self.dive} cool={self.cool} '
                f'shots={pairs(self.shots)} eshots={pairs(self.eshots)} '
                f'aliens={"/".join(code(a) for a in self.aliens)}')


def parse_alien(s):
    if s in ('S', 'X'):
        return [s]
    v = [int(t) for t in s[1:].split(',')]
    if s[0] == 'D' and len(v) == 5:
        return ['D'] + v
    if s[0] == 'R' and len(v) == 2:
        return ['R'] + v
    raise ValueError(f'bad alien code {s!r}')


def run(lines):
    out = []
    g = None
    for line in lines:
        w = line.split()
        if not w or w[0].startswith('#'):
            continue
        cmd = w[0]
        if cmd == 'game':
            g = Salvo(int(w[1]), int(w[2]))
        elif cmd in ('down', 'up'):
            if w[1] not in KEYS:
                raise ValueError(f'bad key {w[1]!r}')
            (g.key_down if cmd == 'down' else g.key_up)(w[1])
        elif cmd == 'tick':
            if g.over:
                continue
            g.acc += min(int(w[1]), TICK_MAX)
            while g.acc >= STEP_MS and not g.over:
                g.acc -= STEP_MS            # 50 is taken off, then the step runs
                g.step()
                out.append(g.line())
        elif cmd == 'steps':
            for _ in range(int(w[1])):
                if g.over:
                    break
                g.step()
                out.append(g.line())
        elif cmd == 'set':
            if w[1] not in SCALARS:
                raise ValueError(f'bad scalar {w[1]!r}')
            setattr(g, w[1], int(w[2]))
        elif cmd == 'alien':
            g.aliens[int(w[1])] = parse_alien(w[2])
        elif cmd == 'shot':
            g.shots.append((int(w[1]), int(w[2])))
        elif cmd == 'eshot':
            g.eshots.append((int(w[1]), int(w[2])))
        elif cmd == 'clear':
            if w[1] == 'shots':
                g.shots = []
            elif w[1] == 'eshots':
                g.eshots = []
            else:
                raise ValueError(f'bad clear {w[1]!r}')
        elif cmd == 'draw?':
            out.append(f'draw {g.draw(1000)}')
        else:
            raise ValueError(f'bad command {cmd!r}')
    return out


if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        out = run(f.read().splitlines())
    if out:
        print('\n'.join(out))
