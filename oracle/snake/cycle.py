"""A full game: an autopilot that follows a Hamiltonian cycle of the grid (docs/snake.md, the oracle's check 2).

The cycle: row 0 from left to right; rows 1-21 back and forth over columns 1-39 (odd rows leftward, even rows
rightward); then up column 0. The snake starts on it (row 10, moving right), and a snake that keeps to a cycle
always fills a stretch of it, so it never meets itself and eats every food until the grid is full.

  cycle.py script SEED GAME   the autopilot's keys and steps as a trace script, played in the model as it is made
  cycle.py grown GAMES        GAMES games that follow the cycle for 5-60 foods, then take random keys until they end:
                              long snakes, so deaths into the body (check 1's second half)
"""
import random
import sys

import model as M

W, H = M.W, M.H
KEY = {0: '8', 1: '6', 2: '2', 3: '4'}


def next_dir(c):
    x, y = c % W, c // W
    if y == 0:
        return 1 if x < W - 1 else 2
    if x == 0:
        return 0
    if y % 2:                       # odd rows run left, to column 1, then down (row 21: on to column 0)
        if x > 1:
            return 3
        return 2 if y < H - 1 else 3
    return 1 if x < W - 1 else 2    # even rows run right, then down


def script(seed, game):
    s = M.Snake(seed, game)
    s.place_food()
    out = [f'game {seed} {game}']
    run = 0
    while not s.end:
        d = next_dir(s.body[-1])
        if d != s.dir:
            if run:
                out.append(f'step {run}')
                run = 0
            out.append(f'key {KEY[d]}')
            assert s.offer(d)
        s.step()
        run += 1
    out.append(f'step {run}')
    out.append(f'# end {s.end} at length {len(s.body)}, {s.eaten} eaten')
    return out, s


def grown(n):
    r = random.Random(2)
    out = []
    ends = {}
    for game in range(n):
        seed, k = r.randrange(1 << 20) << 32, r.randrange(5, 61)
        s = M.Snake(seed, game)
        s.place_food()
        out.append(f'game {seed} {game}')
        while not s.end:
            if s.eaten < k:
                d = next_dir(s.body[-1])
                keys = [d] if d != s.dir else []
            else:
                keys = [r.randrange(4) for _ in range(r.choice((0, 0, 1, 2)))]
            for d in keys:
                out.append(f'key {KEY[d]}')
                s.offer(d)
            s.step()
            out.append('step 1')
        ends[s.end] = ends.get(s.end, 0) + 1
    return out, ends


if __name__ == '__main__':
    if sys.argv[1] == 'grown':
        lines, ends = grown(int(sys.argv[2]))
        print('\n'.join(lines))
        print(f'# ends: {ends}')
    else:
        lines, s = script(int(sys.argv[2]), int(sys.argv[3]))
        print('\n'.join(lines))
