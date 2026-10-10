"""SDK runner scripts for mines_app (sdk/runner/apprun.c), with what each must end with: the save and the sound, from
docs/mines.md's rules (the cursor and its repeat, the clock, the best time, the counts), not from the C.

  appscript.py play      seed 5, no save: ENTER partway through a title tick; the complete solver's game played by
                         moving the cursor key by key (a held key's repeat on the way), a win and the first best;
                         ENTER refused before 0.5 s, taken after; a second game with a wrong flag, lost on a mine
  appscript.py refuse    seed 9, a save (best 1,000 ms, 2 wins, 5 games): keys the title refuses, a win slower than
                         the best, which stays
  appscript.py cursor    seed 3: the cursor held into each edge, where it stops, then left alive
  appscript.py badload   seed 9: a first open, then left alive; oracle/mines_check.sh runs it with four damaged saves
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with its sound, '# sound LINE' as the runner's -v prints each tone and tune, then
'# expect save HEX tunes N tones N' (comments, so the runner skips them).
"""
import sys

import model as M
import proof as P

KEY = {'8': 26, '2': 38, '4': 31, '6': 33, '5': 32, '0': 43, 'ENTER': 18, '1': 37}
REPEAT_FIRST, REPEAT_NEXT, AGAIN_MS = 300, 80, 500
FALL = '392/120 0/40 330/120 0/40 262/300'
RISE = '523/100 659/100 784/100 1047/300'


class App:
    def __init__(self, seed, best=0, wins=0, games=0):
        self.seed = seed << 32
        self.best, self.wins, self.games = best, wins, games
        self.out, self.clock = [], 0
        self.tunes = self.tones = 0
        self.sound = []
        self.g = None
        self.state = 'title'

    def tick(self, ms=20, n=1):
        for _ in range(n):
            self.clock += ms
            self.out.append(f'tick {ms}')
            if self.state == 'play' and self.held:
                while self.clock >= self.repeat_at:
                    self.move(self.held)
                    self.repeat_at += REPEAT_NEXT

    def move(self, k):
        dx = {'4': -1, '6': 1}.get(k, 0)
        dy = {'8': -1, '2': 1}.get(k, 0)
        if 0 <= self.cx + dx < M.W:
            self.cx += dx
        if 0 <= self.cy + dy < M.H:
            self.cy += dy

    def down(self, k, at=None):
        at = self.clock if at is None else at
        self.out.append(f'key {KEY[k]} 1 {at}')
        if self.state == 'play':
            if k in '8246':
                self.move(k)
                self.held, self.repeat_at = k, at + REPEAT_FIRST
            elif k in ('5', 'ENTER'):
                self.open_here(at)
            elif k == '0':
                self.g.flag(self.cy * M.W + self.cx)
        elif k == 'ENTER' and (self.state == 'title' or at - self.t_end >= AGAIN_MS):
            self.g = M.Game(self.seed, self.games)
            self.games += 1
            self.cx, self.cy, self.held = 12, 6, None
            self.state = 'play'

    def up(self, k, at=None):
        at = self.clock if at is None else at
        self.out.append(f'key {KEY[k]} 0 {at}')
        if self.state == 'play' and k == self.held:
            self.held = None

    def press(self, k, at=None):
        self.down(k, at)
        self.up(k, at)

    def open_here(self, at):
        dealt = self.g.mines is not None
        opened = len(self.g.opened)
        self.g.open(self.cy * M.W + self.cx)
        if not dealt and self.g.mines is not None:
            self.t0 = at
        if self.g.state == 'play':
            if len(self.g.opened) != opened:
                self.tones += 1
                self.sound.append(f'tone 440 Hz 10 ms at {self.clock} ms')
            return
        self.t_end, self.state, self.held = at, 'over', None
        self.tunes += 1
        notes = FALL if self.g.state == 'lost' else RISE
        self.sound.append(f'tune of {len(notes.split())} notes at {self.clock} ms: {notes}')
        if self.g.state == 'won':
            self.wins += 1
            t = at - self.t0
            if self.best == 0 or t < self.best:
                self.best = t

    def go(self, cell):
        """the cursor to cell, a key at a time, a tick after each"""
        x, y = cell % M.W, cell // M.W
        while (self.cx, self.cy) != (x, y):
            k = '6' if self.cx < x else '4' if self.cx > x else '2' if self.cy < y else '8'
            self.press(k)
            self.tick()

    def act(self, k, cell):
        self.go(cell)
        self.press(k)
        self.tick()

    def solve_and_play(self, first, again=False):
        """the complete solver's game: flags on its mines, opens on its safe cells (again: 5 once more on the first
        cell, an open that opens nothing and so makes no tick)"""
        self.act('5', first)
        if again:
            self.press('5')
            self.tick()
        known = set()
        while self.g.state == 'play':
            safe, mine = P.deduce(self.g.opened, known, self.g.num)
            for c in sorted(mine - known):
                self.act('0', c)
            known |= mine
            new = [c for c in sorted(safe) if c not in self.g.opened]
            assert new, 'a dealt board needed a guess'
            for c in new:
                if c not in self.g.opened and self.g.state == 'play':
                    self.act('5', c)

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('M'), 1
        for at, v in ((2, self.best), (6, self.wins), (10, self.games)):
            b[at:at + 4] = v.to_bytes(4, 'little')
        return b.hex()

    def expect(self):
        return '\n'.join([f'# sound {l}' for l in self.sound]
                         + [f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'])


def play():
    a = App(5)
    a.out.append('# appscript.py play: seed 5, no save')
    a.tick(100)                              # the title asks for 100 ms ticks
    a.press('ENTER', at=a.clock + 30)         # partway through the next one
    a.tick(100)
    a.out.append('# a held 6: one move at once, then one each 80 ms after 300 ms')
    a.down('6')
    a.tick(20, 30)
    a.up('6')
    a.tick()
    a.solve_and_play(P.first_cell(5), again=True)
    a.out.append(f'# game 1: {a.g.state}, {a.t_end - a.t0} ms')
    a.tick(20, 10)                            # 0.2 s into the end: ENTER refused
    a.press('ENTER')
    a.tick(20, 20)
    a.press('ENTER')                          # 0.6 s: taken
    a.tick()
    a.act('5', 0)                             # deals with (0, 0) safe
    wrong = min(c for c in range(M.CELLS) if c not in a.g.mines and c not in a.g.opened)
    a.out.append(f'# a wrong flag: {wrong}')
    a.act('0', wrong)
    mine = min(a.g.mines)
    a.out.append(f'# a mine opened: {mine}')
    a.act('5', mine)
    a.out.append(f'# game 2: {a.g.state}')
    a.tick(20, 30)
    return a


REFUSE_SAVE = (1000, 2, 5)


def refuse():
    a = App(9, *REFUSE_SAVE)
    a.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    a.tick(100)
    for k in ('5', '0', '8'):                 # the title takes only ENTER
        a.press(k)
    a.tick(100)
    a.press('ENTER')
    a.tick()
    a.solve_and_play(P.first_cell(9))
    a.out.append(f'# game: {a.g.state}, {a.t_end - a.t0} ms against the best 1000')
    a.tick(20, 30)
    return a


def cursor():
    a = App(3)
    a.out.append('# appscript.py cursor: seed 3, the cursor held into each edge')
    a.tick(100)
    a.press('ENTER')
    a.tick()
    for k, n in (('8', 60), ('4', 160), ('2', 120), ('6', 250)):
        a.down(k)
        a.tick(20, n)
        a.up(k)
        a.tick()
        a.out.append(f'# after {k}: the cursor at {a.cx},{a.cy}')
    a.act('5', a.cy * M.W + a.cx)
    a.tick(20, 5)
    return a


def badload():
    a = App(9)
    a.out.append('# appscript.py badload: seed 9; one open, then left alive')
    a.tick(100)
    a.press('ENTER')
    a.tick()
    a.act('5', P.first_cell(9))
    a.tick(20, 5)
    return a


def main():
    which = sys.argv[1]
    if which == 'loadfile':
        print(App(0, *REFUSE_SAVE).save())
        return 0
    a = {'play': play, 'refuse': refuse, 'cursor': cursor, 'badload': badload}[which]()
    print('\n'.join(a.out))
    print(a.expect())
    return 0


if __name__ == '__main__':
    sys.exit(main())
