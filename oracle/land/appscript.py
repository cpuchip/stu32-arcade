"""SDK runner scripts for land_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, from docs/land.md's rules (the score, the fuel tank, the counts), not from the C.

  appscript.py play      seed 5, no save: the x5 pad, the x2 pad, then 8 pressed during LANDED and held into the
                         next round, a burn until the tank is empty, and GAME OVER
  appscript.py refuse    seed 9, a save (best 900, 3 landings, 4 crashes): keys the title refuses, ENTER partway
                         through a 100 ms title tick with 8 held (the first tick counts from ENTER), a burn to GAME
                         OVER, ENTER refused before 0.5 s and taken after, then free falls until a crash at exactly
                         200 fuel empties the tank (the low-fuel beep from the crash) and GAME OVER
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with '# expect save HEX tunes N tones N' (a comment, so the runner skips it).
"""
import sys

import fair as F
import model as M

KEY = {'4': 31, '6': 33, '8': 26, 'ENTER': 18, '1': 37}
STEP_MS, ENDED_MS, AGAIN_MS = 20, 1500, 500


class Game:
    def __init__(self, seed, best=0, landings=0, crashes=0):
        self.seed = seed << 32
        self.best, self.landings, self.crashes = best, landings, crashes
        self.out, self.clock = [], 0
        self.tunes = self.tones = 0
        self.score, self.fuel, self.low = 0, M.FUEL0, False

    def tick(self, n=1):
        for _ in range(n):
            self.clock += STEP_MS
            self.out.append(f'tick {STEP_MS}')

    def key(self, name, down, at=None):
        self.out.append(f'key {KEY[name]} {int(down)} {self.clock if at is None else at}')

    def press(self, name, at=None):
        self.key(name, True, at)
        self.key(name, False, at)

    def new_game(self, at=None):
        self.press('ENTER', at)
        self.score, self.fuel, self.low = 0, M.FUEL0, False

    def low_fuel(self, fuel):
        if not self.low and fuel < 200:
            self.low = True
            self.tones += 1

    def fly(self, how, held_in=(), first=(STEP_MS, 1), hold_into_next=None):
        """one round, from its first step to its end: how is ('pad', mult), ('fall',) or ('burn',). held_in: keys
        held from before the round (the app keeps them); first: the first tick's ms and the steps it runs;
        hold_into_next: a key pressed 0.5 s into the round's end and held into the next round"""
        n = self.landings + self.crashes
        verts, pads, start = M.make_terrain(self.seed, n)
        if how[0] == 'pad':
            padi = [p[3] for p in pads].index(how[1])
            r, _, _, lines = F.fly_any(self.seed, n, padi, self.fuel)
            assert r == ('land', how[1]), r
            lines = lines[1:]
        else:
            lines = (['key 8 1'] if how[0] == 'burn' and '8' not in held_in else []) + ['step 100000']
        s = M.new_ship(start, self.fuel)
        s['held'] = set(held_in)
        held = set(held_in)
        end = None
        tick_ms, per_tick = first
        for line in lines:
            w = line.split()
            if w[0] == 'key':
                self.out.append(f'key {KEY[w[1]]} {w[2]} {self.clock}')
                M.key(s, w[1], w[2] == '1')
                (held.add if w[2] == '1' else held.discard)(w[1])
            else:
                left = int(w[1])
                while left and not end:
                    self.clock += tick_ms
                    self.out.append(f'tick {tick_ms}')
                    for _ in range(min(per_tick, left)):
                        end = M.step(s, verts, pads)
                        self.low_fuel(s['fuel'])
                        left -= 1
                        if end:
                            break
                    tick_ms, per_tick = STEP_MS, 1
            if end:
                break
        assert end, 'the flight did not end'
        for k in sorted(held):           # let go during the end of the round
            self.key(k, False)
        self.tunes += 1
        if end[0] == 'land':
            self.score += 50 * end[1]
            self.best = max(self.best, self.score)
            self.landings += 1
            self.fuel = s['fuel']
        else:
            self.crashes += 1
            self.fuel = max(0, s['fuel'] - M.CRASH_COST)
            self.low_fuel(self.fuel)
        self.out.append(f'# round {n}: {end[0]} {end[1]}, fuel {self.fuel}, score {self.score}')
        if hold_into_next:
            self.tick(AGAIN_MS // STEP_MS)
            self.key(hold_into_next, True)
            self.tick((ENDED_MS - AGAIN_MS) // STEP_MS)
        else:
            self.tick(ENDED_MS // STEP_MS)  # the last of these starts the next round or GAME OVER

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('L'), 1
        for at, v in ((2, self.best), (6, self.landings), (10, self.crashes)):
            b[at:at + 4] = v.to_bytes(4, 'little')
        return b.hex()

    def expect(self):
        return f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'


def play():
    g = Game(5)
    g.out.append('# appscript.py play: seed 5, no save')
    g.tick()
    g.new_game()
    g.fly(('pad', 5))
    g.fly(('pad', 2), hold_into_next='8')
    g.fly(('burn',), held_in={'8'})
    g.tick(30)                          # GAME OVER, then ENTER PLAYS after 0.5 s
    return g


REFUSE_SAVE = (900, 3, 4)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.clock += 100
    g.out.append('tick 100')            # the title asks for 100 ms ticks
    for k in ('8', '4', '1'):           # and takes only ENTER
        g.press(k)
    g.clock += 100
    g.out.append('tick 100')
    g.new_game(at=g.clock + 30)         # 30 ms into the next 100 ms tick, 8 held from then
    g.key('8', True, at=g.clock + 30)
    g.fly(('burn',), held_in={'8'}, first=(100, 3))  # that tick runs 70 ms: 3 steps
    g.tick(10)                          # 0.2 s into GAME OVER: ENTER refused
    g.press('ENTER')
    g.tick(20)
    g.new_game()                        # 0.6 s: taken
    while g.fuel:                       # 2,000 down by 200 a crash: the tenth, at 200, empties it
        g.fly(('fall',))
    g.tick(5)
    return g


def main():
    which = sys.argv[1]
    if which == 'loadfile':
        print(Game(0, *REFUSE_SAVE).save())
        return 0
    g = {'play': play, 'refuse': refuse}[which]()
    print('\n'.join(g.out))
    print(g.expect())
    return 0


if __name__ == '__main__':
    sys.exit(main())
