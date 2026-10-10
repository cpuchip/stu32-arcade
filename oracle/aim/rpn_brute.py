# SPDX-License-Identifier: MIT
# Aim oracle self-check (docs/aim.md check 1): a second, differently built enumerator. Every RPN program
# over every tile order, run on a stack. Its reachable integer sets (1-100, tiles 1-9) must equal reach.py's with
# the counts dropped: python3 oracle/aim/rpn_brute.py | cmp - <(python3 oracle/aim/reach.py | sed 's|/[0-9]*||g')
import itertools
from fractions import Fraction as F
OPS = "+-*/"
def run(prog):
    st = []
    for t in prog:
        if isinstance(t, int):
            st.append(F(t))
        else:
            if len(st) < 2: return None
            b = st.pop(); a = st.pop()
            if t == "/" and b == 0: return None
            st.append(a + b if t == "+" else a - b if t == "-" else a * b if t == "*" else a / b)
    return st[0] if len(st) == 1 else None
shapes = [s for s in itertools.product("no", repeat=7) if s.count("n") == 4]
for s in itertools.combinations_with_replacement(range(1, 10), 4):
    vals = set()
    for perm in set(itertools.permutations(s)):
        for ops in itertools.product(OPS, repeat=3):
            for shape in shapes:
                it_n = iter(perm); it_o = iter(ops)
                v = run([next(it_n) if c == "n" else next(it_o) for c in shape])
                if v is not None and v.denominator == 1 and 1 <= v <= 100: vals.add(int(v))
    print(" ".join(map(str, s)) + ":" + "".join(f" {t}" for t in sorted(vals)))
