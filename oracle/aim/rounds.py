# SPDX-License-Identifier: MIT
"""Aim's rounds oracle (docs/aim.md, check 3), on what the C generator made:

    tests/aim_trace rounds SEEDS N | python3 oracle/aim/rounds.py

Each line "seed level n: a b c d target". Every round must be solvable, its target in 10..30 + 10 x level, and
under the level's solution cap whenever the tiles reach any target in range under it (no limit, 24, 12, 6, 3).
The solutions are reach.py's, not the C's.
"""
import sys
from fractions import Fraction

from reach import reach

CAP = [0, 24, 12, 6, 3]
bad = 0
n = 0
for line in sys.stdin:
    head, rest = line.split(":")
    seed, level, i = map(int, head.split())
    *tiles, target = map(int, rest.split())
    r = reach(tuple(sorted(tiles)))
    hi = 30 + 10 * level
    in_range = {t: r[Fraction(t)] for t in range(10, hi + 1) if r[Fraction(t)]}
    under = [t for t, c in in_range.items() if not CAP[level] or c <= CAP[level]]
    why = None
    if not r[Fraction(target)]:
        why = "unsolvable"
    elif not 10 <= target <= hi:
        why = "out of range"
    elif under and target not in under:
        why = f"over the cap ({r[Fraction(target)]} > {CAP[level]})"
    n += 1
    if why:
        bad += 1
        if bad <= 10:
            print(f"{line.strip()}: {why}")
print(f"rounds: {n} checked, {bad} bad")
sys.exit(1 if bad or not n else 0)
