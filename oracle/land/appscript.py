"""SDK runner scripts for land_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, from docs/land.md's rules (the score, the fuel tank, the counts), not from the C.

  appscript.py play      seed 5, no save: four rounds (the x5 pad, the x2 pad, a free fall, a burn until the tank
                         is empty) and GAME OVER
  appscript.py refuse    seed 9, a save (best 900, 3 landings, 4 crashes): keys the title refuses, a burn to GAME
                         OVER, ENTER refused before 0.5 s and taken after, a free fall, the next round's start
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

    def press(self, name):
        self.out.append(f'key {KEY[name]} 1 {self.clock}')
        self.out.append(f'key {KEY[name]} 0 {self.clock}')

    def new_game(self):
        self.press('ENTER')
        self.score, self.fuel, self.low = 0, M.FUEL0, False

    def fly(self, how):
        """one round, from its first step to its end: how is ('pad', mult), ('fall',) or ('burn',)"""
        n = self.landings + self.crashes
        verts, pads, start = M.make_terrain(self.seed, n)
        if how[0] == 'pad':
            padi = [p[3] for p in pads].index(how[1])
            r, _, _, lines = F.fly_any(self.seed, n, padi, self.fuel)
            assert r == ('land', how[1]), r
            lines = lines[1:]
        else:
            lines = (['key 8 1'] if how[0] == 'burn' else []) + ['step 100000']
        s = M.new_ship(start, self.fuel)
        held = set()
        end = None
        for line in lines:
            w = line.split()
            if w[0] == 'key':
                self.out.append(f'key {KEY[w[1]]} {w[2]} {self.clock}')
                M.key(s, w[1], w[2] == '1')
                (held.add if w[2] == '1' else held.discard)(w[1])
            else:
                for _ in range(int(w[1])):
                    end = M.step(s, verts, pads)
                    self.tick()
                    if not self.low and s['fuel'] < 200:
                        self.low = True
                        self.tones += 1
                    if end:
                        break
            if end:
                break
        assert end, 'the flight did not end'
        for k in sorted(held):           # let go during the end of the round
            self.out.append(f'key {KEY[k]} 0 {self.clock}')
        self.tunes += 1
        if end[0] == 'land':
            self.score += 50 * end[1]
            self.landings += 1
            self.fuel = s['fuel']
        else:
            self.crashes += 1
            self.fuel = max(0, s['fuel'] - M.CRASH_COST)
        self.out.append(f'# round {n}: {end[0]} {end[1]}, fuel {self.fuel}, score {self.score}')
        self.tick(ENDED_MS // STEP_MS)      # the last of these starts the next round or GAME OVER
        if self.fuel == 0:
            self.best = max(self.best, self.score)

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
    for how in (('pad', 5), ('pad', 2), ('fall',), ('burn',)):
        g.fly(how)
    g.tick(30)                          # GAME OVER, then ENTER PLAYS after 0.5 s
    return g


REFUSE_SAVE = (900, 3, 4)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.tick()
    for k in ('8', '4', '1'):           # the title takes only ENTER
        g.press(k)
        g.tick()
    g.new_game()
    g.fly(('burn',))
    g.tick(10)                          # 0.2 s into GAME OVER: ENTER refused
    g.press('ENTER')
    g.tick(20)
    g.new_game()                        # 0.6 s: taken
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
