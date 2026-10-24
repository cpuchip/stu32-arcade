"""SDK runner scripts for catch_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, and each tick's ask for the next, from docs/catch.md's rules and the model, not from the C.

  appscript.py gentle    seed 5, no save: ENTER 30 ms into a title tick (gentle, the title's own choice); the
                         autopilot for 1,200 steps in ticks of 50 ms, then of 60, 99, 100 and 250 ms; each move key,
                         both at once, a tap against a held key, + and ENTER moving nothing; the walls' bumps; the
                         script ends with the game running (gentle never ends)
  appscript.py full      seed 5: 6 picks full, 4 picks gentle, SOFT6 full again; the autopilot to the end; ENTER
                         refused 0.2 s and 0.499 s after the end and taken at 0.51 s inside a tick; a second full game
                         left to end
  appscript.py refuse    seed 9, a save (bests 4321 and 99, 7 games): keys the title refuses, full picked, a game
                         left to end with nothing pressed, so both bests stay
  appscript.py midsave   seed 9, full: the autopilot until 12 catches, then the script ends with the game running (the
                         app saves on leaving, so the best must already be the score); catch_check.sh runs it with no
                         save and with damaged ones, which must all be refused and give the same save
  appscript.py nexts NAME  what each tick of script NAME must ask for, one line a tick
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with its sound, '# sound LINE' as the runner's -v prints each tone and tune, then
'# expect save HEX tunes N tones N'.
"""
import sys

import model as M
import pilot

KEY = {'4': 31, 'S1': 0, '6': 33, 'S6': 5, 'E': 18, 'P': 46, '8': 26, 'G': 30, '0': 43}
AGAIN_MS = 500
END = '392/120 0/40 330/120 0/40 262/300'
LOST = '330/100 262/160'
TEN = '523/100 659/100 784/200'


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
            if k in M.ALL_KEYS:
                (self.g.down if down else self.g.up)(k)
        elif down and self.state == 'title' and k in ('4', 'S1', '6', 'S6'):
            self.full = k in ('6', 'S6')
        elif down and k == 'E' and (self.state == 'title' or at - self.end_at >= AGAIN_MS):
            self.g = M.Catch('f' if self.full else 'g', self.seed, self.games)
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
            g.acc += min(min(ms, self.clock - self.started), M.TICK_CAP)
            sound = 0
            while g.acc >= M.STEP_MS and not g.over:
                g.acc -= M.STEP_MS
                n0, score, lives, rng = len(g.stars), g.score, g.lives, g.x
                g.step()
                spawned = 1 if g.x != rng else 0
                catches = g.score - score
                misses = n0 - catches - (len(g.stars) - spawned) if not g.over else 1
                sound |= (1 if catches else 0) | (2 if misses else 0) | (4 if g.lives < lives else 0) \
                    | (8 if g.score // 10 > score // 10 else 0) | (16 if g.bump else 0) | (32 if g.over else 0)
                full = g.mode == 'f'
                self.best[full] = max(self.best[full], g.score)
            # docs/catch.md's sound: the end's tune, a lost life's, the ten's; else a catch's chime, a miss's note,
            # the wall's bump; no beep while a tune plays
            if sound & 32:
                self.tune(END, 620)
            elif sound & 4:
                self.tune(LOST, 260)
            elif sound & 8:
                self.tune(TEN, 400)
            elif self.clock >= self.tune_until:
                if sound & 1:
                    self.tone(1047, 30)
                elif sound & 2:
                    self.tone(196, 30)
                elif sound & 16:
                    self.tone(100, 10)
            if g.over:
                self.state = 'over'
                self.end_at = self.clock
                self.nexts.append(100)
            else:
                self.nexts.append(M.STEP_MS - g.acc)

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
            m = pilot.choose(self.g)
            want = {'4'} if m < 0 else {'6'} if m > 0 else set()
            for k in sorted(held - want):
                self.key(k, False)
            for k in sorted(want - held):
                self.key(k, True)
            held = want
            self.tick(ms)
        for k in sorted(held):
            self.key(k, False)

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('K'), 1
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
    g.tick(100)                             # it counts 70 ms of the game
    g.fly(1200)
    for ms in (60, 99, 100, 250):
        g.out.append(f'# ticks of {ms} ms')
        g.fly(150, ms)
    g.out.append('# each move key to its wall, both at once, a tap against a held key')
    for k in ('S1', 'S6', '4', '6'):
        g.key(k, True)
        g.tick(50, 70)
        g.key(k, False)
    g.key('4', True)
    g.key('S6', True)
    g.tick(50, 10)
    g.key('S6', False)
    g.tick(50, 5)
    g.press('6')
    g.tick(50, 3)
    g.key('4', False)
    g.out.append('# +, ENTER, 8 and GOLD move nothing')
    for k in ('P', 'E', '8', 'G'):
        g.key(k, True)
        g.tick(50, 10)
        g.key(k, False)
    g.tick(50, 40)
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
    g.fly(100000)
    g.tick(20, 10)                          # 0.2 s after the end: ENTER refused
    g.press('E')
    g.tick(20, 14)
    g.press('E', g.end_at + 499)            # 0.499 s: refused
    g.tick(20)
    g.press('E', g.end_at + 510)            # 0.51 s, 10 ms into a tick: taken, and its first tick counts 10 ms
    while g.state == 'play':
        g.tick(100)
    g.tick(100, 3)
    return g


REFUSE_SAVE = ((4321, 99), 7)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.tick(100)
    for k in ('8', 'P', 'G', '0'):          # the title takes only the mode keys and ENTER
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
    while g.g.score < 12:
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
