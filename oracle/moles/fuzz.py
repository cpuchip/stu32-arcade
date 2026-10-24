"""Fuzzed trace scripts: fuzz.py SEEDS GAMES prints, for each seed and game, random downs and ups of the nine hole keys
and of 0, ENTER and + (several in one step, repeats while held, taps), between ticks of 0-250 ms and runs of steps,
in both modes, long enough for full games to end."""
import random
import sys

KEYS = tuple('123456789') + ('0', 'E', 'P')


def main():
    seeds, games = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(5)
    for seed in range(seeds):
        for game in range(games):
            print(f'game {"gf"[game % 2]} {r.getrandbits(32)} {game}')
            held = set()
            for _ in range(r.randrange(300, 1500)):
                for _ in range(r.choice((0, 0, 0, 1, 1, 2, 3))):
                    k = r.choice(KEYS)
                    if k in held and r.random() < 0.6:
                        print(f'up {k}')
                        held.discard(k)
                    else:
                        print(f'down {k}')
                        held.add(k)
                        if r.random() < 0.5:
                            print(f'up {k}')
                            held.discard(k)
                if r.random() < 0.3:
                    print(f'steps {r.randrange(1, 12)}')
                else:
                    print(f'tick {r.choice((0, 1, 17, 49, 50, 51, 60, 99, 100, 101, 250, r.randrange(0, 260)))}')
            print('steps 3000')


if __name__ == '__main__':
    main()
