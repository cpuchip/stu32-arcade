"""Long games: pilot.py MODE SEED GAME STEPS prints a trace script played by an autopilot on the model: fire held,
the ship turned toward the nearest rock (the near way round the wrap), and the engine fired in bursts while no rock is
within 150 px, so the ship moves about. After STEPS steps it lets go, so the game plays on to its end. The script
holds every key change and step, so the C replays the same game from it alone."""
import contextlib
import io
import math
import sys

import model as M


def step(g):
    """the model's step without its state line (the model prints one at every step)"""
    with contextlib.redirect_stdout(io.StringIO()):
        g.step()


def nearest(g):
    best = None
    for r in g.rocks:
        if r is None:
            continue
        dx = M.near(r[1] - g.x, M.W) / 256
        dy = M.near(r[2] - g.y, M.H) / 256
        d = math.hypot(dx, dy) - g.m['radius'][r[0]]
        if best is None or d < best[0]:
            best = (d, dx, dy)
    return best


def want(g):
    keys = {'G'}
    n = nearest(g)
    if n is None or not g.in_play():
        return keys
    d, dx, dy = n
    target = round(math.degrees(math.atan2(dx, -dy)) / 15) % 24
    diff = (target - g.a) % 24
    if diff and g.turn == 0:
        keys.add('6' if diff <= 12 else '4')
    if d > 150 and g.step_n % 40 < 6:
        keys.add('8')
    return keys


def main():
    mode, seed, game, steps = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    g = M.Game(mode, seed, game)
    print(f'game {mode} {seed} {game}')
    held = set()
    while not g.over and g.step_n < steps:
        w = want(g)
        for k in sorted(held - w):
            print(f'up {k}')
            g.key(k, False)
        for k in sorted(w - held):
            print(f'down {k}')
            g.key(k, True)
        held = w
        step(g)
        print('steps 1')
    for k in sorted(held):
        print(f'up {k}')
    print('steps 100000')


if __name__ == '__main__':
    main()
