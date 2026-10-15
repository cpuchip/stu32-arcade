"""SDK runner scripts for snake_app (sdk/runner/apprun.c), with what each must end with: the save, the tunes and the
tones, from docs/snake.md's rules (the clock, the best, the games count), not from the C.

  appscript.py play      seed 5, no save: the title's 4 and 6 changing the mode and back to TURN; ENTER partway
                         through a title tick; in TURN (4 or 6 and the soft keys in turn), the cycle autopilot for
                         19 foods (past the 60 ms floor, 60 ms ticks), ENTER, 8 and 2 doing nothing on the way; then
                         straight on into the wall or itself; ENTER refused before 0.5 s and taken after; straight
                         into the right wall
  appscript.py refuse    seed 9, a save (best 30, 7 games, TURN): keys the title refuses, then a game that runs
                         straight into the right wall, so the best stays 30
  appscript.py full      seed 3, no save: 8 2 4 6 chosen on the title, the cycle autopilot until the grid is full
                         (FULL), 60 ms ticks; too long to commit frames for, so its check is the save (its mode 1),
                         the tunes and the tones
  appscript.py midsave   seed 9, in TURN: three foods, then the script ends with the snake alive (the app saves on
                         leaving, so the best must already be 3); oracle/snake_check.sh runs it with no save and with
                         five damaged ones (the wrong magic, the wrong version, 10 bytes, a best of 878, a mode of 2),
                         which must all be refused and give the same save
  appscript.py loadfile  the refuse script's save, as hex
Each script ends with its sound, '# sound LINE' as the runner's -v prints each tone and tune, then
'# expect save HEX tunes N tones N' (comments, so the runner skips them).
"""
import sys

import cycle
import model as M

KEY = {0: 26, 1: 33, 2: 38, 3: 31, 'ENTER': 18, '1': 37, 'S1': 0, 'S6': 5}   # up 8, right 6, down 2, left 4
TURN, ABSOLUTE = 0, 1                # the modes; TURN is the default (docs/snake.md)
TICK_MAX, AGAIN_MS = 100, 500
FALL = '392/120 0/40 330/120 0/40 262/300'
RISE = '523/100 659/100 784/100 1047/300'


class Game:
    def __init__(self, seed, best=0, games=0, mode=TURN):
        self.seed = seed << 32
        self.best, self.games, self.mode = best, games, mode
        self.soft = False                # in TURN, alternate 4/6 with the soft keys
        self.out, self.clock = [], 0
        self.tunes = self.tones = 0
        self.sound = []
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
            while not self.s.end and self.acc >= M.interval(self.s.eaten):
                self.acc -= M.interval(self.s.eaten)
                eaten = self.s.eaten
                end = self.s.step()
                if self.s.eaten != eaten:
                    self.best = max(self.best, self.s.eaten)
                    if not end:            # the food that fills the grid has the win's tune instead
                        self.tones += 1
                        self.sound.append(f'tone 660 Hz 30 ms at {self.clock} ms')
                if end:
                    self.tunes += 1
                    notes = FALL if end == 'dead' else RISE
                    self.sound.append(f'tune of {len(notes.split())} notes at {self.clock} ms: {notes}')
                    self.end_at = self.clock

    def switch_mode(self, k=3):
        """4 (3) or 6 (1) on the title: the other mode"""
        self.key(k)
        self.mode = ABSOLUTE if self.mode == TURN else TURN

    def offer(self, d):
        """the key that asks for direction d in the mode, pressed; True when the queue took it"""
        if self.mode == ABSOLUTE:
            if self.s.offer(d):
                self.key(d)
                return True
            return False
        last = self.s.queue[-1] if self.s.queue else self.s.dir
        right = d == (last + 1) % 4
        if not right and d != (last + 3) % 4:
            return False                 # the same direction or its reverse: no key in TURN asks for it
        self.soft = not self.soft
        self.key(('S6' if right else 'S1') if self.soft else (1 if right else 3))
        return self.s.turn(right)

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

    def play(self, policy, ms=20, until=None):
        """ticks of ms until the game ends (or until(snake)); before a tick that steps, policy(snake) gives a direction
        or None. A tick over 60 ms can step twice, so a policy with longer ticks may turn only before the first"""
        while not self.s.end and not (until and until(self.s)):
            if self.steps_next_tick(ms):
                d = policy(self.s)
                if d is not None and self.offer(d):
                    assert ms <= 60 or self.s.dir == d or len(self.s.queue) == 1, 'a turn a long tick could split'
            self.tick(ms)

    def save(self):
        b = bytearray(16)
        b[0], b[1] = ord('S'), 1
        b[2:6] = self.best.to_bytes(4, 'little')
        b[6:10] = self.games.to_bytes(4, 'little')
        b[10] = self.mode
        return b.hex()

    def expect(self):
        return '\n'.join([f'# sound {l}' for l in self.sound]
                         + [f'# expect save {self.save()} tunes {self.tunes} tones {self.tones}'])


def follow(s):
    d = cycle.next_dir(s.body[-1])
    return d if d != s.dir else None


def follow_until(n, then):
    return lambda s: follow(s) if s.eaten < n else then


def play():
    g = Game(5)
    g.out.append('# appscript.py play: seed 5, no save; in TURN, the default')
    g.tick(100)                          # the title asks for 100 ms ticks
    g.out.append('# the title: 4 changes to 8 2 4 6, 6 back to TURN')
    g.switch_mode(3)
    g.tick(100)
    g.switch_mode(1)
    g.tick(100)
    g.new_game(at=g.clock + 30)          # ENTER 30 ms into the next one
    g.tick(100)                          # it counts 70 ms of the game
    g.play(follow, ms=60, until=lambda s: s.eaten >= 5)
    g.out.append('# ENTER during play: nothing; 8 and 2 in TURN: nothing')
    g.key('ENTER')
    g.key(0)
    g.key(2)
    g.play(follow, ms=60, until=lambda s: s.eaten >= 19)
    g.out.append('# past the 60 ms floor: straight on into the wall or itself, in 100 ms ticks (two steps in some)')
    g.play(lambda s: None, ms=100)
    g.out.append(f'# game 1: {g.s.end} at {g.s.eaten} eaten')
    g.tick(20, 10)                       # 0.2 s into GAME OVER: ENTER refused
    g.key('ENTER')
    g.tick(20, 20)
    g.new_game()                         # 0.6 s: taken
    g.out.append('# straight on into the right wall')
    g.play(lambda s: None)
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
    g.out.append('# 250 ms ticks: each counts 100 (the cap)')
    g.play(lambda s: None, ms=250)
    g.out.append(f'# game: {g.s.end} at {g.s.eaten} eaten')
    g.tick(20, 30)
    return g


def midsave():
    g = Game(9)
    g.out.append('# appscript.py midsave: seed 9, in TURN; leaves with the snake alive after three foods')
    g.tick(100)
    g.new_game()
    g.play(follow, ms=60, until=lambda s: s.eaten >= 3)
    g.tick(20, 5)
    g.out.append(f'# left alive at {g.s.eaten} eaten')
    return g


def full():
    g = Game(3)
    g.out.append('# appscript.py full: seed 3, no save; in 8 2 4 6 (4 on the title), the cycle autopilot until full')
    g.tick(100)
    g.switch_mode(3)
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
    g = {'play': play, 'refuse': refuse, 'full': full, 'midsave': midsave}[which]()
    print('\n'.join(g.out))
    print(g.expect())
    return 0


if __name__ == '__main__':
    sys.exit(main())
