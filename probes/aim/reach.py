# SPDX-License-Identifier: MIT
# Prototype (throwaway): for every multiset of four tiles, the exact values reachable with + - * / using each tile
# once (fractions allowed in between), and how many RPN programs reach each integer target. Answers: is "pick the
# round's target from what the tiles reach" a good generator, and what ranges/tile sets make sense.
import itertools, sys
from fractions import Fraction as F
from collections import Counter
from functools import lru_cache

@lru_cache(maxsize=None)
def reach(tiles):  # tiles: sorted tuple; returns Counter value -> number of expression trees (ordered operands)
    if len(tiles) == 1:
        return Counter({F(tiles[0]): 1})
    out = Counter()
    n = len(tiles)
    idx = range(n)
    seen = set()
    for r in range(1, n):
        for left in itertools.combinations(idx, r):
            L = tuple(sorted(tiles[i] for i in left)); R = tuple(sorted(tiles[i] for i in idx if i not in left))
            if (L, R) in seen: continue
            seen.add((L, R))
            for a, ca in reach(L).items():
                for b, cb in reach(R).items():
                    c = ca * cb
                    out[a + b] += c; out[a - b] += c; out[a * b] += c
                    if b: out[a / b] += c
    return out

for top in (9, 13):
    sets = list(itertools.combinations_with_replacement(range(1, top + 1), 4))
    for lo, hi in ((10, 30), (10, 50), (24, 24), (1, 100)):
        solvable = 0; per = []; hard = 0
        for s in sets:
            r = reach(s)
            ints = [v for v in r if v.denominator == 1 and lo <= v <= hi]
            per.append(len(ints)); solvable += bool(ints)
            hard += sum(1 for v in ints if r[v] <= 4)
        per.sort()
        print(f"tiles 1-{top}: {len(sets)} sets; targets {lo}-{hi}: {solvable} sets reach one; "
              f"reachable targets per set min {per[0]} median {per[len(per)//2]} max {per[-1]}; (set,target) pairs with <=4 programs: {hard}")
print("example 3 3 8 8 -> 24 programs:", reach((3, 3, 8, 8))[F(24)])
print("example 1 1 1 1 reach 10-30:", sorted(int(v) for v in reach((1,1,1,1)) if v.denominator == 1 and 10 <= v <= 30))
