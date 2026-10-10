"""No board needs a guess (docs/mines.md, the oracle's check 2), proven by a complete solver, not the game's.

The complete solver sees what a player sees (the opened numbers and the mine total). It splits the closed cells
next to opened numbers into groups joined by the numbers they share, enumerates every mine placement in each group
that fits every number, and combines the groups with the closed cells beyond the frontier and the total. A cell is
safe exactly when no placement consistent with everything has a mine there, and a mine exactly when none leaves it
empty. It opens only safe cells (with the flood), so a board it clears needs no guess, and one where it finds no
safe cell before the end needs one.

  proof.py deals SEEDS        the C's dealing for seeds 0..SEEDS-1, game 0, first open at a cell drawn per seed:
                              each must be cleared; prints the attempts, and boards the game's solver gave up on that
                              the complete one clears (fine), and any the game's solver cleared that it cannot (a bug)
  proof.py control FILE       the board in FILE (mines, first open) must be reported as needing a guess
  proof.py play SEEDS         trace scripts for the C: games won by opening only the complete solver's safe cells
                              (with flags on its mines and a chord where one opens something), one per seed
"""
import multiprocessing
import random
import sys

import model as M

CELLS, N, NB = M.CELLS, M.MINES, M.NB
LIMIT = 2_000_000


class TooBig(Exception):
    pass


def components(frontier, cons):
    """the frontier's cells grouped by the numbers they share"""
    parent = {c: c for c in frontier}

    def find(c):
        while parent[c] != c:
            parent[c] = parent[parent[c]]
            c = parent[c]
        return c
    for cells, _ in cons:
        cells = list(cells)
        for c in cells[1:]:
            parent[find(c)] = find(cells[0])
    groups = {}
    for c in frontier:
        groups.setdefault(find(c), []).append(c)
    return list(groups.values())


def enumerate_group(group, cons):
    """{mine count: (assignments, cells that are a mine in some, cells empty in some)} over every fit"""
    idx = {c: i for i, c in enumerate(group)}
    mine_cons = [(sorted(idx[c] for c in cells), need) for cells, need in cons if cells & set(group)]
    by_cell = [[] for _ in group]
    for k, (cells, _) in enumerate(mine_cons):
        for i in cells:
            by_cell[i].append(k)
    val = [None] * len(group)
    out = {}
    seen = [0]

    def ok(k):
        cells, need = mine_cons[k]
        s = sum(1 for i in cells if val[i] == 1)
        free = sum(1 for i in cells if val[i] is None)
        return s <= need <= s + free

    def rec(i):
        seen[0] += 1
        if seen[0] > LIMIT:
            raise TooBig
        if i == len(group):
            m = sum(val)
            e = out.setdefault(m, [0, set(), set()])
            e[0] += 1
            for j, v in enumerate(val):
                (e[1] if v else e[2]).add(group[j])
            return
        for v in (0, 1):
            val[i] = v
            if all(ok(k) for k in by_cell[i]):
                rec(i + 1)
        val[i] = None
    rec(0)
    return out


def sums(sets):
    """every total reachable by taking one count from each set"""
    acc = {0}
    for s in sets:
        acc = {a + b for a in acc for b in s}
    return acc


def deduce(opened, known, num):
    """(safe cells, mine cells) proven from the opened numbers, the known mines and the total"""
    closed = [c for c in range(CELLS) if c not in opened and c not in known]
    cons = []
    for c in opened:
        if num[c]:
            cells = frozenset(n for n in NB[c] if n not in opened and n not in known)
            if cells:
                cons.append((cells, num[c] - sum(n in known for n in NB[c])))
    frontier = set().union(*[cells for cells, _ in cons]) if cons else set()
    interior = [c for c in closed if c not in frontier]
    R = N - len(known)
    groups = components(frontier, cons)
    tables = [enumerate_group(g, cons) for g in groups]
    counts = [set(t) for t in tables]
    safe, mine = set(), set()
    for gi, t in enumerate(tables):
        others = sums(counts[:gi] + counts[gi + 1:])
        fits = {m for m in t if any(0 <= R - m - o <= len(interior) for o in others)}
        for c in groups[gi]:
            can_mine = any(c in t[m][1] for m in fits)
            can_empty = any(c in t[m][2] for m in fits)
            if not can_mine:
                safe.add(c)
            elif not can_empty:
                mine.add(c)
    if interior:
        totals = sums(counts)
        can_mine = any(1 <= R - t <= len(interior) for t in totals)
        can_empty = any(0 <= R - t <= len(interior) - 1 for t in totals)
        if not can_mine:
            safe.update(interior)
        elif not can_empty:
            mine.update(interior)
    return safe, mine


def prove(mines, f):
    """('cleared' | 'guess', the opened cells, the steps): the complete solver from opening f"""
    num = M.counts(mines)
    opened, known = set(), set()
    M.flood(opened, known, num, f)
    steps = []
    while len(opened) < CELLS - N:
        safe, mine = deduce(opened, known, num)
        assert not (safe & mines) and not (mine - mines), 'the complete solver drew a false conclusion'
        known |= mine
        new = [c for c in sorted(safe) if c not in opened]
        if not new:
            return 'guess', opened, steps
        for c in new:
            if c not in opened:
                M.flood(opened, known, num, c)
        steps.append((sorted(mine), new))
    return 'cleared', opened, steps


def first_cell(seed):
    return random.Random(seed).randrange(CELLS)


def deals(n):
    bad = rescued = 0
    attempts = []
    with multiprocessing.Pool() as pool:
        rows = pool.map(one_deal, range(n), chunksize=4)
    for seed, att, ok in rows:
        attempts.append(att)
        if ok != 'cleared' and att < M.TRIES:
            bad += 1
            print(f'  BUG: seed {seed}: the game\'s solver cleared it, the complete one needs a guess')
        if ok != 'cleared' and att == M.TRIES:
            print(f'  seed {seed}: the fallback (100 attempts) dealt a board needing a guess')
    # the game's solver against the complete one on the attempts it rejected, for the record
    r = random.Random(3)
    for _ in range(200):
        f = r.randrange(CELLS)
        banned = set(NB[f]) | {f}
        mines = set(r.sample([c for c in range(CELLS) if c not in banned], N))
        g_ok = M.solver(mines, f)[0]
        c_ok = prove(mines, f)[0] == 'cleared'
        if g_ok and not c_ok:
            bad += 1
            print('  BUG: a random board the game\'s solver clears needs a guess')
        rescued += (not g_ok) and c_ok
    worst = max(rows, key=lambda r: r[1])
    print(f'proof: {n - bad} of {n} dealt boards cleared without a guess by the complete solver; attempts median '
          f'{sorted(attempts)[n // 2]}, most {worst[1]} (seed {worst[0]}); '
          f'of 200 random boards, {rescued} the game\'s solver gave up on the complete one cleared')
    return 1 if bad else 0


def one_deal(seed):
    g = M.Game(seed << 32, 0)
    f = first_cell(seed)
    g.open(f)
    return seed, g.attempts, prove(g.mines, f)[0]


def control(path):
    rows = dict(l.split(None, 1) for l in open(path).read().splitlines() if l.strip() and not l.startswith('#'))
    mines = set(int(c) for c in rows['mines'].split(','))
    f = int(rows['first'])
    res, opened, _ = prove(mines, f)
    print(f'control: the complete solver says {res} ({len(opened)} of {CELLS - N} safe cells opened)')
    return 0 if res == 'guess' else 1


def play(n):
    for seed in range(n):
        g = M.Game(seed << 32, 0)
        f = first_cell(seed)
        print(f'game {seed << 32} 0')
        print(f'open {f}')
        g.open(f)
        num = g.num
        known = set()
        chorded = False
        while g.state == 'play':
            safe, mine = deduce(g.opened, known, num)
            for c in sorted(mine - known):
                print(f'flag {c}')
                g.flag(c)
            known |= mine
            new = [c for c in sorted(safe) if c not in g.opened]
            if not new:
                break
            # one chord a game, where an opened number's flags are all placed and it has a safe closed neighbour
            if not chorded:
                for c in sorted(g.opened):
                    if num[c] and sum(n in g.flags for n in NB[c]) == num[c] and any(n in new for n in NB[c]):
                        print(f'open {c}')
                        g.open(c)
                        chorded = True
                        break
            for c in new:
                if c not in g.opened and g.state == 'play':
                    print(f'open {c}')
                    g.open(c)
        print(f'# {g.state}')


if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'deals':
        sys.exit(deals(int(sys.argv[2])))
    if mode == 'control':
        sys.exit(control(sys.argv[2]))
    if mode == 'play':
        play(int(sys.argv[2]))
