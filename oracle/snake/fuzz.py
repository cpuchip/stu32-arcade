"""Fuzzed trace scripts: fuzz.py SEEDS GAMES prints, for each seed and game, random direction keys and TURN's quarter
turns (several at once included, so the queue fills) between runs of steps, until most games end."""
import random
import sys


def main():
    seeds, games = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(1)
    for seed in range(seeds):
        for game in range(games):
            print(f'game {seed << 32} {game}')
            for _ in range(r.randrange(40, 200)):
                for _ in range(r.choice((1, 1, 1, 2, 3))):
                    if r.random() < 0.5:
                        print(f"key {r.choice('8624')}")
                    else:
                        print(f"turn {r.choice('LR')}")
                print(f'step {r.randrange(1, 9)}')
            print('step 2000')


if __name__ == '__main__':
    main()
