"""Fuzzed trace scripts: fuzz.py SEEDS GAMES prints, for each seed and game, random opens and flags (a few flags
before the first open, chords on opened cells, opens after the end), long enough that most games end."""
import random
import sys

CELLS = 24 * 13


def main():
    seeds, games = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(1)
    for seed in range(seeds):
        for game in range(games):
            print(f'game {seed << 32} {game}')
            for _ in range(r.randrange(0, 3)):
                print(f'flag {r.randrange(CELLS)}')
            for _ in range(r.randrange(20, 120)):
                print(f"{r.choice(('open', 'open', 'flag'))} {r.randrange(CELLS)}")


if __name__ == '__main__':
    main()
