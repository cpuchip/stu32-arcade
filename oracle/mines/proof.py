"""No board needs a guess (docs/mines.md, the oracle's check 2), proven by a complete solver, not the game's.

The complete solver sees what a player sees (the opened numbers and the mine total). It splits the closed cells
next to opened numbers into groups joined by the numbers they share, enumerates every mine placement in each group
that fits every number, and combines the groups with the closed cells beyond the frontier and the total. A cell is
safe exactly when no placement consistent with everything has a mine there, and a mine exactly when none leaves it
empty. It opens only safe cells (with the flood), so a board it clears needs no guess, and one where it finds no
safe cell before the end needs one.

  proof.py deals BOARD SEEDS  the C's dealing for seeds 0..SEEDS-1, game 0, first open at a cell drawn per seed:
                              each must be cleared; prints the attempts, and boards the game's solver gave up on that
                              the complete one clears (fine), and any the game's solver cleared that it cannot (a bug)
  proof.py control FILE       the board in FILE (board, mines, first open) must be reported as needing a guess
  proof.py same BOARD SEEDS   the two ways of tabling a group (listing every fit, and the sweep) must give the same
                              safe cells and mines at every step of random boards' proofs, where the listing is
                              small enough to finish
  proof.py play BOARD SEEDS   trace scripts for the C: games won by opening only the complete solver's safe cells
                              (with flags on its mines and a chord where one opens something), one per seed
"""
import multiprocessing
import random
import sys

import model as M

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


def sweep_group(group, cons):
    """the same table as enumerate_group (its counts left out), without listing the fits: a pass over the group's
    cells in an order that walks the numbers, keeping at each step only the needs still open (the numbers with
    cells on both sides of it) and the mine counts that reach them; a pass from the other end gives what the cells
    beyond each step can supply, and a cell can be a mine (or empty) with m mines exactly when the two meet"""
    gset = set(group)
    mine_cons = [(cells, need) for cells, need in cons if cells & gset]
    # the order: breadth first from the group's first cell in row order, through the numbers they share
    by = {c: [] for c in group}
    for k, (cells, _) in enumerate(mine_cons):
        for c in cells:
            by[c].append(k)
    order, seen = [], {min(group)}
    queue = [min(group)]
    while queue:
        c = queue.pop(0)
        order.append(c)
        for k in by[c]:
            for d in sorted(mine_cons[k][0]):
                if d not in seen:
                    seen.add(d)
                    queue.append(d)
    assert len(order) == len(group)
    L = len(order)

    def run(seq):
        """[the states at each boundary of seq]: {((number, need left), ...): {mines so far}}"""
        pos = {c: i for i, c in enumerate(seq)}
        first = {k: min(pos[c] for c in cells) for k, (cells, _) in enumerate(mine_cons)}
        last = {k: max(pos[c] for c in cells) for k, (cells, _) in enumerate(mine_cons)}
        out = [{(): {0}}]
        for i, c in enumerate(seq):
            nxt = {}
            for state, ms in out[-1].items():
                for v in (0, 1):
                    s2 = step(state, i, c, v, first, last, pos)
                    if s2 is not None:
                        nxt.setdefault(s2, set()).update(m + v for m in ms)
            out.append(nxt)
        return out, first, last, pos

    def step(state, i, c, v, first, last, pos):
        left = dict(state)
        for k in by[c]:
            r = (mine_cons[k][1] if first[k] == i else left[k]) - v
            after = sum(1 for d in mine_cons[k][0] if pos[d] > i)
            if r < 0 or r > after:
                return None
            if last[k] == i:
                left.pop(k, None)
            else:
                left[k] = r
        return tuple(sorted(left.items()))

    fwd, first, last, pos = run(order)
    back, _, _, _ = run(order[::-1])
    # the pass from the end, keyed as the forward pass keys it: the forward need left is what the far side holds
    need = {k: n for k, (_, n) in enumerate(mine_cons)}
    beyond = [None] * (L + 1)
    for j in range(L + 1):
        beyond[L - j] = {tuple(sorted((k, need[k] - r) for k, r in st)): ms for st, ms in back[j].items()}
    out = {}
    for i, c in enumerate(order):
        for state, ms in fwd[i].items():
            for v in (0, 1):
                s2 = step(state, i, c, v, first, last, pos)
                if s2 is None or s2 not in beyond[i + 1]:
                    continue
                for a in ms:
                    for b in beyond[i + 1][s2]:
                        e = out.setdefault(a + v + b, [None, set(), set()])
                        (e[1] if v else e[2]).add(c)
    return out


GROUP = sweep_group  # how deduce tables a group; enumerate_group is the cross-check's (proof.py same)


def sums(sets):
    """every total reachable by taking one count from each set"""
    acc = {0}
    for s in sets:
        acc = {a + b for a in acc for b in s}
    return acc


def deduce(b, opened, known, num):
    """(safe cells, mine cells) proven from the opened numbers, the known mines and the total"""
    closed = [c for c in range(b.CELLS) if c not in opened and c not in known]
    cons = []
    for c in opened:
        if num[c]:
            cells = frozenset(n for n in b.NB[c] if n not in opened and n not in known)
            if cells:
                cons.append((cells, num[c] - sum(n in known for n in b.NB[c])))
    frontier = set().union(*[cells for cells, _ in cons]) if cons else set()
    interior = [c for c in closed if c not in frontier]
    R = b.N - len(known)
    groups = components(frontier, cons)
    tables = [GROUP(g, cons) for g in groups]
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


def prove(b, mines, f):
    """('cleared' | 'guess', the opened cells, the steps): the complete solver from opening f"""
    num = M.counts(b, mines)
    opened, known = set(), set()
    M.flood(b, opened, known, num, f)
    steps = []
    while len(opened) < b.CELLS - b.N:
        safe, mine = deduce(b, opened, known, num)
        assert not (safe & mines) and not (mine - mines), 'the complete solver drew a false conclusion'
        known |= mine
        new = [c for c in sorted(safe) if c not in opened]
        if not new:
            return 'guess', opened, steps
        for c in new:
            if c not in opened:
                M.flood(b, opened, known, num, c)
        steps.append((sorted(mine), new))
    return 'cleared', opened, steps


def first_cell(b, seed):
    return random.Random(seed).randrange(b.CELLS)


def deals(board, n):
    b = M.BOARDS[board]
    bad = rescued = undecided = 0
    attempts = []
    with multiprocessing.Pool() as pool:
        rows = pool.map(one_deal, [(board, s) for s in range(n)], chunksize=4)
    for seed, att, ok in rows:
        attempts.append(att)
        if ok != 'cleared' and att < M.TRIES:
            bad += 1
            print(f'  BUG: seed {seed}: the game\'s solver cleared it, the complete one needs a guess')
        if ok != 'cleared' and att == M.TRIES:
            print(f'  seed {seed}: the fallback ({M.TRIES} attempts) dealt a board needing a guess')
    # the game's solver against the complete one on the attempts it rejected, for the record
    r = random.Random(3)
    for _ in range(200):
        f = r.randrange(b.CELLS)
        banned = set(b.NB[f]) | {f}
        mines = set(r.sample([c for c in range(b.CELLS) if c not in banned], b.N))
        g_ok = M.solver(b, mines, f)[0]
        try:
            c_ok = prove(b, mines, f)[0] == 'cleared'
        except TooBig:  # too many placements to list: undecided, so a bug only where the game's solver cleared it
            undecided += 1
            if g_ok:
                bad += 1
                print('  BUG: a random board the game\'s solver clears is too big for the complete one to decide')
            continue
        if g_ok and not c_ok:
            bad += 1
            print('  BUG: a random board the game\'s solver clears needs a guess')
        rescued += (not g_ok) and c_ok
    worst = max(rows, key=lambda r: r[1])
    cleared = sum(1 for _, _, ok in rows if ok == 'cleared')
    print(f'proof: {cleared} of {n} dealt boards cleared without a guess by the complete solver; attempts median '
          f'{sorted(attempts)[n // 2]}, most {worst[1]} (seed {worst[0]}); '
          f'of 200 random boards, {rescued} the game\'s solver gave up on the complete one cleared'
          + (f', {undecided} too big to decide (the game\'s solver gave up on each)' if undecided else ''))
    return 1 if bad else 0


def one_deal(arg):
    board, seed = arg
    g = M.Game(seed << 32, 0, board)
    f = first_cell(g.b, seed)
    g.open(f)
    return seed, g.attempts, prove(g.b, g.mines, f)[0]


def control(path):
    rows = dict(l.split(None, 1) for l in open(path).read().splitlines() if l.strip() and not l.startswith('#'))
    b = M.BOARDS[rows['board'].strip()]
    mines = set(int(c) for c in rows['mines'].split(','))
    f = int(rows['first'])
    res, opened, _ = prove(b, mines, f)
    print(f'control: the complete solver says {res} ({len(opened)} of {b.CELLS - b.N} safe cells opened)')
    return 0 if res == 'guess' else 1


def same_one(arg):
    """one random board's proof, the sweep's deductions against the listing's at every step it can list"""
    global GROUP
    board, seed = arg
    b = M.BOARDS[board]
    r = random.Random(seed)
    f = r.randrange(b.CELLS)
    banned = set(b.NB[f]) | {f}
    mines = set(r.sample([c for c in range(b.CELLS) if c not in banned], b.N))
    num = M.counts(b, mines)
    opened, known = set(), set()
    M.flood(b, opened, known, num, f)
    steps = listed = 0
    while len(opened) < b.CELLS - b.N:
        GROUP = sweep_group
        sw = deduce(b, opened, known, num)
        GROUP = enumerate_group
        try:
            ls = deduce(b, opened, known, num)
            listed += 1
            if sw != ls:
                return seed, steps, listed, 'differ'
        except TooBig:
            pass
        GROUP = sweep_group
        steps += 1
        safe, mine = sw
        known |= mine
        new = [c for c in sorted(safe) if c not in opened]
        if not new:
            break
        for c in new:
            if c not in opened:
                M.flood(b, opened, known, num, c)
    return seed, steps, listed, 'same'


def same(board, n):
    with multiprocessing.Pool() as pool:
        rows = pool.map(same_one, [(board, s) for s in range(n)], chunksize=2)
    bad = [r for r in rows if r[3] != 'same']
    for seed, step_, _, _ in bad:
        print(f'  BUG: seed {seed}, step {step_}: the sweep and the listing deduce different cells')
    print(f'same: {board}, {n} random boards, {sum(r[1] for r in rows)} proof steps, {sum(r[2] for r in rows)} of them '
          f'small enough to list, ' + (f'{len(bad)} boards where the sweep and the listing differ' if bad else
                                       'the sweep and the listing deducing the same cells at each'))
    return 1 if bad else 0


def play(board, n):
    for seed in range(n):
        g = M.Game(seed << 32, 0, board)
        f = first_cell(g.b, seed)
        print(f'game {seed << 32} 0 {board} noguess')
        print(f'open {f}')
        g.open(f)
        num = g.num
        known = set()
        chorded = False
        while g.state == 'play':
            safe, mine = deduce(g.b, g.opened, known, num)
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
                    if num[c] and sum(n in g.flags for n in g.b.NB[c]) == num[c] and any(n in new for n in g.b.NB[c]):
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
        sys.exit(deals(sys.argv[2], int(sys.argv[3])))
    if mode == 'control':
        sys.exit(control(sys.argv[2]))
    if mode == 'same':
        sys.exit(same(sys.argv[2], int(sys.argv[3])))
    if mode == 'play':
        play(sys.argv[2], int(sys.argv[3]))
