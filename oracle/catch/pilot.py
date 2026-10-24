"""Long games: pilot.py MODE SEED GAME STEPS [SET...] prints a trace script played by an autopilot on the model: the
basket steered under the lowest star still above the mouth. Each SET is a `NAME=VALUE` given to `set` after `game` (to
start full at its limits). After STEPS steps it lets go, so a full game plays on to its end; a gentle one plays STEPS
more and stops. The script holds every key change and step, so the C replays the same game from it alone."""
import sys

import model as M


def choose(g):
    """-1, 0 or 1: toward the star that will reach the mouth first"""
    best = None
    for x, y in g.stars:
        if y + M.STAR_W * 16 < M.MOUTH_ROW * 16 and (best is None or y > best[1]):
            best = (x, y)
    if best is None:
        return 0
    c, t = g.bx + 24, best[0] + 5
    return -1 if t < c - 3 else 1 if t > c + 3 else 0


def main():
    mode, seed, game, steps = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    g = M.Catch(mode, seed, game)
    print(f'game {mode} {seed} {game}')
    for s in sys.argv[5:]:
        k, v = s.split('=')
        setattr(g, k, int(v))
        print(f'set {k} {v}')
    held = set()
    while not g.over and g.step_n < steps:
        m = choose(g)
        want = {'4'} if m < 0 else {'6'} if m > 0 else set()
        for k in sorted(held - want):
            print(f'up {k}')
            g.up(k)
        for k in sorted(want - held):
            print(f'down {k}')
            g.down(k)
        held = want
        g.step()
        print('steps 1')
    for k in sorted(held):
        print(f'up {k}')
    print(f'steps {100000 if mode == "f" else steps}')


if __name__ == '__main__':
    main()
