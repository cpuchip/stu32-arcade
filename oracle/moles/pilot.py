"""Long games: pilot.py MODE SEED GAME STEPS DELAY prints a trace script played by an autopilot on the model: each
mole is bonked DELAY steps after it starts rising (a tap, down and up before the next step). DELAY -1 never presses,
so a full game is lost to getaways. After STEPS steps it lets go, so a full game plays on to its end; a gentle one
plays STEPS more and stops. The script holds every key and step, so the C replays the same game from it alone."""
import sys

import model as M

DIGIT = {h: k for k, h in M.KEY_HOLE.items()}


def presses(g, delay):
    """the keys to tap before the next step: the moles seen for `delay` steps"""
    if delay < 0:
        return []
    out = []
    for i, (s, n) in enumerate(g.holes):
        seen = n + 1 if s == 'R' else M.RISE_STEPS + n + 1 if s == 'U' else -1
        if seen >= delay:
            out.append(DIGIT[i])
    return out


def main():
    mode, seed, game, steps, delay = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
    g = M.Game(mode, seed, game)
    print(f'game {mode} {seed} {game}')
    while not g.over and g.step < steps:
        for k in presses(g, delay):
            print(f'down {k}')
            print(f'up {k}')
            g.down(k)
            g.up(k)
        g.run_step()
        print('steps 1')
    print(f'steps {100000 if mode == "f" else steps}')


if __name__ == '__main__':
    main()
