"""Fuzzed trace scripts: fuzz.py SEEDS GAMES prints, for each seed and game, a board and rules in turn, random opens
and flags (a few flags before the first open, chords on opened cells, opens after the end), long enough that most
games end. One game in three deals as the app does, a few attempts at a time with moves between that must do
nothing, before its first open."""
import random
import sys

BOARDS = (('easy', 81), ('medium', 256), ('hard', 480))


def main():
    seeds, games = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(1)
    for seed in range(seeds):
        for game in range(games):
            board, cells = BOARDS[(seed + game) % 3]
            rules = 'classic' if r.random() < 0.3 else 'noguess'
            print(f'game {seed << 32} {game} {board} {rules}')
            for _ in range(r.randrange(0, 3)):
                print(f'flag {r.randrange(cells)}')
            if r.random() < 1 / 3:
                print(f'deal {r.randrange(cells)}')
                for _ in range(r.randrange(1, 6)):
                    print(f'step {r.randrange(1, 5)}')
                    print(f"{r.choice(('open', 'flag'))} {r.randrange(cells)}")
                print('step 400')
            for _ in range(r.randrange(20, 120)):
                print(f"{r.choice(('open', 'open', 'flag'))} {r.randrange(cells)}")


if __name__ == '__main__':
    main()
