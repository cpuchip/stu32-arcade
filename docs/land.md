# Land: set the ship down on a pad (blueprint)

Michael's decision 71 (abacus aa30714): after Aim, Lander, then Snake and Mines, each a unit through soroban's gate.
An APPS name has at most 5 characters, so LANDER does not fit; LAND is the recommended name, on Michael's board
(abacus #6193, lander-name) and used here until he says otherwise. Landing a craft under gravity is an old game
form; "Lunar Lander" is Atari's title, so nothing here uses it. The rules, the numbers below and everything on the
screen are ours. This is Land's design and the checks that will judge it, written before any of its code.

## A round

The ship starts near the top of one screen of terrain with some sideways speed. The player turns it with 4 and 6
and fires its engine with 8 to set it down on one of three flat pads, upright and slowly. A landing scores 50 times
the pad's multiplier: the narrowest pad is ×5, the middle ×3, the widest ×2. Then a new terrain is drawn and the
ship starts again from the top.

One fuel tank lasts the whole game: 2,000 units, one burned for each 20 ms step the engine fires. A crash costs 200
more (to no less than 0). The game ends when a round ends with the tank empty. A ship with no fuel still flies, but can only fall.

| key | does |
|---|---|
| 4 | turns the nose 15° to the left, and again every 80 ms while held |
| 6 | the same to the right |
| 8 | fires the engine while held |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

## The physics (all integers)

Every number is an integer, so the host, the S3 and the .wasm compute the same frames. Floats would not: GCC's
default fp-contract lets the Xtensa build fuse a multiply and an add, and a value one bit off moves a pixel
(approved by abacus as an engineering call, #6193).

- **Units.** Positions are in 1/65,536 px (`Q` = 65,536) and speeds in Q per step. Screen x runs 0-399 to the right
  and y 0-239 downward.
- **The step.** The game advances in fixed 20 ms steps. A tick adds its ms, at most 100, to an accumulator, and one
  step runs for each whole 20 ms in it.
- **Angle.** `a` is 0-23 in 15° steps: 0 is upright, and it increases clockwise on the screen. `SIN[a]` and `COS[a]`
  are round(16,384 × sin(a × 15°)) and the same for cos.
- **Input per step.** A key's down is kept as a press until a step uses it, so a tap shorter than a step still
  counts. 8 fires the engine on a step if it is held or pressed, and the step clears its press. For turning, the
  turn timer first counts down by 1 if it is above 0; then, if it is 0, left = 4 held or pressed, right = the same for
  6, and when right − left is not 0 the ship turns by it (`a` mod 24, 4 turning toward 23), the timer becomes 4,
  and both turn presses clear. Held, a turn repeats every 4 steps (80 ms). Both keys together cancel and clear.
- **One step, in this order:**
  1. turn, as above;
  2. if the engine fires and the fuel is above 0: `vx += A × SIN[a] / 16,384` and `vy −= A × COS[a] / 16,384`
     (C's `/`, rounding toward 0), and the fuel drops by 1;
  3. `vy += G`;
  4. `vx` and `vy` are clamped to ±4 px a step (±262,144);
  5. `x += vx`, `y += vy`; `x` wraps into 0..400 px (the screen's left and right edges meet), and `y` stops at
     −200 px, where `vy` becomes 0;
  6. the contact test below.
- **The constants.** `G` = 655 (0.01 px a step², 25 px/s²); `A` = 1,500, about 2.3 G; the landing limits
  `VY` = 26,214 (0.4 px a step, 20 px/s, downward) and `VX` = 13,107 (0.2 px a step, 10 px/s, either way).

## The ship and the contact test

The ship's outline is seven points around its reference point, in px, with y downward: the feet (−6, 6) and (6, 6),
then (6, 1), (4, −5), (0, −8), (−4, −5), (−6, 1), a convex outline in that order. Turned to angle `a`, a point
(hx, hy) sits at `x + (hx × COS[a] − hy × SIN[a]) × 4` and `y + (hx × SIN[a] + hy × COS[a]) × 4`, in Q
(Q / 16,384 is exactly 4, so the turn is exact).

The ground is the terrain's polyline: at x, between vertices (x0, y0) and (x1, y1),
`y0 × Q + (y1 − y0) × (x − x0 × Q) / (x1 − x0)`, with C's `/`, and x taken mod 400 px.

The ship is in contact when one of its points is at or below the ground (y ≥ ground(x)), or when a terrain vertex
lies strictly inside its outline: on the inner side of all seven edges, none on an edge (cross products in 64
bits; a vertex is tested at its x and at x ± 400 px, for the wrap). The second test catches a peak narrower than the
ship poking between two of its points.

At contact the round ends. It is a **landing** when all of these hold, and a **crash** otherwise:
- the ship is upright (`a` = 0);
- |vy| ≤ VY and |vx| ≤ VX;
- both feet are over the same pad: each foot's x is within the pad's ends, inclusive;
- no terrain vertex is inside the outline.

A landing sets the ship on the pad (`y` = pad y − 6 px).

## Making a terrain

From a round's seed (below), with the draws `x = mix64(x)`, value `x % n` (Aim's mix64; the state starts at
`mix64(seed ^ mix64(round))`):

1. **The pads' order.** The pads are (24 px, ×5), (34 px, ×3), (48 px, ×2). They are shuffled: for i = 2, then 1,
   j = draw(i + 1), swap pad i with pad j.
2. **Their places.** The screen is three thirds of 133 px. The k-th pad of the shuffled order, w px wide, starts at
   `x0 = 133 k + 12 + draw(133 − 24 − w)` and has y = `130 + draw(80)`.
3. **The ground between.** It starts with (0, `150 + draw(70)`). Then, walking right toward the next pad's start
   (or to 400 after the last pad): the next x is the current x plus `12 + draw(17)`, but becomes the pad's start
   (or 400) if it would be past the start less 12 px; the next y is the current y plus `draw(61) − 30`, clamped to
   130-226, except that a vertex at a pad's start takes the pad's y. A pad adds the vertex at its end, at the same
   y, and the walk continues from there.

At most 48 vertices. **The start**, drawn after the terrain in this order: x = `40 + draw(320)` px; the speed
`19,661 + draw(19,661)` (0.3-0.6 px a step); its sign, leftward when `draw(2)` is 0. y = 28 px, vy = 0, a = 0.

**The round's seed:** round n is `seed = (uint64)app_seed() << 32`, `round = n`, where n counts the landings and
crashes of every game played and is kept in the save, so each round differs and a test with `-seed` repeats.

## The screen (400 × 240, draw.h only)

- The top line: SCORE, FUEL, and the two speeds in px/s (H across and V down, signed).
- The terrain as lines, each pad 3 px thick, and its multiplier (×5, ×3, ×2) under it.
- The ship as its outline, with a flame behind the engine on every other step that it fires. Near an edge it is
  drawn twice, wrapped. Above the top, a mark at the top edge shows where it is.
- At the end of a round: LANDED with the points, or CRASHED, for 1.5 s, then the next round. After the last
  round: GAME OVER, the score and the best; ENTER (after 0.5 s) plays again.
- The title: LAND, the keys, and the best score.
- Sound: a short tune on landing, a falling one on a crash, and one beep when the fuel drops under 200.

## What it saves

16 bytes: 'L', version 1, the best score (u32), landings (u32), crashes (u32), then zeros. Taken whole or not at all.

## Where it lives

`games/land/land_logic.{h,c}`: the physics, the contact test, the judge and the terrain generator, with no draw or
framework calls. `games/land/land_app.c`: the app_def, the screens and the sound. `games/land/stu32-game.txt`: name
LAND, folder GAMES, MIT. `tests/land_trace.c` prints what the oracle prints.

## The oracle (written before the game)

`oracle/land/`, Python, all integers, from this file alone:

1. **The model** (`model.py`): the physics, the contact test, the judge and the generator. The C must print the same
   state at every step for fuzzed key scripts over many seeds, and the same terrains.
2. **Every pad is landable** (`fair.py`): a reference autopilot, flown in the model, lands on every pad of every
   terrain for a range of seeds within 700 fuel (the first of four settings that lands it: clearance over the path,
   how far out the approach starts, how high it holds while centring). Its keys, replayed through the C, must land on the same step. The
   control: given too little fuel (say 100), it must fail, and the check must report the failure.
3. **The landing rule at its edges**: |vy| = VY lands and VY + 1 crashes; the same for VX; `a` = 1 and 23 crash; a
   foot exactly at a pad's end lands and one pixel past it crashes; a vertex poking into the outline crashes even
   with every point above the ground. In both the C and the model.
4. **The app on the SDK runner**: scripted games (the autopilot's keys on the app's clock) with frame hashes
   committed, and the expected save; and the same scripts through the .wasm and the bridge.
5. **Plants**, each of which must go red: gravity left out, the judge ignoring the angle, the vertex test left out,
   a pad that is not flat, and no wrap at the edges.
