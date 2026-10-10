# Target: four numbers to the round's target, in RPN (blueprint, for abacus's read)

Michael's decision 71 (abacus aa30714): the second game is Target, then Lander, Snake and Mines, each a unit
through soroban's gate. This is Target's design and the checks that will judge it, written before any of its
code. The puzzle is the old one of making a number from four others with + − × ÷; the name, the rules below and
everything on the screen are ours, and nothing calls it "24" (Suntex holds marks on "24", "24 GAME" and
"TWENTY-FOUR CHALLENGE"; memory feedback_check_names_for_marks).

## A round

Four tiles, each a digit 1 to 9, and a target. The player reaches the target by entering the tiles in RPN on
the calculator's own keys, each tile used exactly once, with + − × ÷. A round is won when every tile is used and
the stack holds one value equal to the target. Values in between may be fractions: 8 3 8 3 ÷ − ÷ is
8 ÷ (3 − 8 ÷ 3) = 24, a solution only fractions allow, and the game keeps every value exact.

**Entry is the calculator's.** Digits build a number in X; ENTER, an operator or x↔y ends the entry. An ended
entry must match a tile not yet used, or the game beeps and clears the entry. So 8 ENTER 3 × is how two tiles
multiply, as on the calculator; 8 3 is the number 83, which no tile matches. ENTER after ENTER would copy a tile
(a second use), so the game beeps instead.

| key | does |
|---|---|
| 0-9 | builds the entry (a tile is 1-9, so 0 only ever fails the match) |
| ENTER | ends the entry |
| + − × ÷ | ends any entry, then Y op X; ÷ by 0 beeps and changes nothing |
| x↔y | ends any entry, then swaps X and Y |
| ← | in an entry, deletes a digit; otherwise undoes the last step (a tile or an operation) |
| SOFT1-SOFT6 | the labels on the screen's bottom line: UNDO, SKIP, SHOW, and NEW after a round |
| C | the framework's: saves and ends the game |

SHOW gives up the round and writes one solution as the keys to press (8 ENTER 3 ENTER 8 ENTER 3 ÷ − ÷), which
also teaches RPN. SKIP draws a new round and counts as a pass.

## Making a round

The game draws four tiles from `app_seed()` mixed with a round counter, computes every value the four can reach
(a small search over the 2-, 3- and 4-tile subsets, exact fractions in int32: measured over every set of four digits, no value's numerator passes 6,561 or its denominator 729, and no term an operation forms before reducing passes 6,561), and
picks the target from the reachable integers in the round's range. A round is never unsolvable by construction,
and the oracle checks it for every round the generator makes from a range of seeds.

Measured with the prototype (an exhaustive Python enumerator, 5 s for all 495 multisets of four digits):

| tiles 1-9, targets | sets that reach one | reachable targets a set, median (max) |
|---|---|---|
| 10-30 | 491 of 495 | 17 (21) |
| 10-50 | 491 of 495 | 27 (41) |
| exactly 24 | 404 of 495 | 1 |

A set that reaches nothing in 10-30 is redrawn; there are four (1 1 1 1, 1 1 1 2, 1 1 1 3 and 1 1 2 2). **Difficulty** is the number of
distinct solutions (expression trees, operand order kept, a tile split counted once): 3 3 8 8 to 24 has exactly
one. Rounds start in 10-30 with many solutions, and the range and the scarcity grow with the streak.

## The screen (400 x 240, draw.h only)

The target in large digits at the top; the four tiles under it, a used tile in half shade; the stack's four
levels (T, Z, Y, X) on the right in the calculator's own order, X at the bottom, the entry under X; the round, the
streak and the best streak along the top; the soft-key labels on the bottom line. A won round flashes the stack
and plays a short tune; a refused key beeps.

## What it saves

The best streak, rounds won, rounds shown and the difficulty reached, 16 bytes, taken whole or not at all
(magic, version).

## Where it lives

`games/target/`: `target_logic.{c,h}` (the rules: tiles, entry, stack, undo, the solver and the generator, no
drawing and no framework calls), `target_app.c` (the app_def: keys and clock into the logic, and the drawing), and
`stu32-game.txt` (name, folder GAMES, MIT, author arcade). Native C, like Babal; `make wasmcheck` builds it as a
.wasm too, as it does Babal. All state in ctx, about 12 KB of it the solver's table (a set of four digits reaches at most 922 distinct values), nothing on the stack past
`-Wframe-larger-than=1024`.

## The oracle (written before the game)

1. **Solver:** the Python enumerator (`oracle/target/reach.py`, independent of the C) against the C solver, built
   on the host, for every multiset of four digits: the same reachable integers and the same solution counts.
2. **Rules:** an independent Python model of the entry, the stack and undo, fed the same key sequences as the C
   logic (from scripts and a seeded fuzzer): the same state after every key (stack, tiles used, entry, beep, won).
3. **Rounds:** every round the generator makes from seeds 0 to 9,999 is solvable, and its target is in range.
4. **Frames:** the SDK runner's scripts (a won round, a shown one, every refused key), frame hashes on the host,
   the same hashes from the .wasm, then on the S3 at soroban's gate.
5. **Falsified on purpose, each red then green:** a solver that never divides, logic that lets a tile be used
   twice, a generator that ignores its range, a draw that skips the used-tile shade.

## Open, for Michael

- **The name on the APPS menu:** names there are 1 to 5 characters, so "TARGET" doesn't fit. TARGT or AIM fit.
  The title screen can still say Target. TARGET is also a large retailer's mark, and my search didn't settle
  whether it covers games; a menu name of our own avoids the question.
- **Tiles 1-9 or 1-13:** 1-9 is one key a tile. 1-13 (a deck of cards) needs two-digit entry, which RPN already
  handles. I recommend 1-9.
- **The target's range:** 10-30 to start, growing with the streak. Exactly 24 is one fixed round of the same game,
  but not as its name.
