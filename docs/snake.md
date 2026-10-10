# Snake: eat, grow, and keep off the walls and yourself (blueprint)

Michael's decision 71 (abacus aa30714): after Aim and Lander, Snake, then Mines, each a unit through soroban's gate.
SNAKE has five letters, so it is its own APPS name. A snake that grows as it eats is an old game form with a
generic name; the rules, the numbers below and everything on the screen are ours. This is Snake's design and the
checks that will judge it, written before any of its code.

## A game

The snake moves one cell at a time on a grid of 40 × 22 cells. The player turns it with 8, 2, 4 and 6 (up, down,
left, right, as they sit on the keypad). Food sits on one free cell; eating it makes the snake one cell longer and
puts new food on another free cell. The snake dies when its head leaves the grid or runs into its own body. It
speeds up as it eats. The score is the food eaten. A snake that fills the whole grid has won.

| key | does |
|---|---|
| 8 2 4 6 | turn up, down, left, right |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

## The rules (all integers)

- **The grid.** Cells (x, y), x 0-39 to the right and y 0-21 downward; 880 in all.
- **The start.** Length 4, the body (7, 10), (8, 10), (9, 10) and the head (10, 10), moving right. Food is placed as
  below.
- **The clock.** The snake steps every `max(60, 150 − 5 × eaten)` ms. A tick adds its ms, at most 100, to an
  accumulator; while it holds a step's interval, the snake steps and the interval is taken off, at most 4 steps a
  tick. A game starts on a key (ENTER), and its first tick counts only the time since that key.
- **Turns.** A direction key's down is offered to a queue of at most 2 turns. It is taken when the queue has room
  and the direction is neither the last queued one (or the current one, when the queue is empty) nor its reverse;
  otherwise it is dropped. Each step first takes the queue's oldest turn, if any, as the direction.
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

- The top line, 20 px: SCORE, the length, and BEST. A rule under it.
- The grid below: each cell 10 × 10 px from (0, 20); the body as 8 × 8 squares, the head as an 8 × 8 outline with
  a dot, the food as a hollow diamond.
- At death: GAME OVER with the score and the best; at the win: FULL, the same. ENTER (after 0.5 s) plays again.
- The title: SNAKE, the keys, and the best once there is one.
- Sound: a short beep for each food, a falling tune at death, a rising one at the win.

## What it saves

16 bytes: 'S', version 1, the best score (u32), the games started (u32), little-endian, then zeros. Taken whole or
not at all. The best updates at each food, so leaving with C keeps it.

## Where it lives

`games/snake/snake_logic.{h,c}`: the grid, the queue, the step and the food, with no draw or framework calls.
`games/snake/snake_app.c`: the app_def, the screens and the sound. `games/snake/stu32-game.txt`: name SNAKE, folder
GAMES, MIT. `tests/snake_trace.c` prints what the oracle prints.

## The oracle (written before the game)

`oracle/snake/`, Python, from this file alone:

1. **The model** (`model.py`): the rules above. The C must print the same state after every key and step for fuzzed
   key scripts over many seeds.
2. **A full game** (`cycle.py`): an autopilot that follows a Hamiltonian cycle of the grid (row 0 left to right,
   rows 1-21 back and forth over columns 1-39, then up column 0) cannot hit itself, so it eats every food until the
   grid is full. In the model and the C, for several seeds, the game must end in a win at length 880, with every
   food placed on a free cell, and the two traces identical. This reaches the longest snake, the ring buffer's every
   wrap, and food placement with one free cell left.
3. **The rules at their edges**, by hand: the head into the tail's cell (safe), into a body cell (death), into each
   wall; a reverse key dropped; a repeat of the direction dropped; two turns inside one step taken on two steps; a
   third dropped; the interval at 0, 1, 17, 18 and 19 foods (150, 145, 65, 60 and 60 ms).
4. **The app on the SDK runner**: scripted games with frame hashes committed (and checked by eye, since they come
   from the C), and the save, tunes and tones that the rules give; the same through the .wasm and the bridge.
5. **Plants**, each of which must go red: the tail freed after the collision test, a reverse key taken, food
   counted over all cells instead of the free ones, a queue of 1, a missing wall, and the interval never shortening.
