"""Long games: pilot.py SEED GAME STEPS prints a trace script played by an autopilot on the model: fire held, the ship
steered under the lowest alien, each step's move chosen by looking 10 steps ahead on copies of the model and taking
the first that keeps the ship alive. After STEPS steps it lets go of every key, so the game plays on to its end. The
script then holds every key change and step, so the C replays the same game from it alone."""
import copy
import sys

import model as M

DEPTH = 10


def safe(g, move):
    """How many steps the ship survives the move (held two steps, then still), up to DEPTH."""
    h = copy.deepcopy(g)
    h.held = {'8'} | ({'4'} if move < 0 else {'6'} if move > 0 else set())
    h.latched = set()
    for d in range(DEPTH):
        if d == 2:
            h.held = {'8'}
        h.step()
        if h.ship_hit or h.over:
            return d
    return DEPTH


def choose(g):
    tx, best = None, -999
    for i, a in enumerate(g.aliens):
        if a[0] == 'S' or (a[0] == 'D' and a[2] < 160):
            x, y = g.alien_rect(i)
            if y > best:
                best, tx = y, x + 6
    c = g.sx + 7
    pref = 0 if tx is None else (-1 if tx < c - 2 else 1 if tx > c + 2 else 0)
    if g.ship:
        return 0
    order = [pref] + [m for m in (0, -1, 1) if m != pref]
    return max((safe(g, m), -k, m) for k, m in enumerate(order))[2]


def main():
    seed, game, steps = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    g = M.Salvo(seed, game)
    print(f'game {seed} {game}')
    keys = {'8': '8', -1: '4', 1: '6'}
    held = set()
    while not g.over and g.step_n < steps:
        m = choose(g)
        want = {'8'} | ({'4'} if m < 0 else {'6'} if m > 0 else set())
        for k in sorted(held - want):
            print(f'up {k}')
        for k in sorted(want - held):
            print(f'down {k}')
        held = want
        g.held, g.latched = set(want), set()
        g.step()
        print('steps 1')
    for k in sorted(held):
        print(f'up {k}')
    print('steps 100000')


if __name__ == '__main__':
    main()
