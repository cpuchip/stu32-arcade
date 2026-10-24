# CATCH: catch the falling stars (blueprint)

Michael's ask (abacus #10256): a basket on 4 and 6 (and SOFT1 and SOFT6) catches falling stars; a gentle default
where nothing is lost and a missed star just falls away; the full mode adds a pace and lives as icons; nothing to
read. CATCH is a plain word for the game's one action, 5 letters, and fits the menu; a web search found no game
trademark on it, which is not a register search. The rules, the numbers and every sprite below are ours. This is the
design and the checks that will judge it, written before any of its code.

## A game

A basket sits at the bottom of the screen and moves left and right. Stars fall from the top. A star that drops into
the basket is caught, with a chime and a mark on the score. A star that misses falls off the bottom.

- **Gentle (the default):** stars fall slowly, one every 1.5 s, at most 3 at once. A miss costs nothing. Every 10
  catches a tune plays. The game goes on until C.
- **Full:** three lives, drawn as small baskets. A miss costs one. Every 10 catches the stars fall faster and come
  more often. The game ends when the lives are gone.

| key | does |
|---|---|
| 4 or SOFT1 | moves the basket left while held |
| 6 or SOFT6 | moves it right while held |
| 4/SOFT1 and 6/SOFT6 on the title | pick gentle (left picture) or full (right picture) |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

No other key does anything in play, so a young player pressing around can't lose a game by it.

## The rules (all integers)

**The clock, the keys, the draws.** SALVO's clock (50 ms steps, at most two a tick, the 100 ms cap). A bit for each
of the four keys; a down latches its direction for the next step, so a tap moves the basket once. Left is a left key
held or latched, right the same; both cancel. The draws as in SALVO.

**The field.** 400 × 228 px under a 12 px top line, y downward from the top line.

**The basket.** 48 × 14 at (bx, 202), its mouth the top row from bx + 4 to bx + 43. It moves 6 px a step in gentle
and 8 in full, clamped to 0-352 (it stops at the walls; nothing wraps). A game starts with bx = 176.

**Stars.** Each is 11 × 11 at (x, y), its y in 1/16 px. A new star comes when the gap counter reaches 0 and fewer than
the limit are falling: x = `draw(390)`, y = −11 px (just above the field), and the counter is set to the gap. A star
falls by its speed each step.

| | gentle | full, start | full, each 10 catches | full, limit |
|---|---|---|---|---|
| fall a step | 32 (2 px) | 32 (2 px) | +8 (0.5 px) | 96 (6 px) |
| gap (steps) | 30 | 30 | −3 | 9 |
| falling at once | 3 | 3 | +1 | 6 |

**The catch.** On the step a star's bottom moves from above the mouth's row to at or below it (before < 202 ≤ after),
it is caught if its 11 px overlap the mouth (x < bx + 44 and x + 11 > bx + 4), and then it is gone. Otherwise it keeps
falling and is a miss when its top passes the field's bottom (y ≥ 228). The test is on the crossing, so a fast star
can't skip the mouth.

**A miss:** in gentle nothing; in full the lives go down by 1 (3 at the start), and at 0 every star is gone and the
game is over.

**One step:** the basket moves; the stars fall, in the order they came, each tested; the gap counter counts down and
a star may come; every 10 catches, the pace (full).

**Score:** one mark a catch. The best is kept for each mode.

## The fine print

The model's author read the rules above with no C in sight and listed what they left open (2026-10-24); each answer
below is a rule, and the C and the model both follow it.

1. **A star's bottom** is its edge, y + 11 px (SALVO's shot bottom is y + 6 the same way), so a star is caught when its
   last row moves from above 201 to 201 or below: touching the rim. In 1/16 px a new star is at y −176; it crosses when
   y + 176 goes from under 3,232 to 3,232 or more; it is a miss at y ≥ 3,648.
2. **The gap counter** starts at 0, so the first star comes on step 1. Each step it goes down by 1 if above 0; then a
   star comes if it is 0 and there is room, counted after this step's catches and misses, and the counter is set to
   the gap, so stars come exactly `gap` steps apart. With no room it waits at 0. A draw is made only when a star comes.
3. **A new star** doesn't fall on the step it appears (the fall comes before the counter).
4. **The mouth** is the exclusive test above, in px with the basket where this step's move left it: the rim's outer
   4 px each side don't catch.
5. **Catch before miss** for each star; a star that crosses the row outside the mouth keeps falling.
6. **The pace** comes at the end of the step, once for each new multiple of 10 the score reached in it, each of the
   three to its own limit (one more at once to 6 at 30 catches, the gap to 9 at 70, the fall to 96 at 80). The counter
   is not changed when the gap shrinks. Every star falls by the current fall, including those already falling.
7. **The end.** When the lives reach 0 the stars are cleared, the game is over and the rest of the step is skipped:
   no later star is tested, no counter, no star and no pace.
8. **The bump** is 1 only on a step whose move changed bx and left it at 0 or 352, clamped or exactly. Pressing
   against a wall gives 0. The clamp applies only when the basket moves.
9. **Latches** are kept for each direction, set by any down and not cleared by an up, and cleared after every step. A
   tap of 6 against a held SOFT1 cancels for that step.
10. **Gentle** keeps lives at 0 and never ends; its tune every 10 catches changes no state.

## The screen (400 × 240, draw.h only)

**No words are needed to play.**
- **The top line:** in full, a small basket for each life, at the left; the catches at the right as 3720's tally.
- **The field:** the basket's outline with a weave of two lines, drawn over its mouth only (bx + 4 to bx + 43, its
  sides slanting in), so the basket a player sees is the part that catches; 5-point stars; a caught star flashes a
  ring of dots over the basket for 4 steps; a missed star in gentle blinks at the bottom of the screen for 4 steps.
- **The title:** the name, a basket and stars, and two framed pictures: gentle (one slow star) on the left and full
  (several stars and three small baskets) on the right, the chosen one in a thick frame; the 4 and 6 keycaps with
  arrows; the ENTER keycap blinking; each mode's best as a tally. The title opens on gentle.
- **The end (full):** this game's tally and the best, and after 0.5 s the ENTER keycap blinking.

**What still carries words or digits:** the name on the title and the keycap legends (4, 6, SOFT1, SOFT6, ENTER).

**Sound.** Notes are Hz/ms, 0 a rest. Each tick, the steps it ran are taken together, and one plays:
1. the end: 392/120 0/40 330/120 0/40 262/300, a tune of 620 ms;
2. else a lost life (full): 330/100 262/160, 260 ms;
3. else a tenth catch: 523/100 659/100 784/200, 400 ms;
4. else, when no tune is playing, one beep: a catch's chime (1,047 Hz, 30 ms); else a soft bump (100 Hz, 10 ms) when
   the basket reaches a wall, once each time it arrives, so a player hears where the edge is; else a miss's soft low
   note (196 Hz, 30 ms).

A new tune can start over one still playing. The bump outranks the miss so the stand-in's wall check can't lose it
to a star missed on the same tick.

## Frame, state, flash

- **Rows:** the basket's 14 rows when it moves, and about 12 rows for each falling star that moved a whole pixel: in
  gentle about 50 rows at most (21 ms); in full with 6 stars about 90 (38 ms). All under the 50 ms step. Reasoned from
  the layout, not modelled; Rev A's bench measures them.
- **State:** the ctx is 136 B on the host (x86-64, the SDK runner's count; the blueprint estimated about 80): 6 star
  slots, the basket, the counters, the draws, the bests, the flashes.
- **Flash:** about 3 KB. Internal RAM +0.

## The WebAssembly gate

As 3720 and MOLES: it builds as a .wasm from the same source and its host check runs the .wasm against the native
frames. It ships native.

## What it saves

16 bytes: 'K', version 1, the best gentle (u32), the best full (u32), the games started (u32), then two zeros. Taken
whole or not at all.

## Where it lives

`games/catch/catch_logic.{h,c}`, `games/catch/catch_app.c`, `games/catch/stu32-game.txt` (name CATCH, folder GAMES,
MIT), `tests/catch_trace.c`.

## The oracle

`oracle/catch/`, from this file alone by a fresh agent:
1. **The model:** every step's basket, stars, counters, score, lives, against the C, for fuzzed key scripts in both
   modes.
2. **Long games:** an autopilot that moves under the lowest star, in both modes, to full's limits; one that never
   moves, to full's end. Traces identical.
3. **Edges by hand:** a star crossing the mouth's row exactly at 202; the mouth's overlap at each end by one pixel
   and missing by one; the basket at both walls; a star at x 0 and 389; the gap and the limit at their bounds; two
   misses on one step with one life left.
4. **The app on the runner:** frames, tones, the title's choice, the save, odd ticks, damaged saves, the .wasm.
5. **Plants:** each move key left out, the latch dropped, the wall clamp off by one, the catch tested at the end
   position only, the mouth's ends off by one, the pace's limits off, a gentle miss costing a life.

## On the board

- **Stand-in:** `device/standin/catch.txt` reaches CATCH through GAMES and plays gentle. Catches depend on the
  seed, so they can't tell a live key from a dead one; the wall's bump can. From bx 176, 4 held reaches the left wall
  in 30 steps (1.5 s) and bumps once; then 6 held crosses to the right wall in 59 steps (about 3 s) and bumps once;
  SOFT1 and SOFT6 the same. The control, + held, gives no bump. Then C and the X lines.
  `scripts/device_catch.sh` runs the app scripts on the S3 against the host's frames, with a control.
- **Rollover:** not needed (one key at a time).

GAMES with nine games: see 3720.md, last section.
