"""Fuzzed trace scripts: fuzz.py SEEDS ROUNDS prints, for each seed and round, random key downs and ups between
runs of steps (taps shorter than a step included), long enough to end most flights."""
import random
import sys


def main():
    seeds, rounds = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(1)
    for seed in range(seeds):
        for rnd in range(rounds):
            fuel = r.choice((2000, 2000, 300, 0, 1))
            print(f'round {seed} {rnd} {fuel}')
            for _ in range(r.randrange(20, 120)):
                k = r.choice('4468868')
                print(f'key {k} {r.randrange(2)}')
                if r.random() < 0.8:
                    print(f'step {r.randrange(0, 12)}')
            print('step 3000')


if __name__ == '__main__':
    main()
