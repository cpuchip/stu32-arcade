"""Fuzzed trace scripts: fuzz.py SEEDS GAMES prints, for each seed and game, random downs and ups of the nine keys
(both directions at once, taps shorter than a step, several fire keys held together) between ticks of 0-250 ms and
runs of steps, long enough for waves to change and many games to end."""
import random
import sys

KEYS = ('4', 'S1', '6', 'S6', '8', 'E', '9', 'X', '7', 'G', 'S2', 'S5')
MOVE, FIRE = KEYS[:4], KEYS[4:]


def main():
    seeds, games = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(1)
    for seed in range(seeds):
        for game in range(games):
            s = r.getrandbits(32)
            print(f'game {s} {game}')
            held = set()
            for _ in range(r.randrange(300, 1200)):
                for _ in range(r.choice((0, 1, 1, 2, 3))):
                    k = r.choice(MOVE if r.random() < 0.6 else FIRE)
                    if k in held and r.random() < 0.7:
                        print(f'up {k}')
                        held.discard(k)
                    else:
                        print(f'down {k}')
                        held.add(k)
                        if r.random() < 0.15:          # a tap shorter than a step
                            print(f'up {k}')
                            held.discard(k)
                if r.random() < 0.3:
                    print(f'steps {r.randrange(1, 12)}')
                else:
                    print(f'tick {r.choice((0, 1, 17, 49, 50, 51, 60, 99, 100, 101, 250, r.randrange(0, 260)))}')
            print('steps 3000')


if __name__ == '__main__':
    main()
