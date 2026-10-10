"""Mines' rules from docs/mines.md alone: the field, opening, the chord, flags, dealing and the game's solver.

Nothing here reads the C. tests/mines_trace.c prints the same lines, and oracle/mines_check.sh compares them.
Script lines:
  game SEED N         a new game: the seed (as app_seed() << 32 gives it) and the game number; dealt on its first open
  field C,C,...       a hand-made game: these cells are the mines, already dealt (nothing is safe for free)
  open C / flag C     the player's open (the chord included) and flag, at cell C
  solve F             the game's solver on the current mines from opening F: 'solve F cleared|stuck OPENED'
  cell C / mine C     cell C's state (open, flag, closed) / whether it is a mine
  hit                 the mine shown as opened at a loss, or -1
"""
import sys

W, H, MINES = 24, 13, 48
CELLS = W * H
TRIES = 100
M64 = (1 << 64) - 1
ORDER = ((-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1))
NB = [[(y + dy) * W + x + dx for dx, dy in ORDER if 0 <= x + dx < W and 0 <= y + dy < H]
      for y in range(H) for x in range(W)]


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


def counts(mines):
    return [sum(n in mines for n in NB[c]) for c in range(CELLS)]


def flood(opened, flags, num, c):
    """open c and, from each 0, its closed unflagged neighbours"""
    stack = [c]
    while stack:
        c = stack.pop()
        if c in opened:
            continue
        opened.add(c)
        if num[c] == 0:
            for n in reversed(NB[c]):
                if n not in opened and n not in flags:
                    stack.append(n)


def solver(mines, f):
    """the game's solver (docs/mines.md): (cleared, the cells it opened)"""
    num = counts(mines)
    opened, mark = set(), set()
    flood(opened, mark, num, f)
    while True:
        changed = True
        while changed:                       # A, sweeps in row order on the changing state
            changed = False
            for c in range(CELLS):
                if c not in opened or num[c] == 0:
                    continue
                U = [n for n in NB[c] if n not in opened and n not in mark]
                if not U:
                    continue
                k = sum(n in mark for n in NB[c])
                if k == num[c]:
                    for n in U:
                        if n not in opened:
                            flood(opened, mark, num, n)
                    changed = True
                elif k + len(U) == num[c]:
                    mark.update(U)
                    changed = True
        done = False                          # B, the first productive pair
        cons = []
        for c in range(CELLS):
            if c in opened and num[c]:
                U = frozenset(n for n in NB[c] if n not in opened and n not in mark)
                if U:
                    cons.append((U, num[c] - sum(n in mark for n in NB[c])))
        for ua, na in cons:
            for ub, nb in cons:
                if ua < ub:
                    d = sorted(ub - ua)
                    nd = nb - na
                    if nd == 0:
                        for n in d:
                            if n not in opened:
                                flood(opened, mark, num, n)
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
        rest = [c for c in range(CELLS) if c not in opened and c not in mark]   # C, the total
        if rest and len(mark) == MINES:
            for c in rest:
                if c not in opened:
                    flood(opened, mark, num, c)
            continue
        break
    return len(opened) == CELLS - MINES, opened


class Game:
    def __init__(self, seed, game):
        self.x = mix64((seed ^ mix64(game)) & M64)
        self.mines = None
        self.num = None
        self.opened, self.flags = set(), set()
        self.state = 'play'
        self.attempts = 0
        self.hit = None

    def draw(self, n):
        self.x = mix64(self.x)
        return self.x % n

    def deal(self, f):
        banned = set(NB[f]) | {f}
        allowed = [c for c in range(CELLS) if c not in banned]
        for t in range(1, TRIES + 1):
            cells = list(allowed)
            for i in range(MINES):
                j = i + self.draw(len(cells) - i)
                cells[i], cells[j] = cells[j], cells[i]
            mines = set(cells[:MINES])
            if solver(mines, f)[0] or t == TRIES:
                self.attempts = t
                return mines

    def set_mines(self, mines):
        self.mines = set(mines)
        self.num = counts(self.mines)

    def open_one(self, c):
        """True when c is a mine"""
        if c in self.mines:
            return True
        flood(self.opened, self.flags, self.num, c)
        return False

    def open(self, c):
        if self.state != 'play':
            return
        if c in self.flags:
            return
        if self.mines is None:
            self.set_mines(self.deal(c))
        if c not in self.opened:
            lost = self.open_one(c)
            self.hit = c if lost else None
        else:
            if sum(n in self.flags for n in NB[c]) != self.num[c]:
                return
            lost = False
            self.hit = None
            for n in NB[c]:
                if n not in self.opened and n not in self.flags:
                    if self.open_one(n):
                        lost = True
                        self.hit = self.hit if self.hit is not None else n
        if lost:
            self.state = 'lost'
        elif len(self.opened) == CELLS - len(self.mines):
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
                f"hit={-1 if self.hit is None else self.hit}")


def run(lines):
    out = []
    g = None
    for line in lines:
        w = line.split()
        if not w or w[0].startswith('#'):
            continue
        if w[0] == 'game':
            g = Game(int(w[1]), int(w[2]))
            out.append('game ' + g.line())
        elif w[0] == 'field':
            g = Game(0, 0)
            g.set_mines(int(c) for c in w[1].split(','))
            out.append('field ' + g.line())
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
            ok, opened = solver(g.mines, int(w[1]))
            out.append(f"solve {w[1]} {'cleared' if ok else 'stuck'} {len(opened)}")
    return out


if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        print('\n'.join(run(f.read().splitlines())))
