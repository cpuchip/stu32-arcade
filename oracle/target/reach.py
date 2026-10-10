# SPDX-License-Identifier: MIT
"""Target's solver oracle (docs/target.md, check 1), independent of the game's C.

    python3 oracle/target/reach.py [--top 9] [--lo 1] [--hi 100]

For every multiset of four tiles 1..top, in ascending order, one line:
    a b c d: t1/n1 t2/n2 ...
each tk an integer target in [lo, hi] the four reach with + - * / (each tile once, exact fractions between), and
nk the number of expression trees reaching it: operand order kept, and a split of the tiles into two sides counted
once however equal tiles fall (3 3 8 8 reaches 24 in exactly one). The game's C solver prints the same form for the
same arguments, and the check is a byte comparison.
"""
import argparse
import itertools
from collections import Counter
from fractions import Fraction
from functools import lru_cache


@lru_cache(maxsize=None)
def reach(tiles):
    """tiles: a sorted tuple. Returns a Counter: exact value -> number of expression trees."""
    if len(tiles) == 1:
        return Counter({Fraction(tiles[0]): 1})
    out = Counter()
    seen = set()
    n = len(tiles)
    for r in range(1, n):
        for left in itertools.combinations(range(n), r):
            a_side = tuple(sorted(tiles[i] for i in left))
            b_side = tuple(sorted(tiles[i] for i in range(n) if i not in left))
            if (a_side, b_side) in seen:
                continue
            seen.add((a_side, b_side))
            for a, ca in reach(a_side).items():
                for b, cb in reach(b_side).items():
                    c = ca * cb
                    out[a + b] += c
                    out[a - b] += c
                    out[a * b] += c
                    if b:
                        out[a / b] += c
    return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--top", type=int, default=9)
    p.add_argument("--lo", type=int, default=1)
    p.add_argument("--hi", type=int, default=100)
    a = p.parse_args()
    for s in itertools.combinations_with_replacement(range(1, a.top + 1), 4):
        r = reach(s)
        ts = sorted(int(v) for v in r if v.denominator == 1 and a.lo <= v <= a.hi)
        print(" ".join(map(str, s)) + ":" + "".join(f" {t}/{r[Fraction(t)]}" for t in ts))


if __name__ == "__main__":
    main()
