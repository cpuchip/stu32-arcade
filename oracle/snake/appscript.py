"""SDK runner scripts for snake_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, from docs/snake.md's rules (the clock, the best, the games count), not from the C.

  appscript.py play      seed 5, no save: ENTER partway through a title tick; the cycle autopilot for 19 foods
                         (past the 60 ms floor, 60 ms ticks), then up into the wall or itself; ENTER refused before 0.5 s and taken
                         after; a reverse key dropped, then straight up into the top wall
  appscript.py refuse    seed 9, a save (best 30, 7 games): keys the title refuses, then a game that runs straight
                         into the right wall, so the best stays 30
  appscript.py full      seed 3, no save: the cycle autopilot until the grid is full (FULL), 60 ms ticks; too long
                         to commit frames for, so its check is the save, the tunes and the tones
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with '# expect save HEX tunes N tones N' (a comment, so the runner skips it).
"""
import sys

import cycle
import model as M

KEY = {0: 26, 1: 33, 2: 38, 3: 31, 'ENTER': 18, '1': 37}   # up is the 8 key, right 6, down 2, left 4
TICK_MAX, STEPS_MAX, AGAIN_MS = 100, 4, 500


class Game:
    def __init__(self, seed, best=0, games=0):
        self.seed = seed << 32
        self.best, self.games = best, games
        self.out, self.clock = [], 0
        self.tunes = self.tones = 0
        self.s = None
        self.acc = self.started = 0

    def key(self, k, at=None):
        at = self.clock if at is None else at
        self.out.append(f'key {KEY[k]} 1 {at}')
        self.out.append(f'key {KEY[k]} 0 {at}')

    def tick(self, ms=20, n=1):
        """n ticks of ms; plays the app's steps, so a policy can be asked before each one"""
        for _ in range(n):
            self.clock += ms
            self.out.append(f'tick {ms}')
            if self.s is None or self.s.end:
                continue
            dt = min(ms, self.clock - self.started)
            self.acc += min(dt, TICK_MAX)
            for _ in range(STEPS_MAX):
                step = M.interval(self.s.eaten)
                if self.acc < step or self.s.end:
                    break
                self.acc -= step
                eaten = self.s.eaten
                end = self.s.step()
                if self.s.eaten != eaten:
                    self.best = max(self.best, self.s.eaten)
                    self.tones += 1
                if end:
                    self.tunes += 1
                    self.end_at = self.clock

    def steps_next_tick(self, ms):
        """whether the next tick of ms will step (ticks of at most 60 ms step at most once)"""
        dt = min(ms, self.clock + ms - self.started)
        return self.acc + min(dt, TICK_MAX) >= M.interval(self.s.eaten)

    def new_game(self, at=None):
        self.key('ENTER', at)
        self.started = self.clock if at is None else at
        self.s = M.Snake(self.seed, self.games)
        self.s.place_food()
        self.games += 1
        self.acc = 0

    def play(self, policy, ms=20):
        """ticks of ms until the game ends; before a tick that steps, policy(snake) gives a direction or None"""
        assert ms <= 60
        while not self.s.end:
            if self.steps_next_tick(ms):
                d = policy(self.s)
                if d is not None and self.s.offer(d):
                    self.key(d)
            self.tick(ms)

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('S'), 1
        b[2:6] = self.best.to_bytes(4, 'little')
        b[6:10] = self.games.to_bytes(4, 'little')
        return b.hex()

    def expect(self):
        return f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'


def follow(s):
    d = cycle.next_dir(s.body[-1])
    return d if d != s.dir else None


def follow_until(n, then):
    return lambda s: follow(s) if s.eaten < n else then


def play():
    g = Game(5)
    g.out.append('# appscript.py play: seed 5, no save')
    g.tick(100)                          # the title asks for 100 ms ticks
    g.new_game(at=g.clock + 30)          # ENTER 30 ms into the next one
    g.tick(100)                          # it counts 70 ms of the game
    g.play(follow_until(19, 0), ms=60)
    g.out.append(f'# game 1: {g.s.end} at {g.s.eaten} eaten')
    g.tick(20, 10)                       # 0.2 s into GAME OVER: ENTER refused
    g.key('ENTER')
    g.tick(20, 20)
    g.new_game()                         # 0.6 s: taken
    g.out.append('# a reverse key, dropped')
    assert not g.s.offer(3)
    g.key(3)
    g.play(lambda s: 0)
    g.out.append(f'# game 2: {g.s.end} at {g.s.eaten} eaten')
    g.tick(20, 30)
    return g


REFUSE_SAVE = (30, 7)


def refuse():
    g = Game(9, *REFUSE_SAVE)
    g.out.append('# appscript.py refuse: seed 9, loads the save from appscript.py loadfile')
    g.tick(100)
    for k in (0, 2, '1'):                # the title takes only ENTER
        g.key(k)
    g.tick(100)
    g.new_game()
    g.play(lambda s: None)
    g.out.append(f'# game: {g.s.end} at {g.s.eaten} eaten')
    g.tick(20, 30)
    return g


def full():
    g = Game(3)
    g.out.append('# appscript.py full: seed 3, no save; the cycle autopilot until the grid is full')
    g.tick(100)
    g.new_game()
    g.play(follow, ms=60)
    g.out.append(f'# game: {g.s.end} at length {len(g.s.body)}, {g.s.eaten} eaten')
    g.tick(20, 30)
    return g


def main():
    which = sys.argv[1]
    if which == 'loadfile':
        print(Game(0, *REFUSE_SAVE).save())
        return 0
    g = {'play': play, 'refuse': refuse, 'full': full}[which]()
    print('\n'.join(g.out))
    print(g.expect())
    return 0


if __name__ == '__main__':
    sys.exit(main())
