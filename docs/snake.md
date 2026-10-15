# Snake: eat, grow, and keep off the walls and yourself (blueprint)

Michael's decision 71 (abacus aa30714): after Aim and Lander, Snake, then Mines, each a unit through soroban's gate.
SNAKE has five letters, so it is its own APPS name. A snake that grows as it eats is an old game form with a
generic name; the rules, the numbers below and everything on the screen are ours. This is Snake's design and the
checks that will judge it, written before any of its code.

## A game

The snake moves one cell at a time on a grid of 40 × 22 cells. The player turns it in one of two modes, chosen on the
title: TURN, the default, where 4 or SOFT1 turns it a quarter turn left and 6 or SOFT6 a quarter turn right of where it
is heading; or 8 2 4 6, where 8, 2, 4 and 6 send it up, down, left and right, as they sit on the keypad (Michael's asks,
abacus #8558, and his ruling #8610: TURN the default, one best score for both). Food sits on one free cell; eating it
makes the snake one cell longer and puts new food on another free cell. The snake dies when its head leaves the grid or
runs into its own body. It speeds up as it eats. The score is the food eaten. A snake that fills the whole grid has won.

| key | does |
|---|---|
| 4 or SOFT1, 6 or SOFT6 | in TURN: a quarter turn left, right of the heading |
| 8 2 4 6 | in 8 2 4 6: turn up, down, left, right |
| 4 or 6 on the title | change the mode |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

## The rules (all integers)

- **The grid.** Cells (x, y), x 0-39 to the right and y 0-21 downward; 880 in all.
- **The start.** Length 4, the body (7, 10), (8, 10), (9, 10) and the head (10, 10), moving right. Food is placed as
  below.
- **The clock.** The snake steps every `max(60, 150 − 5 × eaten)` ms. A tick adds its ms, at most 100, to an
  accumulator; while it holds a step's interval, the snake steps and the interval is taken off (with the cap and
  the 60 ms floor, a tick gives at most 2 steps). A game starts on a key (ENTER), and its first tick counts only the
  time since that key.
- **Turns.** A direction key's down is offered to a queue of at most 2 turns. It is taken when the queue has room
  and the direction is neither the last queued one (or the current one, when the queue is empty) nor its reverse;
  otherwise it is dropped. Each step first takes the queue's oldest turn, if any, as the direction.
- **A turn in TURN.** 4's or SOFT1's down offers the direction a quarter turn left (counterclockwise) of the last
  queued direction, or of the current one when the queue is empty: `(last + 3) mod 4` with 0 up, 1 right, 2 down,
  3 left. 6's or SOFT6's offers a quarter turn right, `(last + 1) mod 4`. It is offered as above, so it is taken
  whenever the queue has room (a quarter turn is never the same direction nor its reverse), and two quick lefts make
  a U-turn over two steps. In TURN, 8 and 2 do nothing; in 8 2 4 6, the soft keys do nothing.
- **One step**, in this order:
  1. the direction, from the queue as above;
  2. the next cell is the head plus the direction; outside the grid, the snake dies (and nothing moves);
  3. if the next cell holds the food, the snake grows: the head moves there, the tail stays, `eaten` goes up by 1,
     and new food is placed; if there is no free cell for it, the snake has won;
  4. otherwise the tail cell is freed first, and then, if the next cell is part of the body, the snake dies (and
     nothing moves, the tail included); else the head moves there.

  So moving into the cell the tail is leaving is safe. Food is never on the snake, so eating never meets the tail.
- **Food.** The state is `x = mix64(seed ^ mix64(game))` (Aim's mix64); each placement is `x = mix64(x)`, then
  `k = x % free`, where `free` = 880 − length, and the food goes on the k-th free cell counted from 0 in row order
  (y, then x). The game's seed is `(uint64)app_seed() << 32`; `game` counts the games ever started, kept in the
  save, so each game differs and a test with `-seed` repeats.

## The screen (400 × 240, draw.h only)

- The top line, 20 px: SCORE, the length, and BEST.
- The grid below, framed by a line (its top edge under the top line): each cell 10 × 10 px from (0, 20); the body
  as 8 × 8 squares, the head as an 8 × 8 outline with a dot, the food as a hollow diamond 7 px across.
- At death: GAME OVER with the score and the best; at the win: FULL, the same. ENTER at least 0.5 s after the end
  (by the key's own time) plays again; ENTER during play does nothing.
- The title: SNAKE, the mode and its keys, how to change it (4 or 6 there), and the best once there is one.
- Sound: a short beep for each food (660 Hz, 30 ms), but the food that fills the grid plays the win's tune instead;
  a falling tune at death (392, 330, 262 Hz), a rising one at the win (523, 659, 784, 1047 Hz).

## What it saves

16 bytes: 'S', version 1, the best score (u32), the games started (u32), little-endian, the mode (0 TURN, 1 8 2 4 6),
then zeros. Taken whole or not at all: a save that is short, has the wrong magic or version, a best over 876 (more
than a game can score) or a mode over 1 is not taken. A save from before the modes has a 0 there, so it loads as
TURN, the default, keeping its best; the version stays 1. The mode is kept when it is changed on the title. One best
covers both modes. The best updates at each food, so leaving with C keeps it.

## Where it lives

`games/snake/snake_logic.{h,c}`: the grid, the queue, the step and the food, with no draw or framework calls.
`games/snake/snake_app.c`: the app_def, the screens and the sound. `games/snake/stu32-game.txt`: name SNAKE, folder
GAMES, MIT. `tests/snake_trace.c` prints what the oracle prints.

## The oracle (written before the game)

`oracle/snake/`, Python, from this file alone:

1. **The model** (`model.py`): the rules above. The C must print the same state after every key and step for fuzzed
   key scripts over many seeds, and for grown games (the cycle below for 5-60 foods, then random keys), whose long
   snakes die into their own bodies.
2. **A full game** (`cycle.py`): an autopilot that follows a Hamiltonian cycle of the grid (row 0 left to right,
   rows 1-21 back and forth over columns 1-39, then up column 0) cannot hit itself, so it eats every food until the
   grid is full. In the model and the C, for several seeds, the game must end in a win at length 880, with every
   food placed on a free cell, and the two traces identical. This reaches the longest snake, the ring buffer's every
   wrap, and food placement with one free cell left.
3. **The rules at their edges**, by hand, and every state line of the C against the model's: the head into the tail's
   cell (safe), into a body cell (death), into each wall; a reverse key dropped; a repeat of the direction dropped; two
   turns inside one step taken on two steps; a third dropped; the interval at 0, 1, 17, 18 and 19 foods (150, 145, 65,
   60 and 60 ms).
4. **The app on the SDK runner**: scripted games with frame hashes committed (and checked by eye, since they come
   from the C), and the save and every tone and tune (pitch, length and time, as the runner's -v prints them) that
   the rules give: ENTER partway through a title tick, ENTER during play, ticks of 100 and 250 ms (the cap), a game
   that fills the grid in the app, a game left alive (the best kept at each food) with no save and with four damaged
   ones that must be refused. The same through the .wasm and the bridge.
5. **Plants**, each of which must go red: the tail freed after the collision test, a reverse key taken, food
   counted over all cells instead of the free ones, a queue of 1, a missing wall, the interval never shortening, a
   body death that moves the tail, no 100 ms cap, the tunes swapped, a save with the wrong magic taken, ENTER
   restarting a game in play, the best not kept at each food, and the food not drawn. A plant that does not build
   is red too.
