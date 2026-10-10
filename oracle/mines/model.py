"""Mines' rules from docs/mines.md alone: the field, opening, the chord, flags, dealing and the game's solver.

Nothing here reads the C. tests/mines_trace.c prints the same lines, and oracle/mines_check.sh compares them.
Script lines:
  game SEED N BOARD RULES  a new game: the seed (as app_seed() << 32 gives it), the game number, the board (easy,
                      medium, hard) and the rules (noguess, classic); dealt on its first open
  field BOARD C,C,... a hand-made game on BOARD: these cells are the mines, already dealt (nothing is safe for free)
  open C / flag C     the player's open (the chord included) and flag, at cell C
  deal F / step D     a deal started at F, as the app's first open does it / up to D more attempts of it
  solve F             the game's solver on the current mines from opening F: 'solve F cleared|stuck OPENED'
  cell C / mine C     cell C's state (open, flag, closed) / whether it is a mine
  hit                 the mine shown as opened at a loss, or -1
"""
import os
import sys

TRIES = int(os.environ.get('MINES_TRIES', 400))  # a check sets it lower, to reach the fallback
M64 = (1 << 64) - 1
ORDER = ((-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1))


class Board:
    def __init__(self, name, w, h, n):
        self.name, self.W, self.H, self.N = name, w, h, n
        self.CELLS = w * h
        self.NB = [[(y + dy) * w + x + dx for dx, dy in ORDER if 0 <= x + dx < w and 0 <= y + dy < h]
                   for y in range(h) for x in range(w)]


BOARDS = {name: Board(name, *size) for name, size in
          (('easy', (9, 9, 10)), ('medium', (16, 16, 40)), ('hard', (30, 16, 99)))}


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def counts(b, mines):
    return [sum(n in mines for n in b.NB[c]) for c in range(b.CELLS)]


def flood(b, opened, flags, num, c):
    """open c and, from each 0, its closed unflagged neighbours"""
    stack = [c]
    while stack:
        c = stack.pop()
        if c in opened:
            continue
        opened.add(c)
        if num[c] == 0:
            for n in reversed(b.NB[c]):
                if n not in opened and n not in flags:
                    stack.append(n)


def solver(b, mines, f):
    """the game's solver (docs/mines.md): (cleared, the cells it opened)"""
    num = counts(b, mines)
    opened, mark = set(), set()
    flood(b, opened, mark, num, f)
    while True:
        changed = True
        while changed:                       # A, sweeps in row order on the changing state
            changed = False
            for c in range(b.CELLS):
                if c not in opened or num[c] == 0:
                    continue
                U = [n for n in b.NB[c] if n not in opened and n not in mark]
                if not U:
                    continue
                k = sum(n in mark for n in b.NB[c])
                if k == num[c]:
                    for n in U:
                        if n not in opened:
                            flood(b, opened, mark, num, n)
                    changed = True
                elif k + len(U) == num[c]:
                    mark.update(U)
                    changed = True
        done = False                          # B, the first productive pair
        cons = []
        for c in range(b.CELLS):
            if c in opened and num[c]:
                U = frozenset(n for n in b.NB[c] if n not in opened and n not in mark)
                if U:
                    cons.append((U, num[c] - sum(n in mark for n in b.NB[c])))
        for ua, na in cons:
            for ub, nbv in cons:
                if ua < ub:
                    d = sorted(ub - ua)
                    nd = nbv - na
                    if nd == 0:
                        for n in d:
                            if n not in opened:
                                flood(b, opened, mark, num, n)
                        done = True
                    elif nd == len(d):
                        mark.update(d)
                        done = True
                if done:
                    break
            if done:
                break
        if done:
            continue
        rest = [c for c in range(b.CELLS) if c not in opened and c not in mark]   # C, the total
        if rest and len(mark) == b.N:
            for c in rest:
                if c not in opened:
                    flood(b, opened, mark, num, c)
            continue
        break
    return len(opened) == b.CELLS - b.N, opened


class Game:
    def __init__(self, seed, game, board, classic=False):
        self.b = BOARDS[board]
        self.classic = classic
        self.x = mix64((seed ^ mix64(game)) & M64)
        self.mines = None
        self.num = None
        self.opened, self.flags = set(), set()
        self.state = 'play'
        self.attempts = 0
        self.hit = None
        self.deal_f = None

    def draw(self, n):
        self.x = mix64(self.x)
        return self.x % n

    def deal_start(self, f):
        """the app's first open: True when a deal starts"""
        if self.mines is not None or self.deal_f is not None or self.state != 'play' or f in self.flags:
            return False
        self.deal_f = f
        self.attempts = 0
        return True

    def deal_step(self, d):
        """up to d more attempts: True once dealt"""
        b = self.b
        for _ in range(d):
            if self.mines is not None or self.deal_f is None:
                break
            f = self.deal_f
            banned = set(b.NB[f]) | {f}
            cells = [c for c in range(b.CELLS) if c not in banned]
            for i in range(b.N):
                j = i + self.draw(len(cells) - i)
                cells[i], cells[j] = cells[j], cells[i]
            mines = set(cells[:b.N])
            self.attempts += 1
            if self.classic or self.attempts == TRIES or solver(b, mines, f)[0]:
                self.set_mines(mines)
                self.deal_f = None
        return self.mines is not None

    def set_mines(self, mines):
        self.mines = set(mines)
        self.num = counts(self.b, self.mines)

    def open_one(self, c):
        """True when c is a mine"""
        if c in self.mines:
            return True
        flood(self.b, self.opened, self.flags, self.num, c)
        return False

    def open(self, c):
        if self.state != 'play' or c in self.flags or self.deal_f is not None:
            return
        if self.mines is None:
            self.deal_start(c)
            while not self.deal_step(1):
                pass
        if c not in self.opened:
            lost = self.open_one(c)
            self.hit = c if lost else None
        else:
            if sum(n in self.flags for n in self.b.NB[c]) != self.num[c]:
                return
            lost = False
            self.hit = None
            for n in self.b.NB[c]:
                if n not in self.opened and n not in self.flags:
                    if self.open_one(n):
                        lost = True
                        self.hit = self.hit if self.hit is not None else n
        if lost:
            self.state = 'lost'
        elif len(self.opened) == self.b.CELLS - len(self.mines):
            self.state = 'won'
            self.flags = set(self.mines)

    def flag(self, c):
        if self.state != 'play' or c in self.opened:
            return
        self.flags ^= {c}

    def line(self):
        def ck(cells):
            s = 0
            for c in cells:
                s = (s + (c + 1) * (c + 7)) & 0xFFFFFFFF
            return s
        mk = ck(self.mines) if self.mines is not None else 0
        return (f"state={self.state} opened={len(self.opened)} open_ck={ck(self.opened)} flags={len(self.flags)} "
                f"flag_ck={ck(self.flags)} mines_ck={mk} attempts={self.attempts} "
                f"dealt={int(self.mines is not None)} hit={-1 if self.hit is None else self.hit}")


def run(lines):
    out = []
    g = None
    for line in lines:
        w = line.split()
        if not w or w[0].startswith('#'):
            continue
        if w[0] == 'game':
            g = Game(int(w[1]), int(w[2]), w[3], w[4] == 'classic')
            out.append('game ' + g.line())
        elif w[0] == 'field':
            g = Game(0, 0, w[1])
            g.set_mines(int(c) for c in w[2].split(','))
            out.append('field ' + g.line())
        elif w[0] in ('deal', 'step'):
            g.deal_start(int(w[1])) if w[0] == 'deal' else g.deal_step(int(w[1]))
            out.append(f'{w[0]} {w[1]} ' + g.line())
        elif w[0] in ('open', 'flag'):
            getattr(g, w[0])(int(w[1]))
            out.append(f'{w[0]} {w[1]} ' + g.line())
        elif w[0] == 'cell':
            c = int(w[1])
            out.append(f"cell {c} {'open' if c in g.opened else 'flag' if c in g.flags else 'closed'}")
        elif w[0] == 'hit':
            out.append(f"hit {-1 if g.hit is None else g.hit}")
        elif w[0] == 'mine':
            out.append(f"mine {w[1]} {'yes' if int(w[1]) in g.mines else 'no'}")
        elif w[0] == 'solve':
            ok, opened = solver(g.b, g.mines, int(w[1]))
            out.append(f"solve {w[1]} {'cleared' if ok else 'stuck'} {len(opened)}")
    return out


if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        print('\n'.join(run(f.read().splitlines())))
