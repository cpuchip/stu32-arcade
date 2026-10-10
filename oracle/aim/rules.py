# SPDX-License-Identifier: MIT
"""Aim's rules oracle (docs/aim.md, check 2): entry, the stack and undo, from the blueprint, independent of the C.

    python3 oracle/aim/rules.py SCRIPT        state after every key, the form the C driver (tests/aim_trace.c) prints
    python3 oracle/aim/rules.py --fuzz SEED N  writes N random rounds of keys, a script for both

A script is lines of
    round A B C D T      four tiles and the target; a fresh round
    key NAME             0-9, ENTER, PLUS, MINUS, TIMES, DIVIDE, SWAP, BACK
and for every key the output is one line:
    NAME beep=B entry=DIGITS stack=V,V used=UUUU won=W
V an exact value written n or n/d (reduced, the sign on n), X last; U a tile's 1 or 0 in the round's order.

The rules (docs/aim.md, "A round"): digits build an entry of at most 3; ENTER, an operator or SWAP ends it, and an
ended entry must equal a tile not yet used (the first such in order) or the key beeps, the entry clears and the key
does nothing else. ENTER with no entry beeps. An operator or SWAP needs two values, else beeps (a tile it ended
stays pushed). DIVIDE by 0 beeps and changes nothing. BACK deletes the entry's last digit, else undoes the last step
(a tile, an operation or a swap; a SWAP straight after a SWAP cancels it instead of adding a step), else beeps. A round is won when all four tiles are used and the stack is one value
equal to the target; after that every key beeps and changes nothing.
"""
import random
import sys
from fractions import Fraction

OPS = {"PLUS", "MINUS", "TIMES", "DIVIDE"}
KEYS = [str(d) for d in range(10)] + ["ENTER", "PLUS", "MINUS", "TIMES", "DIVIDE", "SWAP", "BACK"]


class Round:
    def __init__(self, tiles, target):
        self.tiles = tiles
        self.target = Fraction(target)
        self.used = [False] * 4
        self.stack = []
        self.entry = ""
        self.steps = []  # ("tile", i) | ("op", a, b) | ("swap",)
        self.won = False

    def end_entry(self):
        """True when there was no entry or it matched a tile (now pushed); False when it beeped."""
        if not self.entry:
            return True
        n = int(self.entry)
        self.entry = ""
        for i, t in enumerate(self.tiles):
            if not self.used[i] and t == n:
                self.used[i] = True
                self.stack.append(Fraction(t))
                self.steps.append(("tile", i))
                return True
        return False

    def key(self, k):
        if self.won:
            return True
        beep = False
        if k.isdigit():
            if len(self.entry) < 3:
                self.entry += k
            else:
                beep = True
        elif k == "BACK":
            if self.entry:
                self.entry = self.entry[:-1]
            elif self.steps:
                s = self.steps.pop()
                if s[0] == "tile":
                    self.stack.pop()
                    self.used[s[1]] = False
                elif s[0] == "op":
                    self.stack.pop()
                    self.stack += [s[1], s[2]]
                else:
                    self.stack[-1], self.stack[-2] = self.stack[-2], self.stack[-1]
            else:
                beep = True
        elif k == "ENTER":
            had = bool(self.entry)
            beep = not self.end_entry() or not had
        elif k in OPS or k == "SWAP":
            if not self.end_entry():
                beep = True
            elif len(self.stack) < 2:
                beep = True
            elif k == "SWAP":
                self.stack[-1], self.stack[-2] = self.stack[-2], self.stack[-1]
                if self.steps and self.steps[-1] == ("swap",):
                    self.steps.pop()  # a swap after a swap cancels it, so the undo list stays bounded
                else:
                    self.steps.append(("swap",))
            else:
                b, a = self.stack[-1], self.stack[-2]
                if k == "DIVIDE" and b == 0:
                    beep = True
                else:
                    v = a + b if k == "PLUS" else a - b if k == "MINUS" else a * b if k == "TIMES" else a / b
                    del self.stack[-2:]
                    self.stack.append(v)
                    self.steps.append(("op", a, b))
        else:
            raise ValueError(k)
        if all(self.used) and len(self.stack) == 1 and self.stack[0] == self.target:
            self.won = True
        return beep

    def line(self, k, beep):
        vals = ",".join(str(v.numerator) if v.denominator == 1 else f"{v.numerator}/{v.denominator}" for v in self.stack)
        return (f"{k} beep={int(beep)} entry={self.entry} stack={vals} "
                f"used={''.join('1' if u else '0' for u in self.used)} won={int(self.won)}")


def run(path):
    r = None
    for raw in open(path):
        w = raw.split()
        if not w or w[0].startswith("#"):
            continue
        if w[0] == "round":
            r = Round([int(x) for x in w[1:5]], int(w[5]))
        elif w[0] == "key":
            print(r.line(w[1], r.key(w[1])))


def fuzz(seed, n):
    g = random.Random(seed)
    for _ in range(n):
        tiles = [g.randint(1, 9) for _ in range(4)]
        print("round", *tiles, g.randint(10, 30))
        # weighted toward the round's own tiles and the operators, so rounds get deep
        pool = [str(t) for t in tiles] * 3 + ["ENTER"] * 4 + ["PLUS", "MINUS", "TIMES", "DIVIDE", "SWAP"] * 2 + \
            ["BACK"] * 2 + KEYS
        for _ in range(g.randint(1, 24)):
            print("key", g.choice(pool))


if __name__ == "__main__":
    if sys.argv[1] == "--fuzz":
        fuzz(int(sys.argv[2]), int(sys.argv[3]))
    else:
        run(sys.argv[1])
