# Salvo: hold off the swarm (blueprint)

Michael's ruling on the games plan (abacus #8569): a top-down space shooter in the style of the fixed-screen arcade
shooters, where a formation of aliens hangs at the top and peels off in dives at the player's ship. SALVO is a
generic word (a burst of fire) and fits the APPS menu's five cells. The name waits for Michael's word; a web search
found indie games of that name and no live trademark covering games, which is not a register search. The rules, the
numbers and every sprite below are ours. This is Salvo's design and the checks that will judge it, written before
any of its code.

## A game

The ship sits at the bottom of the screen and moves left and right. A formation of 32 aliens slides in at the top,
sways from side to side, and sends aliens down in dives that fire at the ship and try to ram it. A diver that misses
flies off the bottom and drops back into its place from the top. Shooting every alien clears the wave, and the next
wave dives sooner and in larger numbers. The ship has three lives; the score is what the shots earn.

| key | does |
|---|---|
| 4 or SOFT1 | move left while held |
| 6 or SOFT6 | move right while held |
| 7, 8, 9, ×, GOLD, SOFT2, SOFT5 or ENTER | fire while held (a shot as often as the rules allow) |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

The keys are Michael's layout for the three action games (2026-10-23, via basecamp #9900, #9906, #9907; before it,
8, ENTER, 9, × and + fired). Either hand can move or fire: a thumb on SOFT1 and SOFT6 at the screen's corners with a
finger on the number row (7, 8, 9, ×, GOLD), or 4 and 6 with the thumbs on SOFT2 and SOFT5. + does nothing. Moving and
firing at once needs two keys held together, which Rev A's diode matrix allows; the rollover log on board 0005 is the
measurement (#8604; this line said board 0001 until 2026-10-23, which was wrong).

## The rules (all integers)

**The field.** x 0-399 to the right, y downward; the play field is y 12-239, under a 12 px top line. A thing is a
rectangle (x, y, w, h); two overlap when `x1 < x2 + w2 and x2 < x1 + w1 and y1 < y2 + h2 and y2 < y1 + h1`.

**The clock.** The game steps every 50 ms. A tick adds its ms, at most 100, to an accumulator; while it holds 50, 50 is
taken off and a step runs (so a tick gives at most two steps). Once the game is over, a tick adds nothing and runs
nothing. A game starts on ENTER, and its first tick counts
only the time since that key.

**The keys in play.** A key's down sets it held, its up clears it, and a down also latches its action for the next
step, so a tap shorter than a step still counts once. Left is any left key held or latched, right the same, fire
any fire key held or latched. The latches clear after each step.

**The draws.** Each game has a state `x = mix64(seed ^ mix64(game))` (SplitMix64's finalizer, the mix64 of Snake and
Land; `seed = app_seed()`, used as it is, `game` the count of games started before this one, 0 for
the first, kept in the save), and `draw(n)` is `x =
mix64(x)` then `x % n`. Every draw comes from this one state, in the order the rules below make them (a dive's column,
then its fire_at; a column is drawn whenever a dive may start, even if no alien is in a slot to take it), so the model,
the host and the device draw the same numbers from the same seed and save; nothing else in the game is random.

**The ship.** 15 × 8 at (sx, 228); a game starts with sx = 192. For hits it is the box (sx + 2, 230, 11, 6), its body
without the nose's first two rows and the wing tips, so a hit is never on blank pixels. In a step, left and not right
moves it 5 px left, right and not left 5 px right, clamped to 0-385.

**The shots.** At most 2 on the field, each 1 × 6. A cooldown counts down by 1 at the start of each step's shots (never
below 0), then: when the ship is alive, fire is on, the cooldown is 0 and fewer than 2 shots fly, a shot appears at (sx
+ 7, 222) and the cooldown becomes 6. A shot moves 12 px up each step; once its bottom (y + 6) is at or above 12 it
is gone, but only after that step's hits, so its last path is tested too (and it still counts toward the 2 when the
fire test comes). A new shot does not move on its first step, but it meets that step's hits where it appears. A shot's
box can reach into the top line; aliens there, entering or returning, can be hit.

**The formation.** 32 slots, row r 0-3 by column c 0-7, slot (r, c) at `(fx + 24c, fy + 16r)`; an alien is 13 × 10.
Row 0 is the flagship row, row 1 the escorts, rows 2 and 3 the drones. A wave starts with fx = 108, fy = −60, the
sway's direction rightward, the sway's count and the dive counter 0, and every alien in its slot. Shots already
flying are kept.
- **Entry.** While fy < 20, fy grows by 4 each step (the formation comes down) (20 steps). The step on which fy reaches
  20 or more ends the entry: the sway's count is set to 0 and the dive counter to the gap (both counted from the next
  step). Nothing dives or fires until then.
- **Sway.** After the entry, each step adds 1 to the sway's count, and when the count is a multiple of 4, fx moves 4 px
  in its direction, rightward at a wave's start; when that would take it below 8 or above 208, the direction turns first
  and the move is the other way. The aliens' wings flap on each sway move: the frame is (count / 4) mod 2, 0 at a wave's
  start.

**An alien** is in its slot, diving, returning or dead.
- **The dive's start.** The dive gap is `max(12, 40 − 4 × (wave − 1))` steps. A dive counter (set as the entry ends)
  counts down by 1 in each later step of the wave, after its entry, while the ship is alive; at 0 it is set to the gap
  again and, if fewer than `min(4, 1 + wave)` aliens are diving (returning ones do not count), one starts: c = draw(8),
  then the first of columns c, c + 1, … (mod 8) that has an alien in its slot, and in it the lowest such alien (the
  largest r). With none in any slot, nothing starts (the column was still drawn); an alien that came back to its slot
  earlier in the step may be the one. A diver also takes `fire_at = 6 + draw(10)`.
- **Diving.** From its slot's position after this step's sway; it first moves on the next step, its dive's step 0. Each
  step: on its dive's step 0 and every 4th step after, `vx = clamp((sx + 1 − x) / 12, −3, 3)` (sx after this step's
  move), the division truncated toward zero; then x += vx (clamped to 0-387) and y += 5. On its dive's step fire_at (its
  moves counted from 0), with y after the move, if y < 180 and fewer than 6 enemy shots fly, it fires one; a refused
  shot is not tried again. (From a slot the formation's lowest row is at y 68 and fire_at is at most 15, so a dive fires
  by y 148 in play, and a diver's x stays within 1-386: these limits guard hand-set states.) After its move, at y ≥ 240
  it is at once returning, at (its slot's x, −10), and makes its first return move on the next step.
- **Returning.** It reappears at its slot's x with y = −10. Each step x is its slot's x and y += 4; once y is at
  or below the slot's y (y ≥ slot y) it takes the slot's y and is back in its slot.

**Enemy shots.** 2 × 4, from (x + 6, y + 10) of the alien that fires; 6 px down each step, the one just fired
included (step 5 comes after step 4); gone at y ≥ 240.

**One step**, in this order:
1. the ship moves (if it is alive);
2. the shots move, then one may be fired;
3. the formation: the entry, or the sway;
4. the aliens: each returning one, then each diving one, by slot order; then the dive counter;
5. the enemy shots move;
6. the hits, below, then the shots gone at the top are removed;
7. the timers, below.

**The hits.**
1. Each shot, oldest first, against the aliens (in a slot, diving or returning): of those its box overlaps, the one
   lowest on the screen (the largest y; on a tie, the first in slot order) dies, and the shot is gone. That is the one
   it meets first. A shot that moved this step is tested over the whole path it moved, the box (x, y, 1, 18)
   from its new top to its old bottom, so it cannot pass through an alien coming the other way (a diver closes 17 px a
   step with it, a returning alien 16); a shot that appeared this step is its own 1 × 6. It scores 150, 80 or 50
   (flagship, escort, drone), doubled for a diving one (not a returning one). Extra ships are counted at each kill.
2. If the ship is alive, each enemy shot against its box, then each diving alien by slot order: the first that
   overlaps it hits the ship (a diver that hits it dies too, scoring nothing), and the rest are not tested.

**A lost ship.** Lives (the ships left, the one in play included; 3 at the start) go down by 1. Every shot and
enemy shot is gone, and every diving or returning alien is back in its slot. For 40 steps the ship shows its
explosion and nothing dives. Then, with lives left, the ship returns at sx = 192 and the dive counter is 20; with
none, the game is over.

**Extra ships.** Each time the score reaches the next multiple of 10,000, that multiple is passed and a ship is added,
unless lives are already 5 (that ship is not kept for later).

**The timers** (step 7), in this order:
- **The ship's.** A hit sets it to 40. In each later step it goes down by 1, and on reaching 0 the ship returns as
  above, or, with no lives left, the game is over and nothing steps again (the wave's timer is not
  looked at).
- **The wave's.** When no alien is left, the ship is in play and the wave's timer is not running, every shot and
  enemy shot is gone and the timer is set to 30. In each later step it goes down by 1, and on reaching 0 the next
  wave starts (wave + 1): fx 108, fy −60, the sway's direction rightward, every alien in its slot, its entry from
  the next step. Waves go on until the ships run out.

## The screen (400 × 240, draw.h only)

- The top line, 12 px: SCORE, the wave, and a small ship for each life held beyond the one in play.
- 48 fixed stars on the play field (from mix64 of a constant, the same every game), drawn first, so a star costs a
  row only where a sprite crosses it.
- Sprites in 1 bit: the ship 15 × 8, three alien kinds 13 × 10 with two wing frames each, a 13 × 10 burst shown for
  4 steps where an alien dies, and the ship's own burst, blinking (on for 2 steps, off for 2) through its 40 steps.
- Game over: GAME OVER with the score and the best. ENTER at least 0.5 s after the end (by the key's own time) plays
  again.
- The title: SALVO, the keys ("4 6 OR SOFT 1 6 MOVE", "7 8 9 × GOLD SOFT 2 5 ENTER FIRE"), ENTER PLAYS, and the best once there
  is one.
- Sound: a click for a shot (1,200 Hz, 10 ms), a beep for a kill (880 Hz, 20 ms), a falling tune for a lost ship
  (392, 330, 262 Hz, 620 ms in all), a rising one for a cleared wave (523, 659, 784 Hz, 400 ms). In one tick the
  lost ship's tune comes first, then the wave's, the kill's beep and the shot's click; while a tune plays, no click or
  beep starts (a new tune does), so the next shot does not cut it short.

**The frame's cost.** Rev A sends only the rows that changed, at 1 MHz: 52 bytes, about 0.42 ms, a row. A throwaway
model of this layout with random motion (2 to 4 divers, 2 shots, 6 enemy shots, the sway every 4th step) changed
about 71 rows a step on average, 109 at the 95th percentile and 131 at worst over 6,000 steps: about 30, 45 and
55 ms. So the 50 ms step draws every frame nearly always, and a busy moment drops one. A scrolling background would
change all 240 rows, about 100 ms, which is why the field is fixed. These are computed; the device bench measures them.

**When a frame overruns.** The steps never slow and the frames skip. The game's state is a function of the ticks'
ms alone (the clock above). A frame whose transfer takes longer than 50 ms delays the next tick, that tick carries
the extra ms into the accumulator, and once it holds two steps' worth a tick runs both; the frame drawn after that
tick shows the state after both, so one step is never seen. Play keeps wall-clock time up to the 100 ms cap. The
model is driven by the same ticks the scripts give, so it matches by construction, and the scripts include ticks of
50, 60, 99, 100 and 250 ms to check the catch-up, its two-step limit and the cap.

## What it saves

16 bytes: 'V', version 1, the best score (u32), the games started (u32), little-endian, then six zeros. Taken whole
or not at all: a save that is short, has the wrong magic or version, or a non-zero byte in the last six is not
taken. The best is updated in memory at each kill; the framework takes the save only when the app exits (C, the idle
limit, a soft off or a low battery), and stores it only if it changed, so a kill never writes flash.

## Where it lives

`games/salvo/salvo_logic.{h,c}`: the field, the step and the hits, with no draw or framework calls.
`games/salvo/salvo_app.c`: the app_def, the keys' held state and latches, the screens and the sound.
`games/salvo/stu32-game.txt`: name SALVO, folder GAMES, MIT. `tests/salvo_trace.c` prints what the oracle prints.
Budget: at most 10 KB of flash and no internal RAM, as for each game so far; the state is a few hundred bytes.

## The oracle (written before the game)

`oracle/salvo/`, Python, from this file alone:

1. **The model** (`model.py`): the rules above. The C must print the same state (the ship, each shot, each alien's
   state and place, the enemy shots, the counters, the score, lives and wave) after every step for fuzzed key
   scripts over many seeds, with held keys, taps shorter than a step, and both directions at once.
2. **Long games** (`pilot.py`): an autopilot that steers under the lowest alien and fires, and dodges an enemy shot
   above it, in the model and the C, for several seeds over many waves: wave changes, extra ships at 10,000 and the
   cap at 5, the dive limits as the waves grow, and a game played to its end. The two traces must be identical.
3. **The rules at their edges**, by hand: a shot at the top line, a third shot refused, the cooldown at 0 and 1, the
   sway at both turns, a dive's start with an empty column and with none in any slot, vx at a division that rounds
   toward zero, a diver firing at y 179 and refused at 180, a diver leaving at 239 and 240, a return landing exactly
   on its slot, a shot and an enemy shot on the same step as a lost ship, the last alien shot while diving,
   two things on the ship in one step, the enemy shots' cap of 6, a diver's x clamps, a shot crossing a diver and a
   returning alien, an enemy shot by the ship box's corner.
4. **The app on the SDK runner**: scripted games with frame hashes and the ticks' asks (`-keyframes`, `-nexts`),
   the save, and every tone and tune; ENTER partway through a title tick; ticks of 100 and 250 ms; damaged saves
   that must be refused. Since 2026-10-23 every fire key of the layout held with a move key in both pairings, and +
   held and firing nothing. The same through the .wasm and the bridge.
5. **Plants**, each of which must go red: among them the latch dropped, the ship's clamp off by one, a third shot,
   the sway's turn late, the dive's column order, vx rounded the wrong way, the diver's fire limit, a lost ship
   leaving divers out, the extra-ship cap, the save's zero bytes ignored; since 2026-10-23 each new fire key left out
   of the app, + firing, and the logic's fire keys past × not counted.
6. **On the S3**: the scripts' frames against the host's on the stand-in bench, with a control that must fail, and
   on Rev A the frame time, the changed rows and the time between frames for a scripted busy wave.
