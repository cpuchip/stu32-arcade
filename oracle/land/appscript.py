"""SDK runner scripts for land_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, from docs/land.md's rules (the score, the fuel tank, the counts), not from the C.

  appscript.py play      seed 5, no save: the x5 pad with each press of a turn or the engine on the next of its keys
                         (so the soft keys are held with the number row, and 4 and 6 with SOFT2 and SOFT5), the x2
                         pad, then x pressed during LANDED and held into the next round, a burn on x and then 9 (9
                         down before x is let go), SOFT5 tapped while 9 is held and 9 let go before the step (the
                         tap's press fires that step), GOLD, then SOFT2, then SOFT5 holding the engine while another
                         of its keys is tapped, 4 with SOFT1 and 6 with SOFT6 overlapping, + held with no engine key
                         down (it fires nothing), until the tank is empty, and GAME OVER
  appscript.py refuse    seed 9, a save (best 900, 3 landings, 4 crashes): keys the title refuses, ENTER partway
                         through a 100 ms title tick with 8 held (the first tick counts from ENTER), a burn to GAME
                         OVER, ENTER refused at 0.499 s and taken at 0.5 s by the key's time, between two ticks,
                         then free falls until a crash at exactly 200 fuel empties the tank (the low-fuel beep from
                         the crash) and GAME OVER
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with '# expect save HEX tunes N tones N' (a comment, so the runner skips it).
"""
import sys

import fair as F
import model as M

KEY = {'4': 31, '6': 33, '8': 26, 'ENTER': 18, '1': 37, '7': 25, '9': 27, 'x': 34, 'GOLD': 30, 'S1': 0, 'S6': 5,
       'S2': 1, 'S5': 4, '+': 46}
# docs/land.md, "The keys": the ship's three keys and the keys for each
SHIP = {'4': '4', 'S1': '4', '6': '6', 'S6': '6', '8': '8', '7': '8', '9': '8', 'x': '8', 'GOLD': '8', 'S2': '8',
        'S5': '8'}
ALL = {'4': ('4', 'S1'), '6': ('6', 'S6'), '8': ('8', '7', '9', 'x', 'GOLD', 'S2', 'S5')}
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

    def fly(self, how, held_in=(), first=(STEP_MS, 1), hold_into_next=None, rotate=False, burn=()):
        """one round, from its first step to its end: how is ('pad', mult), ('fall',) or ('burn',). held_in: keys
        held from before the round (the app keeps them); first: the first tick's ms and the steps it runs;
        hold_into_next: a key pressed 0.5 s into the round's end and held into the next round; rotate: each of the
        autopilot's presses of 4, 6 or 8 on the next of that ship key's keys (ALL); burn: lines before a burn's
        last. The ship sees each key's down, and a ship key's up when the last of its keys is let go."""
        n = self.landings + self.crashes
        verts, pads, start = M.make_terrain(self.seed, n)
        if how[0] == 'pad':
            padi = [p[3] for p in pads].index(how[1])
            r, _, _, lines = F.fly_any(self.seed, n, padi, self.fuel)
            assert r == ('land', how[1]), r
            lines = lines[1:]
        else:
            lines = (['key 8 1'] if how[0] == 'burn' and not held_in else []) + list(burn) + ['step 100000']
        s = M.new_ship(start, self.fuel)
        s['held'] = {SHIP[k] for k in held_in}
        held = set(held_in)
        end = None
        tick_ms, per_tick = first
        turn, pressed = {k: 0 for k in ALL}, {}
        for line in lines:
            w = line.split()
            if w[0] == 'key':
                k, down = w[1], w[2] == '1'
                if rotate and k in ALL:              # the autopilot's 4, 6 or 8: the next of its keys
                    if down:
                        pressed[k], turn[k] = ALL[k][turn[k] % len(ALL[k])], turn[k] + 1
                    k = pressed[k]
                self.out.append(f'key {KEY[k]} {w[2]} {self.clock}')
                if k not in SHIP:                    # + and the like: the ship never hears them
                    continue
                (held.add if down else held.discard)(k)
                if down or not any(SHIP[h] == SHIP[k] for h in held):
                    M.key(s, SHIP[k], down)
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
    g.fly(('pad', 5), rotate=True)
    g.fly(('pad', 2), hold_into_next='x')
    g.fly(('burn',), held_in={'x'}, burn=('step 200', 'key 9 1', 'key x 0', 'step 200', 'key S5 1', 'key S5 0',
                                          'key 9 0', 'step 1', 'key 9 1', 'step 50',
                                          'key GOLD 1', 'key 9 0', 'key 8 1', 'key 8 0', 'step 50',
                                          'key S2 1', 'key GOLD 0', 'key 7 1', 'key 7 0', 'step 50',
                                          'key S5 1', 'key S2 0', 'key x 1', 'key x 0', 'step 50',
                                          'key 4 1', 'key S1 1', 'key 4 0', 'step 12', 'key S1 0',
                                          'key 6 1', 'key S6 1', 'key 6 0', 'step 12', 'key S6 0',
                                          'key S5 0', 'key + 1', 'step 5', 'key + 0', 'key 9 1'))
    g.tick(30)                          # GAME OVER, then ENTER PLAYS after 0.5 s
    return g


REFUSE_SAVE = (900, 3, 4)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.clock += 100
    g.out.append('tick 100')            # the title asks for 100 ms ticks
    for k in ('8', '4', '1', 'GOLD', 'x', 'S2', 'S1'):  # and takes only ENTER
        g.press(k)
    g.clock += 100
    g.out.append('tick 100')
    g.new_game(at=g.clock + 30)         # 30 ms into the next 100 ms tick, 8 held from then
    g.key('8', True, at=g.clock + 30)
    g.fly(('burn',), held_in={'8'}, first=(100, 3))  # that tick runs 70 ms: 3 steps
    g.tick(24)                          # 0.48 s into GAME OVER
    g.press('ENTER', at=g.clock + 19)   # 0.499 s: refused
    g.new_game(at=g.clock + 20)         # 0.5 s by the key's time, before the tick that reaches it: taken
    first = (STEP_MS, 0)                # that tick counts from ENTER: no step
    while g.fuel:                       # 2,000 down by 200 a crash: the tenth, at 200, empties it
        g.fly(('fall',), first=first)
        first = (STEP_MS, 1)
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
