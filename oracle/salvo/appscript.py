"""SDK runner scripts for salvo_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, and each tick's ask for the next, from docs/salvo.md's rules and the model, not from the C.

  appscript.py play      seed 5, no save: ENTER 30 ms into a title tick; the autopilot (pilot.py) with fire held for
                         1,200 steps in ticks of 50 ms, then of 60, 99, 100 and 250 ms (the catch-up, its two steps
                         and the cap), both soft keys and a fire key besides 8; then hands off until the game ends;
                         ENTER refused 0.2 s into GAME OVER and taken at 0.6 s; a second game left to end
  appscript.py refuse    seed 9, a save (best 4321, 7 games): keys the title refuses, then a game left to end with
                         nothing pressed, so the best stays 4321
  appscript.py midsave   seed 9: the autopilot until the score passes 1,000, then the script ends with the game
                         running (the app saves on leaving, so the best must already be the score); salvo_check.sh
                         runs it with no save and with damaged ones, which must all be refused and give the same save
  appscript.py nexts NAME  what each tick of script NAME must ask for, one line a tick (-nexts prints the same)
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with its sound, '# sound LINE' as the runner's -v prints each tone and tune, then
'# expect save HEX tunes N tones N' (comments, so the runner skips them).
"""
import sys

import model as M
import pilot

KEY = {'4': 31, 'S1': 0, '6': 33, 'S6': 5, '8': 26, 'E': 18, '9': 27, 'X': 34, 'P': 46}
AGAIN_MS = 500
FALL = '392/120 0/40 330/120 0/40 262/300'
RISE = '523/100 659/100 784/200'


class Game:
    def __init__(self, seed, best=0, games=0):
        self.seed, self.best, self.games = seed, best, games
        self.out, self.nexts, self.sound = [], [], []
        self.clock = self.tunes = self.tones = 0
        self.g = None
        self.state = 'title'
        self.started = self.end_at = 0

    def key(self, k, down, at=None):
        at = self.clock if at is None else at
        self.out.append(f'key {KEY[k]} {1 if down else 0} {at}')
        if self.state == 'play':
            (self.g.key_down if down else self.g.key_up)(k)
        elif down and k == 'E' and (self.state == 'title' or at - self.end_at >= AGAIN_MS):
            self.g = M.Salvo(self.seed, self.games)
            self.games += 1
            self.started = at
            self.state = 'play'

    def press(self, k, at=None):
        self.key(k, True, at)
        self.key(k, False, at)

    def tick(self, ms=50, n=1, before_step=None):
        """n ticks of ms; before_step(game) runs before each step (to change keys there is a script line)."""
        for _ in range(n):
            self.clock += ms
            self.out.append(f'tick {ms}')
            if self.state != 'play':
                self.nexts.append(100)
                continue
            g = self.g
            g.acc += min(min(ms, self.clock - self.started), M.TICK_MAX)
            sound = 0
            while g.acc >= M.STEP_MS and not g.over:
                g.acc -= M.STEP_MS
                score, wavet = g.score, g.wavet
                g.step()
                if g.cool == M.COOL:                # a shot fired this step sets the cooldown to 6
                    sound |= 1
                if g.score > score:
                    sound |= 2
                self.best = max(self.best, g.score)
                if g.ship_hit:
                    sound |= 4
                if wavet == 0 and g.wavet == M.WAVE_TIMER:
                    sound |= 8
            if sound & 4:
                self.tune(FALL)
            elif sound & 8:
                self.tune(RISE)
            elif sound & 2:
                self.tone(880, 20)
            elif sound & 1:
                self.tone(1200, 10)
            if g.over:
                self.state = 'over'
                self.end_at = self.clock
                self.nexts.append(100)
            else:
                self.nexts.append(M.STEP_MS - g.acc)

    def tone(self, hz, ms):
        self.tones += 1
        self.sound.append(f'tone {hz} Hz {ms} ms at {self.clock} ms')

    def tune(self, notes):
        self.tunes += 1
        self.sound.append(f'tune of {len(notes.split())} notes at {self.clock} ms: {notes}')

    def fly(self, steps, ms=50):
        """the autopilot, fire held, for about `steps` steps in ticks of ms: its keys change only before a tick"""
        held = set()
        start = self.g.step_n
        while not self.g.over and self.g.step_n - start < steps:
            m = pilot.choose(self.g)
            want = {'8'} | ({'4'} if m < 0 else {'6'} if m > 0 else set())
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
        b[0], b[1] = ord('V'), 1
        b[2:6] = self.best.to_bytes(4, 'little')
        b[6:10] = self.games.to_bytes(4, 'little')
        return b.hex()

    def expect(self):
        return '\n'.join([f'# sound {l}' for l in self.sound]
                         + [f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'])


def play():
    g = Game(5)
    g.out.append('# appscript.py play: seed 5, no save')
    g.tick(100)
    g.key('E', True, g.clock + 30)          # ENTER 30 ms into the next title tick
    g.key('E', False, g.clock + 30)
    g.tick(100)                             # it counts 70 ms of the game
    g.fly(1200)
    for ms in (60, 99, 100, 250):
        g.out.append(f'# ticks of {ms} ms')
        g.fly(150, ms)
    g.out.append('# the soft keys and a fire key besides 8')
    g.key('S6', True)
    g.key('X', True)
    g.tick(50, 20)
    g.key('S1', True)                       # both directions: still
    g.tick(50, 5)
    g.key('S6', False)
    g.tick(50, 20)
    g.key('S1', False)
    g.key('X', False)
    g.out.append('# ENTER in play fires, as 8 does, and starts nothing')
    g.key('E', True)
    g.tick(50, 15)
    g.key('E', False)
    g.out.append('# hands off until the game ends')
    while g.state == 'play':
        g.tick(100)
    g.tick(20, 10)                          # 0.2 s into GAME OVER: ENTER refused
    g.press('E')
    g.tick(20, 20)
    g.press('E')                            # 0.6 s: taken
    while g.state == 'play':
        g.tick(100)
    g.tick(100, 3)
    return g


REFUSE_SAVE = (4321, 7)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.tick(100)
    for k in ('4', '8', 'S1', 'P'):         # the title takes only ENTER
        g.press(k)
    g.tick(100)
    g.press('E')
    while g.state == 'play':
        g.tick(100)
    g.tick(100, 3)
    assert g.best == REFUSE_SAVE[0]
    return g


def midsave():
    g = Game(9)
    g.out.append('# appscript.py midsave: seed 9; the script ends with the game running')
    g.tick(100)
    g.press('E')
    while g.g.score < 1000:
        g.fly(20)
    assert g.state == 'play' and g.best == g.g.score
    return g


SCRIPTS = {'play': play, 'refuse': refuse, 'midsave': midsave}


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
