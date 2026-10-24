"""SDK runner scripts for rocks_app, 3720 (sdk/runner/apprun.c), with what each must end with: the save, the tunes and
the tones, and each tick's ask for the next, from docs/3720.md's rules and the model, not from the C. The breaks and
the lost ships are read from the model's own break_rock and lose_ship as they run.

  appscript.py gentle    seed 5, no save: ENTER 30 ms into a title tick (gentle, the title's own choice); the
                         autopilot for 1,200 steps in ticks of 50 ms, then of 60, 99, 100 and 250 ms; turn, thrust and
                         fire held together on each key of each job (the soft row's too), + and ENTER doing nothing;
                         then hands off until the game ends; ENTER refused 0.2 s and 0.499 s after the end and taken at
                         0.51 s inside a tick; the second game left running
  appscript.py full      seed 5: 6 picks full, 4 gentle, SOFT6 full again; the autopilot for 4,000 steps, past
                         an extra ship, then hands off to the end
  appscript.py refuse    seed 9, a save (bests 4321 and 99, 7 games): keys the title refuses, full picked, a game left
                         to end with nothing pressed, so both bests stay
  appscript.py midsave   seed 9, full: the autopilot until the score passes 500, then the script ends running
  appscript.py nexts NAME, appscript.py loadfile: as SALVO's
Each script ends with '# sound LINE' for each tone and tune, then '# expect save HEX tunes N tones N'.
"""
import sys

import model as M
import pilot

KEY = {'4': 31, 'S1': 0, '6': 33, 'S6': 5, '7': 25, '8': 26, '9': 27, 'S2': 1, 'G': 30, '5': 32, 'X': 34, 'S5': 4,
       'E': 18, 'P': 46, '0': 43}
AGAIN_MS = 500
LOST = '392/120 0/40 330/120 0/40 262/300'
WAVE = '523/100 659/100 784/200'
EXTRA = '784/80 1047/120'
SHIELD = '1568/30 0/30 1568/30'
THUD = {'L': 196, 'M': 262, 'S': 392}


def watch(g):
    """record each break's size and each lost ship as the model makes them"""
    g.ev = {'sizes': set(), 'lost': 0}
    brk, lose = g.break_rock, g.lose_ship

    def break_rock(i):
        g.ev['sizes'].add(g.rocks[i][0])
        return brk(i)

    def lose_ship():
        g.ev['lost'] += 1
        return lose()

    g.break_rock, g.lose_ship = break_rock, lose_ship
    return g


class Game:
    def __init__(self, seed, best=(0, 0), games=0):
        self.seed, self.best, self.games = seed, list(best), games
        self.out, self.nexts, self.sound = [], [], []
        self.clock = self.tunes = self.tones = 0
        self.g = None
        self.full = False
        self.state = 'title'
        self.started = self.end_at = self.tune_until = 0

    def key(self, k, down, at=None):
        at = self.clock if at is None else at
        self.out.append(f'key {KEY[k]} {1 if down else 0} {at}')
        if self.state == 'play':
            if k in M.KEYJOB:
                self.g.key(k, down)
        elif not down:
            pass
        elif self.state == 'title' and k in ('4', 'S1', '6', 'S6'):
            self.full = k in ('6', 'S6')
        elif k == 'E' and (self.state == 'title' or at - self.end_at >= AGAIN_MS):
            self.g = watch(M.Game('f' if self.full else 'g', self.seed, self.games))
            self.games += 1
            self.started = at
            self.tune_until = 0
            self.state = 'play'

    def press(self, k, at=None):
        self.key(k, True, at)
        self.key(k, False, at)

    def tick(self, ms=50, n=1):
        for _ in range(n):
            self.clock += ms
            self.out.append(f'tick {ms}')
            if self.state != 'play':
                self.nexts.append(100)
                continue
            g = self.g
            g.acc += min(min(ms, self.clock - self.started), 100)
            sound, sizes = 0, set()
            while g.acc >= 50 and not g.over:
                g.acc -= 50
                g.ev = {'sizes': set(), 'lost': 0}
                thrusting = g.in_play() and g.job('T')
                wave, ships = g.wave, g.ships
                pilot.step(g)
                sizes |= g.ev['sizes']
                sound |= (1 if g.cool == g.m['cool'] - 1 else 0) | (16 if g.ev['lost'] else 0) \
                    | (32 if g.wave > wave else 0) | (64 if g.ships > ships and not g.ev['lost'] else 0) \
                    | (128 if g.lost == 0 and g.shield == 20 else 0) | (256 if thrusting and g.step_n % 4 == 0 else 0)
                self.best[g.mode == 'f'] = max(self.best[g.mode == 'f'], g.score)
            # docs/3720.md's sound: a lost ship's tune, a wave's, an extra ship's, the shield's end; else a break's
            # thud (the largest), a shot's click, the engine's tick; no beep while a tune plays
            for bit, notes, length in ((16, LOST, 620), (32, WAVE, 400), (64, EXTRA, 200), (128, SHIELD, 90)):
                if sound & bit:
                    self.tune(notes, length)
                    break
            else:
                if self.clock >= self.tune_until:
                    if sizes:
                        self.tone(THUD[next(s for s in 'LMS' if s in sizes)], 30)
                    elif sound & 1:
                        self.tone(1500, 8)
                    elif sound & 256:
                        self.tone(110, 15)
            if g.over:
                self.state = 'over'
                self.end_at = self.clock
                self.nexts.append(100)
            else:
                self.nexts.append(50 - g.acc)

    def tone(self, hz, ms):
        self.tones += 1
        self.sound.append(f'tone {hz} Hz {ms} ms at {self.clock} ms')

    def tune(self, notes, length):
        self.tunes += 1
        self.tune_until = self.clock + length
        self.sound.append(f'tune of {len(notes.split())} notes at {self.clock} ms: {notes}')

    def fly(self, steps, ms=50):
        """the autopilot for about `steps` steps in ticks of ms: its keys change only before a tick"""
        held = set()
        start = self.g.step_n
        while not self.g.over and self.g.step_n - start < steps:
            want = pilot.want(self.g)
            for k in sorted(held - want):
                self.key(k, False)
            for k in sorted(want - held):
                self.key(k, True)
            held = want
            self.tick(ms)
        for k in sorted(held):
            self.key(k, False)

    def park(self):
        """turn the ship to point right and nudge it with short burns until it rests past x 345 px, so its last
        burst is drawn clear of the end's box; a ship lost on the way comes back to the middle and goes again"""
        held = set()
        for _ in range(6000):
            g = self.g
            if g.lost == 0 and g.x >= 345 * M.Q and abs(g.vx) < 2000 and abs(g.vy) < 2000:
                break
            want = set()
            if g.lost == 0 and g.turn == 0 and g.a != 6:
                want.add('6' if (6 - g.a) % 24 <= 12 else '4')
            elif g.lost == 0 and g.a == 6 and g.x < 345 * M.Q and g.vx < 26000:
                want.add('8')
            for k in sorted(held - want):
                self.key(k, False)
            for k in sorted(want - held):
                self.key(k, True)
            held = want
            self.tick(50)
        for k in sorted(held):
            self.key(k, False)

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('7'), 1
        b[2:6] = self.best[0].to_bytes(4, 'little')
        b[6:10] = self.best[1].to_bytes(4, 'little')
        b[10:14] = self.games.to_bytes(4, 'little')
        return b.hex()

    def expect(self):
        return '\n'.join([f'# sound {l}' for l in self.sound]
                         + [f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'])


def gentle():
    g = Game(5)
    g.out.append('# appscript.py gentle: seed 5, no save')
    g.tick(100)
    g.key('E', True, g.clock + 30)
    g.key('E', False, g.clock + 30)
    g.tick(100)
    g.fly(1200)
    for ms in (60, 99, 100, 250):
        g.out.append(f'# ticks of {ms} ms')
        g.fly(150, ms)
    g.out.append("# turn, thrust and fire held together, each key of each job")
    for turn, thrust, fire in (('4', '7', 'G'), ('6', '8', '5'), ('S1', '9', 'X'), ('S6', 'S2', 'S5'),
                               ('S1', '8', 'G'), ('6', 'S2', 'X')):
        for k in (turn, thrust, fire):
            g.key(k, True)
        g.tick(50, 12)
        for k in (turn, thrust, fire):
            g.key(k, False)
        g.tick(50, 4)
    g.out.append('# + and ENTER do nothing in play')
    for k in ('P', 'E'):
        g.key(k, True)
        g.tick(50, 10)
        g.key(k, False)
    g.out.append('# hands off until the game ends')
    while g.state == 'play':
        g.tick(100)
    g.tick(20, 10)
    g.press('E')
    g.tick(20, 14)
    g.press('E', g.end_at + 499)
    g.tick(20)
    g.press('E', g.end_at + 510)
    g.tick(100, 30)
    return g


def full():
    g = Game(5)
    g.out.append('# appscript.py full: seed 5, no save')
    g.tick(100)
    for k in ('6', '4', 'S6'):
        g.press(k)
        g.tick(100)
    g.press('E')
    g.tick(50)
    g.fly(4000)
    assert g.g.next > 5000                  # an extra ship's chime on the way
    g.out.append('# the ship flown to the right edge, clear of the end box, and again after each return, until the')
    g.out.append('# game ends')
    while g.state == 'play':
        g.park()
        while g.state == 'play' and g.g.lost == 0:
            g.tick(50)
        while g.state == 'play' and g.g.lost:
            g.tick(50)
    g.tick(100, 3)
    return g


REFUSE_SAVE = ((4321, 99), 7)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.tick(100)
    for k in ('8', 'P', 'G', '0', '5'):     # the title takes only the mode keys and ENTER
        g.press(k)
    g.press('S6')
    g.tick(100)
    g.press('E')
    while g.state == 'play':
        g.tick(100)
    g.tick(100, 3)
    assert g.best == list(REFUSE_SAVE[0])
    return g


def midsave():
    g = Game(9)
    g.out.append('# appscript.py midsave: seed 9, full; the script ends with the game running')
    g.tick(100)
    g.press('6')
    g.press('E')
    while g.g.score < 500:
        g.fly(20)
    assert g.state == 'play' and g.best[1] == g.g.score
    return g


SCRIPTS = {'gentle': gentle, 'full': full, 'refuse': refuse, 'midsave': midsave}


def main():
    if sys.argv[1] == 'loadfile':
        print(Game(9, *REFUSE_SAVE).save())
        return
    if sys.argv[1] == 'nexts':
        print('\n'.join(str(n) for n in SCRIPTS[sys.argv[2]]().nexts))
        return
    g = SCRIPTS[sys.argv[1]]()
    print('\n'.join(g.out))
    print(g.expect())


if __name__ == '__main__':
    main()
