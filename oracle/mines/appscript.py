"""SDK runner scripts for mines_app (sdk/runner/apprun.c), with what each must end with: the save and the sound, from
docs/mines.md's rules (the cursor and its repeat, the settings, the deal across ticks, the clock, the records), not
from the C.

  appscript.py play      seed 5, no save (so MEDIUM, no guess, sound on): ENTER partway through a title tick; the
                         complete solver's game played by moving the cursor key by key (a held key's repeat on the
                         way), a win and the first best; ENTER refused 0.2 s into the end and taken at exactly 0.5 s;
                         a second game with a wrong flag, lost on a mine
  appscript.py refuse    seed 9, a save whose MEDIUM no-guess best is exactly the first win's time, with records on
                         two other boards that must come back unchanged: keys the title refuses, a win that ties the
                         best (no new best), and a slower one (the best stays)
  appscript.py cursor    seed 3: HARD chosen on the settings page; the cursor held into each edge of 30 x 16, where it
                         stops; a second key pressed while one is held takes over, and the first one's up does
                         nothing; then left alive
  appscript.py settings  seed 4: every row of the settings page, its ends and the board's wrap, ENTER back; a classic
                         game (the first attempt dealt) lost with the sound off; 0 at the end refused before 0.5 s and
                         taken after, the sound back on
  appscript.py hard      seed 2441: HARD, whose first open at the centre takes 213 attempts, 4 a tick; keys pressed
                         during the deal do nothing; the clock starts on the dealing tick; lost on a mine
  appscript.py quiet     seed 6: EASY with the sound off, the complete solver's game won with no tone or tune
  appscript.py instant   seed 4976: EASY, whose first open at the centre clears the field on the dealing tick, a
                         win in 0 ms kept as a best of 1 ms
  appscript.py badload   seed 9: a first open, then left alive; oracle/mines_check.sh runs it with damaged saves
  appscript.py loadfile  the refuse script's save, as hex
Every key comes 7 ms after the last tick, so the frames drawn on keys (hashed with the runner's -keyframes) are not
the tick's. Each script ends with its sound, '# sound LINE' as the runner's -v prints each tone and tune, then
'# expect save HEX tunes N tones N' (comments, so the runner skips them).
"""
import sys

import model as M
import proof as P

KEY = {'8': 26, '2': 38, '4': 31, '6': 33, '5': 32, '0': 43, 'ENTER': 18, '1': 37}
REPEAT_FIRST, REPEAT_NEXT, AGAIN_MS, DEAL_STEP = 300, 80, 500, 4
LATE = 7   # keys come this long after the last tick, as on a calculator, so the frames drawn on keys differ
FALL = '392/120 0/40 330/120 0/40 262/300'
RISE = '523/100 659/100 784/100 1047/300'
BOARDS = ('easy', 'medium', 'hard')


class App:
    def __init__(self, seed, save=None):
        self.seed = seed << 32
        self.board, self.classic, self.quiet = 1, False, False   # MEDIUM, no guess, sound on
        self.rec = [[0, 0, 0] for _ in range(6)]                  # best, wins, games
        if save is not None:
            self.board, self.classic, self.quiet, self.rec = save
            self.rec = [list(r) for r in self.rec]
        self.out, self.clock = [], 0
        self.tunes = self.tones = 0
        self.sound = []
        self.g = None
        self.state = 'title'
        self.row = 0
        self.held = None

    def r(self):
        return self.rec[self.board * 2 + self.classic]

    def tick(self, ms=20, n=1):
        for _ in range(n):
            self.clock += ms
            self.out.append(f'tick {ms}')
            if self.state == 'dealing':
                if self.g.deal_step(DEAL_STEP):
                    self.t0 = self.clock
                    self.state = 'play'
                    f = self.dealing_at
                    self.g.open(f)
                    self.opened(0, self.clock)
            elif self.state == 'play' and self.held:
                while self.clock >= self.repeat_at:
                    self.move(self.held)
                    self.repeat_at += REPEAT_NEXT

    def move(self, k):
        dx = {'4': -1, '6': 1}.get(k, 0)
        dy = {'8': -1, '2': 1}.get(k, 0)
        if 0 <= self.cx + dx < self.g.b.W:
            self.cx += dx
        if 0 <= self.cy + dy < self.g.b.H:
            self.cy += dy

    def tone(self):
        if not self.quiet:
            self.tones += 1
            self.sound.append(f'tone 440 Hz 10 ms at {self.clock} ms')

    def tune(self, notes):
        if not self.quiet:
            self.tunes += 1
            self.sound.append(f'tune of {len(notes.split())} notes at {self.clock} ms: {notes}')

    def down(self, k, at=None):
        at = self.clock + LATE if at is None else at
        self.out.append(f'key {KEY[k]} 1 {at}')
        if self.state == 'play':
            if k in '8246':                   # a second key takes over from a held one
                self.move(k)
                self.held, self.repeat_at = k, at + REPEAT_FIRST
            elif k in ('5', 'ENTER'):
                self.open_here(at)
            elif k == '0':
                self.g.flag(self.cy * self.g.b.W + self.cx)
        elif self.state == 'settings':
            if k == '8':
                self.row = max(0, self.row - 1)
            elif k == '2':
                self.row = min(2, self.row + 1)
            elif k in '465' and self.row == 0:
                self.board = (max(0, self.board - 1) if k == '4' else min(2, self.board + 1) if k == '6'
                              else (self.board + 1) % 3)
            elif k in '465' and self.row == 1:
                self.classic = not self.classic
            elif k in '465':
                self.quiet = not self.quiet
            elif k == 'ENTER':
                self.state = 'title'
        elif self.state in ('title', 'over'):
            if self.state == 'over' and at - self.t_end < AGAIN_MS:
                return
            if k == 'ENTER':
                game = sum(r[2] for r in self.rec)
                self.g = M.Game(self.seed, game, BOARDS[self.board], self.classic)
                self.r()[2] += 1
                self.cx, self.cy, self.held = self.g.b.W // 2, self.g.b.H // 2, None
                self.new_best = False
                self.state = 'play'
            elif k == '0':
                self.state, self.row = 'settings', 0

    def up(self, k, at=None):
        at = self.clock + LATE if at is None else at
        self.out.append(f'key {KEY[k]} 0 {at}')
        if k == self.held:
            self.held = None

    def press(self, k, at=None):
        self.down(k, at)
        self.up(k, at)

    def open_here(self, at):
        cell = self.cy * self.g.b.W + self.cx
        if self.g.mines is None:
            if self.g.deal_start(cell):
                self.state, self.held, self.dealing_at = 'dealing', None, cell
            return
        opened = len(self.g.opened)
        self.g.open(cell)
        self.opened(opened, at)

    def opened(self, before, at):
        if self.g.state == 'play':
            if len(self.g.opened) != before:
                self.tone()
            return
        self.t_end, self.state, self.held = at, 'over', None
        self.tune(FALL if self.g.state == 'lost' else RISE)
        if self.g.state == 'won':
            r = self.r()
            r[1] += 1
            t = max(1, at - self.t0)
            if r[0] == 0 or t < r[0]:
                r[0] = t

    def go(self, cell):
        """the cursor to cell, a key at a time, a tick after each"""
        x, y = cell % self.g.b.W, cell // self.g.b.W
        while (self.cx, self.cy) != (x, y):
            k = '6' if self.cx < x else '4' if self.cx > x else '2' if self.cy < y else '8'
            self.press(k)
            self.tick()

    def act(self, k, cell):
        self.go(cell)
        self.press(k)
        self.tick()

    def first_open(self, cell, during=()):
        """the first open at cell, then ticks until the deal is done, pressing the keys in during on the way"""
        self.act('5', cell)
        for k in during:
            self.press(k)
            self.tick()
        while self.state == 'dealing':
            self.tick()

    def solve_and_play(self, first, again=False, pause=0):
        """the complete solver's game: flags on its mines, opens on its safe cells (again: 5 once more on the first
        cell, an open that opens nothing and so makes no tick; pause: ticks after the first open)"""
        self.first_open(first)
        if again:
            self.press('5')
            self.tick()
        self.tick(20, pause)
        known = set()
        while self.g.state == 'play':
            safe, mine = P.deduce(self.g.b, self.g.opened, known, self.g.num)
            for c in sorted(mine - known):
                self.act('0', c)
            known |= mine
            new = [c for c in sorted(safe) if c not in self.g.opened]
            assert new, 'a dealt board needed a guess'
            for c in new:
                if c not in self.g.opened and self.g.state == 'play':
                    self.act('5', c)

    def settings_to(self, board=None, classic=None, quiet=None):
        """from the title: the settings page, rows set by their keys, ENTER back"""
        self.press('0')
        self.tick(100)
        if board is not None:
            while self.board != board:
                self.press('6' if board > self.board else '4')
                self.tick(100)
        for row, want, have in ((1, classic, 'classic'), (2, quiet, 'quiet')):
            if want is not None and getattr(self, have) != want:
                while self.row < row:
                    self.press('2')
                    self.tick(100)
                self.press('5')
                self.tick(100)
        self.press('ENTER')
        self.tick(100)

    def save(self):
        b = bytearray(80)
        b[0], b[1] = ord('M'), 2
        b[2] = self.board | (4 if self.classic else 0) | (8 if self.quiet else 0)
        for i, r in enumerate(self.rec):
            for j, v in enumerate(r):
                b[3 + 12 * i + 4 * j:3 + 12 * i + 4 * j + 4] = v.to_bytes(4, 'little')
        return b.hex()

    def expect(self):
        return '\n'.join([f'# sound {l}' for l in self.sound]
                         + [f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'])


def first_cell(b, seed):
    return P.first_cell(b, seed)


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
    a.solve_and_play(first_cell(a.g.b, 5), again=True)
    a.out.append(f'# game 1: {a.g.state}, {a.t_end - a.t0} ms')
    a.tick(20, 10)                            # 0.2 s into the end: ENTER refused
    a.press('ENTER')
    a.tick(20, 14)
    assert a.clock + LATE - a.t_end == AGAIN_MS
    a.press('ENTER')                          # exactly 0.5 s: taken
    a.tick()
    a.first_open(0)                           # deals with (0, 0) safe
    wrong = min(c for c in range(a.g.b.CELLS) if c not in a.g.mines and c not in a.g.opened)
    a.out.append(f'# a wrong flag: {wrong}')
    a.act('0', wrong)
    mine = min(a.g.mines)
    a.out.append(f'# a mine opened: {mine}')
    a.act('5', mine)
    a.out.append(f'# game 2: {a.g.state}')
    a.tick(20, 30)
    return a


REFUSE_OTHERS = {0 * 2 + 1: [7000, 1, 3], 2 * 2 + 0: [0, 0, 4]}   # EASY classic, HARD no guess


def refuse_save(best):
    rec = [[0, 0, 0] for _ in range(6)]
    for i, r in REFUSE_OTHERS.items():
        rec[i] = list(r)
    rec[1 * 2 + 0] = [best, 2, 5]
    return (1, False, False, rec)


def refuse(best=None):
    """best None: a dry run with no best, to learn the first win's time"""
    a = App(9, refuse_save(best or 0))
    a.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    a.tick(100)
    for k in ('5', '8', '4'):                 # the title takes only ENTER and 0
        a.press(k)
    a.tick(100)
    a.press('ENTER')
    a.tick()
    a.solve_and_play(first_cell(a.g.b, 9))
    t1 = a.t_end - a.t0
    if best is None:
        return t1
    a.out.append(f'# game 1: {a.g.state}, {t1} ms against the best {best}: a tie, no new best')
    a.tick(20, 30)
    a.press('ENTER')
    a.tick()
    a.solve_and_play(first_cell(a.g.b, 9 + 1000), pause=1000)  # 20 s still after the first open: slower
    a.out.append(f'# game 2: {a.g.state}, {a.t_end - a.t0} ms: slower, the best stays')
    a.tick(20, 30)
    return a


def cursor():
    a = App(3)
    a.out.append('# appscript.py cursor: seed 3, HARD, the cursor held into each edge')
    a.tick(100)
    a.settings_to(board=2)
    a.press('ENTER')
    a.tick()
    for k, n in (('8', 60), ('4', 160), ('2', 120), ('6', 250)):
        a.down(k)
        a.tick(20, n)
        a.up(k)
        a.tick()
        a.out.append(f'# after {k}: the cursor at {a.cx},{a.cy}')
    a.out.append('# 4 held, then 8 pressed while it is: 8 takes over; 4 let go first, which does nothing')
    a.down('4')
    a.tick(20, 20)
    a.down('8')
    a.tick(20, 5)
    a.up('4')
    a.tick(20, 30)
    a.up('8')
    a.tick()
    a.out.append(f'# after both: the cursor at {a.cx},{a.cy}')
    a.first_open(a.cy * a.g.b.W + a.cx)
    a.tick(20, 5)
    return a


def settings():
    a = App(4)
    a.out.append('# appscript.py settings: seed 4, every row of the settings page')
    a.tick(100)
    a.press('0')
    a.tick(100)
    # BOARD: 8 at the top stays; 4 to EASY and again (stays); 6 up to HARD and again (stays); 5 wraps to EASY, then MEDIUM
    for k in ('8', '4', '4', '6', '6', '6', '5', '5'):
        a.press(k)
        a.tick(100)
        a.out.append(f'# {k}: row {a.row}, board {BOARDS[a.board]}')
    # CLASSIC RULES: 5 on, 4 off, 6 on; SOUND EFFECTS: 2 at the bottom stays, 5 off
    for k in ('2', '5', '4', '6', '2', '2', '5'):
        a.press(k)
        a.tick(100)
        a.out.append(f'# {k}: row {a.row}, classic {a.classic}, quiet {a.quiet}')
    a.press('ENTER')
    a.tick(100)
    a.press('ENTER')
    a.tick()
    a.first_open(a.cy * a.g.b.W + a.cx)
    assert a.g.attempts == 1
    a.out.append(f'# a classic deal: {a.g.attempts} attempt')
    mine = min(a.g.mines)
    a.act('5', mine)
    a.out.append(f'# lost on {mine}, with no tune: the sound is off')
    a.tick(20, 10)
    a.press('0')                              # 0.2 s in: refused
    a.tick(20, 20)
    a.press('0')                              # after 0.5 s: the settings
    a.tick(100)
    for k in ('2', '2', '5'):                 # the sound back on
        a.press(k)
        a.tick(100)
    a.press('ENTER')
    a.tick(100)
    return a


def hard():
    a = App(2441)
    a.out.append('# appscript.py hard: seed 2441, HARD; the first open at the centre deals 213 attempts, 4 a tick')
    a.tick(100)
    a.settings_to(board=2)
    a.press('ENTER')
    a.tick()
    a.act('5', a.cy * a.g.b.W + a.cx)
    assert a.state == 'dealing'
    for k in ('6', '0', '5', 'ENTER', '8'):   # each does nothing while the board is dealt
        a.press(k)
        a.tick()
    while a.state == 'dealing':
        a.tick()
    assert a.g.attempts == 213, a.g.attempts
    a.out.append(f'# dealt in {a.g.attempts} attempts; the clock started at {a.t0} ms')
    a.tick(20, 60)                            # the clock shows 1 s
    mine = min(a.g.mines, key=lambda m: abs(m % 30 - a.cx) + abs(m // 30 - a.cy))
    a.act('5', mine)
    a.out.append(f'# lost on {mine} after {a.t_end - a.t0} ms')
    a.tick(20, 30)
    return a


def quiet():
    a = App(6)
    a.out.append('# appscript.py quiet: seed 6, EASY with the sound off')
    a.tick(100)
    a.settings_to(board=0, quiet=True)
    a.press('ENTER')
    a.tick()
    a.solve_and_play(first_cell(a.g.b, 6))
    assert a.g.state == 'won'
    a.out.append(f'# won in {a.t_end - a.t0} ms, with no tone or tune')
    a.tick(20, 30)
    return a


def instant():
    a = App(4976)
    a.out.append('# appscript.py instant: seed 4976, EASY; the first open clears the field')
    a.tick(100)
    a.settings_to(board=0)
    a.press('ENTER')
    a.tick()
    a.first_open(a.cy * a.g.b.W + a.cx)
    assert a.g.state == 'won' and a.t_end == a.t0
    a.out.append(f'# won on the dealing tick, {a.t_end - a.t0} ms: the best kept as {a.r()[0]} ms')
    a.tick(20, 30)
    return a


def badload():
    a = App(9)
    a.out.append('# appscript.py badload: seed 9; one open, then left alive')
    a.tick(100)
    a.press('ENTER')
    a.tick()
    a.first_open(first_cell(a.g.b, 9))
    a.tick(20, 5)
    return a


def main():
    which = sys.argv[1]
    if which == 'loadfile':
        a = App(0, refuse_save(refuse()))
        print(a.save())
        return 0
    a = {'play': play, 'refuse': lambda: refuse(refuse()), 'cursor': cursor, 'settings': settings, 'hard': hard,
         'quiet': quiet, 'instant': instant, 'badload': badload}[which]()
    print('\n'.join(a.out))
    print(a.expect())
    return 0


if __name__ == '__main__':
    sys.exit(main())
