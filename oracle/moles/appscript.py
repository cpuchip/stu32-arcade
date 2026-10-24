"""SDK runner scripts for moles_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, and each tick's ask for the next, from docs/moles.md's rules and the model, not from the C.

  appscript.py gentle    seed 5, no save: 0 turns the digits on, ENTER 30 ms into a title tick; the autopilot for
                         1,200 steps in ticks of 50 ms, then of 60, 99, 100 and 250 ms; wrong keys, a key held and
                         pressed again, 0, ENTER and + bonking nothing; the script ends with the game running
  appscript.py full      seed 5: 6 picks full, 4 gentle, SOFT6 full again, 0 twice; the autopilot through the stay's
                         floor and two moles at once; the stun (a wrong key, then the right one inside and after it);
                         then hands off to the end; ENTER refused 0.2 s and 0.499 s after it, taken at 0.51 s inside a
                         tick; a second full game left to end
  appscript.py refuse    seed 9, a save (bests 4321 and 99, 7 games, the digits on): keys the title refuses, full
                         picked, a game left to end with nothing pressed, so the bests and the digits stay
  appscript.py midsave   seed 9, full: the autopilot until 12 bonks, then the script ends with the game running
  appscript.py nexts NAME, appscript.py loadfile: as SALVO's
Each script ends with '# sound LINE' for each tone and tune, then '# expect save HEX tunes N tones N'.
"""
import sys

import model as M
import pilot

KEY = {'1': 37, '2': 38, '3': 39, '4': 31, '5': 32, '6': 33, '7': 25, '8': 26, '9': 27, '0': 43, 'E': 18, 'P': 46,
       'S1': 0, 'S6': 5}
AGAIN_MS = 500
END = '392/120 0/40 330/120 0/40 262/300'
GONE = '330/100 262/160'
TEN = '523/100 659/100 784/200'
BONK = '220/30 1320/15'
POP = '660/30 880/30'


class Game:
    def __init__(self, seed, best=(0, 0), games=0, digits=0):
        self.seed, self.best, self.games, self.digits = seed, list(best), games, digits
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
            (self.g.down if down else self.g.up)(k)
        elif not down:
            pass
        elif self.state == 'title' and k in ('4', 'S1', '6', 'S6'):
            self.full = k in ('6', 'S6')
        elif self.state == 'title' and k == '0':
            self.digits ^= 1
        elif k == 'E' and (self.state == 'title' or at - self.end_at >= AGAIN_MS):
            self.g = M.Game('f' if self.full else 'g', self.seed, self.games)
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
            sound = steps = 0
            while g.acc >= 50 and steps < 2 and not g.over:
                g.acc -= 50
                steps += 1
                queued, bonks, score, lives, stun = len(g.queue), g.bonks, g.score, g.lives, g.stun
                g.run_step()
                wrong = (stun == 0 and g.stun == 6) if g.full else queued > g.bonks - bonks
                sound |= (1 if ('R', 0) in g.holes else 0) | (2 if g.bonks > bonks else 0) | (4 if wrong else 0) \
                    | (8 if g.score // 10 > score // 10 else 0) | (16 if g.lives < lives else 0) \
                    | (32 if g.over else 0)
                self.best[g.full] = max(self.best[g.full], g.score)
            # docs/moles.md's sound: the end's tune, a getaway's, the ten's, the bonk's, the pop's; a wrong key's
            # thunk only while no tune plays
            for bit, notes, length in ((32, END, 620), (16, GONE, 260), (8, TEN, 400), (2, BONK, 45), (1, POP, 60)):
                if sound & bit:
                    self.tune(notes, length)
                    break
            else:
                if sound & 4 and self.clock >= self.tune_until:
                    self.tone(150, 20)
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

    def fly(self, steps, delay=3, ms=50):
        """the autopilot for about `steps` steps in ticks of ms, its taps between ticks"""
        start = self.g.step
        while not self.g.over and self.g.step - start < steps:
            for k in pilot.presses(self.g, delay):
                self.press(k)
            self.tick(ms)

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('M'), 1
        b[2:6] = self.best[0].to_bytes(4, 'little')
        b[6:10] = self.best[1].to_bytes(4, 'little')
        b[10:14] = self.games.to_bytes(4, 'little')
        b[14] = self.digits
        return b.hex()

    def expect(self):
        return '\n'.join([f'# sound {l}' for l in self.sound]
                         + [f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'])


def gentle():
    g = Game(5)
    g.out.append('# appscript.py gentle: seed 5, no save')
    g.tick(100)
    g.press('0')
    g.key('E', True, g.clock + 30)
    g.key('E', False, g.clock + 30)
    g.tick(100)
    g.fly(1200)
    for ms in (60, 99, 100, 250):
        g.out.append(f'# ticks of {ms} ms')
        g.fly(150, 3, ms)
    g.out.append('# wrong keys, a held key pressed again, 0, ENTER and + bonking nothing')
    for k in '123456789':
        g.press(k)
        g.tick(50, 3)
    g.key('5', True)
    g.key('5', True)
    g.tick(50, 2)
    g.key('5', False)
    for k in ('0', 'E', 'P', 'S1', 'S6'):
        g.press(k)
        g.tick(50, 4)
    g.fly(300)
    return g


def full():
    g = Game(5)
    g.out.append('# appscript.py full: seed 5, no save')
    g.tick(100)
    for k in ('6', '4', 'S6', '0', '0'):
        g.press(k)
        g.tick(100)
    g.press('E')
    g.tick(50)
    g.fly(1500, 2)
    g.out.append('# the stun: a wrong key, the right one inside it and after it')
    for _ in range(40):
        up = [i for i, (s, _) in enumerate(g.g.holes) if s in 'RU']
        if up:
            break
        g.tick(50)
    if up:
        right = pilot.DIGIT[up[0]]
        wrong = next(k for k in '123456789' if k != right and g.g.holes[M.KEY_HOLE[k]][0] not in 'RU')
        g.press(wrong)
        g.tick(50)
        g.press(right)
        g.tick(50, 6)
        g.press(right)
        g.tick(50)
    g.out.append('# hands off to the end')
    while g.state == 'play':
        g.tick(100)
    g.tick(20, 10)
    g.press('E')
    g.tick(20, 14)
    g.press('E', g.end_at + 499)
    g.tick(20)
    g.press('E', g.end_at + 510)
    while g.state == 'play':
        g.tick(100)
    g.tick(100, 3)
    return g


REFUSE_SAVE = ((4321, 99), 7, 1)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.tick(100)
    for k in ('8', 'P', '1', '9'):          # the title takes only the mode keys, 0 and ENTER
        g.press(k)
    g.press('S6')
    g.tick(100)
    g.press('E')
    while g.state == 'play':
        g.tick(100)
    g.tick(100, 3)
    assert g.best == list(REFUSE_SAVE[0]) and g.digits == 1
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
