#!/usr/bin/env python3
"""The reach (docs/catch.md, "The reach" and "The oracle" 5): can every star be caught?

The exact check: the set of every basket position a perfect player could hold, stepped one move at a time (left,
stay, right, clamped at the walls) and narrowed at each star's crossing to the positions that catch it. A star with no
position left can't be caught.

  reach.py rule GAMES STEPS        the rule on its own, from the doc's numbers: in each level, GAMES games of STEPS
                                   steps with nothing lost (so each reaches its level's limits), the star's x drawn
                                   uniform in the reach, at a random end of it, and at the end farther from the last
                                   star; with the reach as written, with r = move * gap, and with no reach. Prints a
                                   table; exits 1 unless the reach as written loses none, r = move * gap loses some in
                                   full, and no reach loses some in every level (the control: the check must bite).
  reach.py model MODE SEED GAME STEPS
                                   the model's own draws: the basket set on a position the set allows after each
                                   step, so the model catches every star the set can; prints 'lost N of M'.
  reach.py pilot MODE SEED GAME STEPS
                                   a perfect player as a key script: the positions are traced back from the end, so
                                   the basket always moves to where the next stars can still be caught. The script
                                   holds every key change and step, so the C replays it alone; it ends with
                                   '# expect caught N' (every star that crossed).
"""
import random
import sys

import model as M

MOVE = {'g': 6, 'm': 8, 'f': 8}
# the doc's table: lives, fall (start, step, limit), gap (start, step, limit), at once (start, step, limit)
LEVEL = {'g': (0, (32, 0, 32), (30, 0, 30), (3, 0, 3)),
         'm': (5, (32, 4, 48), (30, 2, 18), (3, 1, 4)),
         'f': (3, (32, 8, 96), (30, 3, 9), (3, 1, 6))}
SPAWN_Y, ROW, BOTTOM = -11 * 16, 202 * 16, 228 * 16


def catching(sx, pos):
    return {b for b in pos if sx < b + 44 and sx + 11 > b + 4}


def spread(pos, v):
    return {min(max(b + d, 0), M.BASKET_MAX) for b in pos for d in (-v, 0, v)}


def rule_game(mode, reach, seed, steps, how):
    """lost, crossed: one game of the rule alone, every star caught that can be"""
    _, (f0, fs, fl), (g0, gs, gl), (m0, ms, ml) = LEVEL[mode]
    v, rnd = MOVE[mode], random.Random(seed)
    stars, fall, gap, most, count, score, last = [], f0, g0, m0, 0, 0, None
    pos, lost, crossed = {176}, 0, 0
    for _ in range(steps):
        pos = spread(pos, v)
        before, keep = score, []
        for sx, sy in stars:
            ny = sy + fall
            if sy + 176 < ROW <= ny + 176:
                crossed += 1
                ok = catching(sx, pos)
                if ok:
                    pos, score = ok, score + 1
                    continue
                lost += 1
            if ny < BOTTOM:
                keep.append((sx, ny))
        stars = keep
        if count > 0:
            count -= 1
        if count == 0 and len(stars) < most:
            if last is None:
                x = rnd.randrange(390)
            else:
                r = reach(v, gap)
                lo, hi = max(0, last - r), min(389, last + r)
                x = (lo + rnd.randrange(hi - lo + 1) if how == 'uniform' else rnd.choice((lo, hi)) if how == 'ends'
                     else lo if last - lo >= hi - last else hi)
            stars.append((x, SPAWN_Y))
            last, count = x, gap
        for _ in range(score // 10 - before // 10):
            fall, gap, most = min(fall + fs, fl), max(gap - gs, gl), min(most + ms, ml)
    return lost, crossed


RULES = {'3/4': lambda v, g: 3 * v * g // 4, 'move*gap': lambda v, g: v * g, 'none': lambda v, g: 1000}


def rule(games, steps):
    lost = {}
    print(f'{"rule":9} {"draw":8} ' + ' '.join(f'{m:>16}' for m in 'gmf'))
    for name, reach in RULES.items():
        for how in ('uniform', 'ends', 'far'):
            row = []
            for mode in 'gmf':
                L = C = 0
                for s in range(games):
                    l, c = rule_game(mode, reach, s, steps, how)
                    L, C = L + l, C + c
                lost[name, how, mode] = L
                row.append(f'{L:>7} of {C:<6}')
            print(f'{name:9} {how:8} ' + ' '.join(row))
    ok = (all(lost['3/4', h, m] == 0 for h in ('uniform', 'ends', 'far') for m in 'gmf')
          and any(lost['move*gap', h, 'f'] for h in ('uniform', 'ends', 'far'))
          and all(lost['none', 'uniform', m] for m in 'gmf'))
    print('GREEN: the reach loses no star in any level, and both controls lose some' if ok else 'RED')
    return 0 if ok else 1


def forward(mode, seed, game, steps):
    """the model's game with every star caught that the set allows: the sets after each step, and lost/crossed"""
    g = M.Catch(mode, seed, game)
    v = MOVE[mode]
    pos, sets, lost, crossed = {g.bx}, [], 0, 0
    for _ in range(steps):
        pos = spread(pos, v)
        for sx, sy in g.stars:
            if sy + 176 < ROW <= sy + g.fall + 176:
                crossed += 1
                ok = catching(sx, pos)
                if ok:
                    pos = ok
                else:
                    lost += 1
        sets.append(pos)
        g.bx = min(pos)                     # placed, not moved: the model tests the catch where the basket is
        g.held, g.latch_left, g.latch_right = set(), False, False
        b = g.bx
        g.step()
        g.bx = b
        if g.over:
            break
    return sets, lost, crossed


def pilot(mode, seed, game, steps):
    sets, lost, crossed = forward(mode, seed, game, steps)
    assert lost == 0, f'lost {lost}: no perfect game to play'
    v, path = MOVE[mode], [min(sets[-1])]
    for t in range(len(sets) - 2, -1, -1):  # back from the end: a position this step that reaches the next one
        nxt = path[-1]
        path.append(min(b for b in sets[t] if nxt in {min(max(b + d, 0), M.BASKET_MAX) for d in (-v, 0, v)}))
    path.reverse()
    out, held, bx = [f'game {mode} {seed} {game}'], None, 176
    for p in path:
        want = None if p == bx else '4' if p < bx else '6'
        if want != held:
            if held:
                out.append(f'up {held}')
            if want:
                out.append(f'down {want}')
            held = want
        out.append('steps 1')
        bx = p
    out.append(f'# expect caught {crossed}')
    return out


def main():
    cmd = sys.argv[1]
    if cmd == 'rule':
        sys.exit(rule(int(sys.argv[2]), int(sys.argv[3])))
    mode, seed, game, steps = sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
    if cmd == 'model':
        _, lost, crossed = forward(mode, seed, game, steps)
        print(f'lost {lost} of {crossed}')
    elif cmd == 'pilot':
        print('\n'.join(pilot(mode, seed, game, steps)))


if __name__ == '__main__':
    main()
