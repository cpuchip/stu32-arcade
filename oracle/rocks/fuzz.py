"""Fuzzed trace scripts: fuzz.py SEEDS GAMES prints, for each seed and game, random downs and ups of the twelve keys
with a job and of ENTER and + (turn, thrust and fire held together, both turns at once, taps shorter than a step),
between ticks of 0-250 ms and runs of steps, in both modes, long enough for waves to change and games to end."""
import random
import sys

TURN = ('4', 'S1', '6', 'S6')
THRUST = ('7', '8', '9', 'S2')
FIRE = ('G', '5', 'X', 'S5')
OTHER = ('E', 'P')


def main():
    seeds, games = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(7)
    for seed in range(seeds):
        for game in range(games):
            print(f'game {"gf"[game % 2]} {r.getrandbits(32)} {game}')
            held = set()
            for _ in range(r.randrange(400, 1600)):
                for _ in range(r.choice((0, 1, 1, 2, 3))):
                    k = r.choice(r.choice((TURN, TURN, THRUST, FIRE, FIRE, OTHER)))
                    if k in held and r.random() < 0.7:
                        print(f'up {k}')
                        held.discard(k)
                    else:
                        print(f'down {k}')
                        held.add(k)
                        if r.random() < 0.15:
                            print(f'up {k}')
                            held.discard(k)
                if r.random() < 0.3:
                    print(f'steps {r.randrange(1, 12)}')
                else:
                    print(f'tick {r.choice((0, 1, 17, 49, 50, 51, 60, 99, 100, 101, 250, r.randrange(0, 260)))}')
            print('steps 4000')


if __name__ == '__main__':
    main()
