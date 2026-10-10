# Mines: clear the field, with no guess ever needed (blueprint)

Michael's decision 71 (abacus aa30714): after Aim, Lander and Snake, Mines, the last of the four, a unit through
soroban's gate. MINES has five letters, so it is its own APPS name; "Minesweeper" is Microsoft's title, so nothing
here uses it. Every board can be cleared by reasoning alone, never a guess (on Michael's board as mines-rules,
abacus #6246, with the classic rule as the fallback). This is Mines' design and the checks that will judge it,
written before any of its code.

## A game

A field of 24 × 13 cells hides 48 mines. The player moves a cursor with 8, 2, 4 and 6 and opens a cell with 5 (or
ENTER). An opened cell shows how many of its up to 8 neighbours are mines; a 0 opens its neighbours by itself. 0
puts a flag on a closed cell, or takes it off. Opening an opened number whose flags match it opens its other closed
neighbours. Opening a mine loses; opening every cell that is not a mine wins. The time counts from the first open,
and the best winning time is kept.

The first cell opened is never a mine, nor are its neighbours, so the first open always clears an area. From there,
every board is one a player can finish by reasoning: the game only deals boards its own solver can clear from that
first open without a guess.

| key | does |
|---|---|
| 8 2 4 6 | the cursor up, down, left, right; held, it repeats |
| 5, ENTER | open the cell under the cursor (on an opened number: open its neighbours, as above) |
| 0 | a flag on the closed cell under the cursor, or off |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

## The rules

- **The field.** Cells (x, y), x 0-23, y 0-12, numbered in row order, `c = y × 24 + x`; 312 in all. A
  cell's neighbours are the up to 8 around it, in the order (−1,−1), (0,−1), (1,−1), (−1,0), (1,0), (−1,1), (0,1),
  (1,1).
- **Opening** a closed, unflagged cell: a mine loses. Otherwise it opens, and if it shows 0 its closed, unflagged
  neighbours open in turn, and so on (a flood; a flagged cell is never opened by it). Opening a flagged cell, or an
  opened cell that is not a number, does nothing.
- **The chord.** Opening an opened number n with exactly n flags among its neighbours opens each of its closed,
  unflagged neighbours in neighbour order, as above; a mine among them loses (the rest are still opened first, so
  the loss shows them). With any other count of flags, nothing.
- **Flags** go on and off closed cells only, without limit; the count shown is 48 minus the flags, and can go below
  0.
- **The end.** A loss shows every mine. A win, when every cell that is not a mine is open, flags every mine.

## Dealing a board

On the first open of a game, at cell f. The state is `x = mix64(seed ^ mix64(game))` (Aim's mix64), and each draw
is `x = mix64(x)`, value `x % n`. The game's seed is `(uint64)app_seed() << 32`; `game` counts the games ever
started, kept in the save.

1. **An attempt.** The allowed cells are every cell but f and its neighbours, in row order (a list of A cells).
   Then for i = 0 to 47: `j = i + draw(A − i)`, swap the list's i-th and j-th cells. The first 48 are the mines.
2. **The test.** The game's solver (below) plays the board from opening f. If it opens every cell that is not a
   mine, the attempt is dealt. Otherwise the next attempt draws from where the last left off.
3. **The fallback.** After 100 attempts that fail, the 100th is dealt anyway (the classic rule: only the first open
   is safe). The checks say how often that happens.

## The game's solver

It knows only what a player sees: the opened cells' numbers. It marks cells as known mines (not the player's
flags) and opens cells it proves safe, by these rules, in this order, until none applies:

- **A, one cell at a time.** For each opened number in row order, with k known mines and U its closed, unmarked
  neighbours, and U not empty: if k equals the number, open every cell of U (with the flood); if k + |U| equals it,
  mark every cell of U. The sweep runs in row order on the state as it changes, and repeats while it changes anything.
- **B, two cells.** When a sweep of A changes nothing: for the opened numbers a, then b, in row order, with U_a a
  strict subset of U_b: the cells of U_b not in U_a hold (n_b − k_b) − (n_a − k_a) mines. If that is 0, open them
  all; if it is their count, mark them all. The first pair that opens or marks anything ends B, and A runs again.
- **C, the total.** When A and B change nothing: if every one of the 48 mines is marked, open every closed cell
  left; if the closed, unmarked cells left number exactly the unmarked mines, mark them all. Then A runs again.

These rules only ever draw true conclusions, so a board it clears is one a player can clear without a guess.

## The screen (400 × 240, draw.h only)

- The top line, 20 px: the mines left (48 minus the flags), the time in seconds, and BEST.
- The field: cells of 16 × 16 px from (8, 24). A closed cell is a raised square (an outline on a half shade); an
  opened one is light, with its number in the 5 × 7 font at scale 2 (nothing for 0); a flag is a small pennant on a
  closed cell; a mine is a dark dot. The cursor is a 2 px frame around its cell.
- A loss shows every mine, the one opened inverted, and a cross on each wrong flag; BOOM, the time and the best. A
  win: CLEAR, the time, and the best, with NEW BEST when it is. ENTER (after 0.5 s) plays again.
- The title: MINES, the keys, and the best once there is one.
- Sound: a tick for each open (440 Hz, 10 ms), a falling tune at a loss, a rising one at a win.

## The clock and the keys

The time starts at the first open and stops at the end; it shows whole seconds, and the best is kept in ms. A
cursor key moves the cursor at once on its down; held 300 ms, it moves again every 80 ms until its up. The cursor
stops at the field's edges. A game starts on ENTER, with the cursor at (12, 6).

## What it saves

16 bytes: 'M', version 1, the best winning time in ms (u32, 0 when there is none), wins (u32), games started (u32),
little-endian, then zeros. Taken whole or not at all: a save that is short, has the wrong magic or version, or more
wins than games, is not taken.

## Where it lives

`games/mines/mines_logic.{h,c}`: the field, opening, the chord, flags, dealing and the game's solver, with no draw
or framework calls. `games/mines/mines_app.c`: the app_def, the cursor, the clock, the screens and the sound.
`games/mines/stu32-game.txt`: name MINES, folder GAMES, MIT. `tests/mines_trace.c` prints what the oracle prints.

## The oracle (written before the game)

`oracle/mines/`, Python, from this file alone:

1. **The model** (`model.py`): the rules, the dealing and the game's solver above. The C must print the same field,
   the same attempts and the same state after every open, flag and chord for fuzzed scripts over many seeds, every
   state line compared.
2. **No board needs a guess** (`proof.py`), proven by a complete solver, not the game's: it splits the closed
   cells next to opened numbers into connected groups, enumerates every mine placement in each group that fits the
   numbers, and combines them with the cells beyond the frontier and the mine total, so a cell is safe exactly when
   no consistent placement has a mine there. Opening only proven-safe cells, it must clear every board the C deals
   for a range of seeds. The control: a board with the classic 50/50 corner, which it must report as needing a
   guess. A board the game's solver gives up on and the complete one clears is fine; the reverse would be a bug, and
   is checked. The attempts a board takes are counted, and the worst seed found is timed on the S3.
3. **The rules at their edges**, by hand: the chord with the right flags, too few, too many, and a wrong one (a
   loss); a flagged cell opened (nothing); a flag on an opened cell (nothing); a flood stopping at numbers and at
   flags; the first open's neighbours never mines; a win with mines left unflagged.
4. **The app on the SDK runner**: scripted games with frame hashes committed (checked by eye), the save and every
   tone and tune the rules give: the cursor's repeat and its edges, a loss, a win and its time against the best,
   ENTER partway through a title tick, damaged saves refused; the same through the .wasm and the bridge.
5. **Plants**, each of which must go red, and a plant that does not build is red too.
