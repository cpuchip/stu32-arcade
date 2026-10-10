# Mines: clear the field, with no guess ever needed (blueprint)

Michael's decision 71 (abacus aa30714): after Aim, Lander and Snake, Mines, the last of the four, a unit through
soroban's gate. MINES has five letters, so it is its own APPS name; "Minesweeper" is Microsoft's title, so nothing
here uses it. Every board can be cleared by reasoning alone, never a guess (on Michael's board as mines-rules,
abacus #6246, with the classic rule as the fallback). This is Mines' design and the checks that will judge it,
written before any of its code.

**Revision 2, settings** (Michael's decisions 76 and 77, abacus b12e275 and c20db07): a settings page with three
board sizes (easy 9 × 9 with 10 mines, medium 16 × 16 with 40, hard 30 × 16 with 99), classic rules as a choice, and
the game's sound on or off. There is no time limit; the clock stays as it was. Revision 1 (unit 064, stu32-arcade
1def492) had one board, 24 × 13 with 48 mines; it is replaced by the three. The hard board's no-guess deal can take
up to 2 s on the S3, so dealing moves out of the key and runs across ticks. What changed from revision 1 is marked
**(rev 2)**; the rest is as unit 064 built it.

## A game

A field of W × H cells hides N mines (the board's, below). The player moves a cursor with 8, 2, 4 and 6 and opens a
cell with 5 (or ENTER). An opened cell shows how many of its up to 8 neighbours are mines; a 0 opens its neighbours
by itself. 0 puts a flag on a closed cell, or takes it off. Opening an opened number whose flags match it opens its
other closed neighbours. Opening a mine loses; opening every cell that is not a mine wins. The time counts from the
first open, and the best winning time is kept for each board and rule.

The first cell opened is never a mine, nor are its neighbours, so the first open always clears an area. With the
game's own rules (no guess, the default), every board is one a player can finish by reasoning: the game only deals
boards its own solver can clear from that first open without a guess. With classic rules **(rev 2)** the board is
the first one drawn, and some will need a guess. The game's solver cannot clear about 19% of easy boards, 46% of
medium and 96% of hard (20,000 seeds each); it is not complete, so the share that truly needs a guess is lower.

| key | does |
|---|---|
| 8 2 4 6 | the cursor up, down, left, right; held, it repeats |
| 5, ENTER | open the cell under the cursor (on an opened number: open its neighbours, as above) |
| 0 | in a game: a flag on the closed cell under the cursor, or off. On the title and after the end: the settings **(rev 2)** |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

## The boards (rev 2)

| board | W × H | N | cell | field at | numbers |
|---|---|---|---|---|---|
| EASY | 9 × 9 | 10 | 22 px | (101, 30) | 5 × 7 font at scale 2 |
| MEDIUM (the default) | 16 × 16 | 40 | 13 px | (96, 22) | scale 1 |
| HARD | 30 × 16 | 99 | 13 px | (5, 22) | scale 1 |

Each field is centred across the screen's 400 px. The cursor starts at (W / 2, H / 2), integer division: (4, 4),
(8, 8) and (15, 8).

## The rules

- **The field.** Cells (x, y), x 0 to W − 1, y 0 to H − 1, numbered in row order, `c = y × W + x`; W × H in all. A
  cell's neighbours are the up to 8 around it, in the order (−1,−1), (0,−1), (1,−1), (−1,0), (1,0), (−1,1), (0,1),
  (1,1).
- **Opening** a closed, unflagged cell: a mine loses. Otherwise it opens, and if it shows 0 its closed, unflagged
  neighbours open in turn, and so on (a flood; a flagged cell is never opened by it). Opening a flagged cell, or an
  opened cell that is not a number, does nothing.
- **The chord.** Opening an opened cell showing n (0 included) with exactly n flags among its neighbours opens each
  of its closed, unflagged neighbours in neighbour order, as above; a mine among them loses (the rest are still
  opened first, so the loss shows them), and the first such mine in neighbour order is the one shown as opened. With
  any other count of flags, nothing. (An opened 0 can have closed neighbours when a flag stopped a flood and was
  then taken off.)
- **Flags** go on and off closed cells only, without limit; the count shown is N minus the flags, and can go below
  0.
- **The end.** A loss shows every mine. A win, when every cell that is not a mine is open, flags every mine.

## Dealing a board

On the first open of a game, at cell f. The state is `x = mix64(seed ^ mix64(game))` (Aim's mix64), and each draw
is `x = mix64(x)`, value `x % n`. The game's seed is `(uint64)app_seed() << 32`; `game` counts the games ever
started on any board, the sum of the six records' games in the save **(rev 2)**.

1. **An attempt.** The allowed cells are every cell but f and its neighbours, in row order (a list of A cells),
   built afresh for each attempt **(rev 2: revision 1 copied a kept list; the list is the same, and the copy cost
   960 B of stack at 30 × 16)**. Then for i = 0 to N − 1: `j = i + draw(A − i)`, swap the list's i-th and j-th
   cells. The first N are the mines.
2. **The test.** With classic rules, the first attempt is dealt **(rev 2)**. Otherwise the game's solver (below)
   plays the board from opening f; if it opens every cell that is not a mine, the attempt is dealt, and if not, the
   next attempt draws from where the last left off.
3. **The fallback.** After 400 attempts that fail **(rev 2: 100 before)**, the 400th is dealt anyway (the classic
   rule: only the first open is safe). Measured over 20,000 seeds per board, first open at the centre: the most
   attempts any board needed were 7 (easy), 16 (medium) and 213 (hard), so the fallback never came.
4. **Across ticks (rev 2).** The first open does not deal inside its key. The key starts the deal (the game shows
   DEALING), and each tick then makes up to D attempts, D = 4, until one is dealt; that tick opens f and starts the
   clock. Keys during the deal do nothing (C is the framework's and still ends the game). On the S3 one attempt at
   30 × 16 takes 9.6 ms on average, so a tick of 4 stays near 40 ms; the hard board's worst deal measured (213
   attempts, 1.95 s of attempts) takes 54 ticks. D is checked on the S3 before the unit ships: the bench reports the
   longest tick.

## The game's solver

It knows only what a player sees: the opened cells' numbers. It marks cells as known mines (not the player's
flags) and opens cells it proves safe, by these rules, in this order, until none applies:

- **A, one cell at a time.** For each opened number in row order, with k known mines and U its closed, unmarked
  neighbours, and U not empty: if k equals the number, open every cell of U (with the flood); if k + |U| equals it,
  mark every cell of U. The sweep runs in row order on the state as it changes, and repeats while it changes anything.
- **B, two cells.** When a sweep of A changes nothing: for the opened numbers a, then b, in row order, with U_a a
  strict subset of U_b: the cells of U_b not in U_a hold (n_b − k_b) − (n_a − k_a) mines. If that is 0, open them
  all; if it is their count, mark them all. The first pair that opens or marks anything ends B, and A runs again.
- **C, the total.** When A and B change nothing: if every one of the N mines is marked, open every closed cell
  left, and A runs again. (Its mirror, marking every closed cell when they number the mines left, can never change
  the result: by then every safe cell is open.)

These rules only ever draw true conclusions, so a board it clears is one a player can clear without a guess.

## The settings (rev 2)

The title shows MINES, the keys, the board and rules chosen (for example `MEDIUM 16 X 16, 40 MINES, NO GUESS`), the
best for that board and rule once there is one, `0 SETTINGS` and `ENTER PLAYS`. 0 on the title, or after the end
(once ENTER would play again), opens the settings page:

| row | values | shown as |
|---|---|---|
| BOARD | EASY, MEDIUM, HARD | the name, its size and its mines |
| CLASSIC RULES | off (the default), on | a box, crossed when on |
| SOUND EFFECTS | on (the default), off | a box, crossed when on |

8 and 2 move between the rows (stopping at the ends); 4 and 6 step BOARD down and up (stopping at the ends), and 5
also steps it up, wrapping from HARD to EASY; 4, 6 and 5 turn a box on or off. ENTER goes back to the title. A
change takes effect from the next game and is kept in the save.

With sound effects off, the game makes no tone or tune of its own. The calculator's VOL and BEEP settings still
apply to everything, and its key click is the framework's, so it is not affected by this setting.

## The screen (400 × 240, draw.h only)

- The top line, 20 px: the mines left (N minus the flags), the time in seconds, and BEST (this board and rule's).
  During the deal, DEALING in place of the mines left **(rev 2)**.
- The field: cells of the board's size from its origin (the table above). A closed cell is a raised square (an
  outline on a half shade); an opened one is light, with its number centred (nothing for 0); a flag is a small
  pennant on a closed cell; a mine is a dark dot. The cursor is a 2 px frame around its cell. The pennant, the dot and
  the cross scale with the cell **(rev 2)**; revision 1's 16 px drawing is the 22 px one's model, and the frames are
  checked by eye at both sizes.
- A loss shows every mine, the one opened inverted, and a cross on each wrong flag; the top line then reads BOOM,
  the time and the best. A win: CLEAR, the time, and the best, with NEW BEST when it is. Nothing covers the field.
  ENTER at least 0.5 s after the end (by the keys' own times) plays again, and ENTER PLAYS shows under the field
  (with `0 SETTINGS` beside it, **rev 2**).
- Sound (when on): a tick for each open (440 Hz, 10 ms), a falling tune at a loss, a rising one at a win.

## The clock and the keys

The time starts on the tick that deals the board **(rev 2: the first open's key, in revision 1)**, so the deal is
never on the clock, and stops at the end's key; it shows whole seconds (0 before the deal), and the best is kept in
ms: a win sets it when it has none or is faster, not when it ties. A cursor key moves the cursor at once on its
down; held 300 ms, it moves again every 80 ms until its up. A second cursor key pressed while one is held takes over,
and the first one's up then does nothing. The cursor stops at the field's edges. A game starts on ENTER, with the
cursor at the board's centre.

## What it saves (rev 2)

80 bytes, little-endian: 'M', version 2, the settings byte, then six records of 12 bytes, then 5 zeros. The
settings byte: bits 0-1 the board (0 EASY, 1 MEDIUM, 2 HARD), bit 2 classic rules on, bit 3 sound effects off, the
rest 0. The records, in the order EASY no guess, EASY classic, MEDIUM no guess, MEDIUM classic, HARD no guess, HARD
classic: the best winning time in ms (u32, 0 when there is none), wins (u32), games started (u32).

Taken whole or not at all: a save that is short, has the wrong magic or version, a board of 3, a settings bit above
bit 3, or a record with more wins than games, is not taken, and the game starts with the defaults (MEDIUM, no
guess, sound on) and no records. A revision 1 save (version 1, the 24 × 13 board's best) is not taken: its best is
for a board that no longer exists.

## Where it lives

`games/mines/mines_logic.{h,c}`: the field, opening, the chord, flags, dealing and the game's solver, with no draw
or framework calls; the board's W, H and N are the game's, set by `mines_init`, and its arrays are sized for the
largest board (480 cells) **(rev 2)**. Dealing is `mines_deal_start` (the first open) and `mines_deal_step` (up to D
attempts; true when dealt) **(rev 2)**. `games/mines/mines_app.c`: the app_def, the cursor, the clock, the settings,
the screens and the sound. `games/mines/stu32-game.txt`: name MINES, folder GAMES, MIT. `tests/mines_trace.c`
prints what the oracle prints.

The ctx grows from 2,272 B to about 3.5 KB (seven bytes a cell at 480 cells), all in PSRAM; no internal RAM.

## The oracle (written before the game)

`oracle/mines/`, Python, from this file alone:

1. **The model** (`model.py`): the rules, the dealing and the game's solver above, for each board **(rev 2)**. The C
   must print the same field, the same attempts and the same state after every open, flag and chord for fuzzed
   scripts over many seeds on each board and both rules, every state line compared.
2. **No board needs a guess** (`proof.py`), proven by a complete solver, not the game's: it splits the closed
   cells next to opened numbers into connected groups, enumerates every mine placement in each group that fits the
   numbers, and combines them with the cells beyond the frontier and the mine total, so a cell is safe exactly when
   no consistent placement has a mine there. Opening only proven-safe cells, it must clear every board the C deals
   with the game's rules, for a range of seeds on each board **(rev 2)**. The control: a board with the classic 50/50
   corner, which it must report as needing a guess. A board the game's solver gives up on and the complete one
   clears is fine; the reverse would be a bug, and is checked. The attempts a board takes are counted, and the worst
   seed found on each board is timed on the S3. With classic rules, every deal is the first attempt **(rev 2)**.
3. **The rules at their edges**, by hand: the chord with the right flags, too few, too many, and a wrong one (a
   loss); a flagged cell opened (nothing); a flag on an opened cell (nothing); a flood stopping at numbers and at
   flags; the first open's neighbours never mines; a win with mines left unflagged; the field's corners and edges on
   each board **(rev 2)**.
4. **The app on the SDK runner**: scripted games with frame hashes committed (checked by eye), the frames drawn on
   keys included (keys come 7 ms after a tick, as on a calculator), the save and every tone and tune the rules
   give: the cursor's repeat, its edges and a second key, a loss, a win and its time against the best (none, a tie,
   a slower one), ENTER partway through a title tick and at exactly 0.5 s, damaged saves refused (each by its own
   rule, a version 1 save among them); the same through the .wasm and the bridge. **Rev 2:** a game on each board;
   the settings page (every row, its ends and the wrap, ENTER back, 0 from the end); a deal across many ticks (a
   hard seed of over 100 attempts) with keys pressed during it that do nothing and the clock starting on the
   dealing tick; classic rules dealing the first attempt; sound off giving no tones at all; the settings and the
   six records kept across a save and a load.
5. **Plants**, each of which must go red, and a plant that does not build, or matches nothing, or runs forever, is
   red too.
