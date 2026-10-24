# MOLES: bonk the mole (blueprint)

Michael's ask (abacus #10256): nine holes on the screen in the number pad's layout; a mole pops up; pressing the key in
the same spot bonks it, with a pop and a bonk sound; a gentle default where the mole waits until it's hit and the
pace rises slowly; nothing to read. The game form (moles popping from holes, hit them) is decades old; its best-known
name is a trademark, so nothing here uses it. The rules, the numbers and every sprite below are ours. This is the
design and the checks that will judge it, written before any of its code.

**The name** is MOLES, Michael's pick (abacus #10280): 5 letters, so it fits the menu. A web search found no game
trademark on it, which is not a register search.

## A game

Nine holes sit on the screen in three rows, as the keys sit on the calculator:

| | | |
|---|---|---|
| 7 | 8 | 9 |
| 4 | 5 | 6 |
| 1 | 2 | 3 |

(app_keys.h puts 7 8 9 on the row above 4 5 6, and 1 2 3 below.) A mole pops up from one hole. Pressing that hole's
key bonks it: it goes dizzy and sinks, and a mark goes on the score. Pressing another key puffs dust from that hole;
in gentle that costs nothing.

- **Gentle (the default):** one mole at a time, and it waits until it's bonked. After each bonk the next mole comes a
  little sooner, from 1.5 s down to 0.5 s. Nothing is lost, and the game goes on until C.
- **Full:** a mole goes back down if it isn't bonked in time, and that costs one of three lives (drawn as small
  moles). Each bonk shortens the time a mole stays up, and from 20 bonks two moles can be up at once. A wrong key
  stuns the player for 0.3 s (abacus's proposal #10278, Michael's yes #10280), so pressing every key fast doesn't win. The game
  ends when the lives are gone.

| key | does |
|---|---|
| 1-9 | bonks the hole in the same spot |
| 4/SOFT1 and 6/SOFT6 on the title | pick gentle (left picture) or full (right picture) |
| 0 on the title | the digits on the moles, on or off |
| ENTER | starts a game from the title, or a new one after the end |
| C | the framework's: saves and ends the game |

**The digits (the option abacus proposed).** With it on, a mole that is up shows its key's digit on its front, so the
game teaches where each digit sits. Off by default; it is a grown-up's setting on the title (0 toggles it, shown as a
mole picture with and without a digit) and the save keeps it.

## The rules (all integers)

**The clock.** SALVO's: a step every 50 ms, a tick's ms (at most 100) into an accumulator, at most two steps a tick. A
game starts on ENTER; its first tick counts only the time since that key.

**The keys.** Each down of 1-9 is a bonk at its hole, kept until the next step so a tap shorter than a step counts;
several downs in one step are taken in the order they came. A held key bonks once (no repeat). Ups do nothing.

**The draws.** As in SALVO: `x = mix64(seed ^ mix64(game))`, `draw(n)` is `x = mix64(x)` then `x % n`.

**A hole** is empty, rising (4 steps), up, bonked (8 steps), or sinking (4 steps), and then empty again. A mole can be
bonked while rising or up.

**Choosing a hole.** The candidates are the holes that are empty and are not the last hole a mole left from, in
the order 7 8 9 4 5 6 1 2 3; the hole is candidate `draw(count)`. So the same hole never comes twice running and two
moles never share a hole.

**Gentle.** The wait W starts at 30 steps (1.5 s). When a bonked mole has sunk, W steps pass, then a mole rises from
a chosen hole; W then drops by 1, to no less than 10 (0.5 s). A mole stays up until bonked. The first mole rises 20
steps after the game starts.

**Full.** The stay S starts at 50 steps (2.5 s) and drops by 1 a bonk to no less than 16 (0.8 s). The gap between
moles rising is 20 steps, then 14 from 20 bonks on; at most 1 mole is up (rising, up or bonked), 2 from 20 bonks on. A
mole that has been up S steps without a bonk sinks, and the lives go down by 1 (3 at the start). With none left, every
mole sinks and the game is over.

**The stun (full only; Michael, abacus #10280).** A key whose hole has no bonkable mole starts a stun of 6
steps (0.3 s): the bonks that come during it are ignored and don't extend it, and a dizzy puff shows over that hole.
Keys taken in one step before the wrong one still count. Without it, pressing all nine keys fast bonks every mole.

**One step:** the bonks, in order (a wrong one may start the stun); the holes' timers and the stun; a new mole if one
is due.

**Score:** one mark a bonk, in both modes. The best is kept for each mode.

## The fine print

The model's author read the rules above with no C in sight and listed what they left open (2026-10-24); each answer
below is a rule, and the C and the model both follow it.

1. **What starts this step waits a step.** A bonk or a stun started by this step's keys is not counted this step, so a
   bonked mole shows B0 to B7, then D0 (8 + 4 + W, the stand-in's arithmetic). A mole rises as R0 in the step's last
   phase, and keys first meet it the next step; it can be bonked as R0 to R3, then U0 on.
2. **The order of the timers:** the next mole's count (`due`) goes down by 1 if above 0; then the holes in the order
   7 8 9 4 5 6 1 2 3; then the stun. So a wait set by a mole sinking this step is not counted this step, and setting
   `due` to N in step s brings a mole in step s + N.
3. **The first mole** comes 20 steps in, in both modes.
4. **Gentle.** A mole rises only when `due` is 0 and all nine holes are empty; while one is out, `due` stays 0. Every
   hole that empties sets `due` to W. W drops by 1 (to no less than 10) at a rise that ends a wait, not at the first
   mole's: the wait uses W before the drop.
5. **The fields a mode does not use** read 0: gentle's stay and gap, full's wait.
6. **The stay** counts only up steps: a mole is bonkable up from U0 to U(S − 1) and gets away when U reaches S (`n ≥
   S`). One stay for all moles, read as it changes, so a bonk that lowers it can let another up mole get away the same
   step.
7. **From 20 bonks** the gap becomes 14 at the bonk that brings bonks to 20 (a count already running is not cut), and
   the two-mole limit is read from the bonks each time.
8. **The gap** counts from one rise to the next: `due` is set to it at a rise. When `due` is 0 and the limit is
   reached, it stays 0 and nothing is drawn; the mole comes on the first step it may, which can be the step the
   blocking mole starts sinking (sinking moles don't count toward the limit, bonked ones do).
9. **Candidates** are the empty holes only. `last` is set when a hole empties. With no candidate nothing rises, no
   draw is made and `due` stays 0. One draw a rise, and none for a rise that doesn't happen.
10. **Getaways** in one step go in hole order, lives stopping at 0. A bonk scores before a getaway in the same step.
11. **The end** is checked after the holes and the stun: every rising, up or bonked mole goes to D0 (sinking ones
    keep their count), the game is over and no mole rises that step. Only a getaway that brings the lives to 0 ends a
    game.
12. **The stun** is 6 at the end of the wrong key's step; the keys of the next 6 steps are ignored and the 7th step's
    keys count. During it right keys are ignored too, with no dust and no extension, and keys after the wrong one in its own
    step are ignored. A wrong key is one whose hole is empty, bonked or sinking, so a second press of a mole bonked
    earlier in the same step stuns. Gentle never starts a stun.
13. **Held keys.** A down of a key already held is a repeat and is ignored; down, up, down in one step is two bonks.
    Keys reach only the first step after them, so the second step of a two-step tick sees none.
14. **The clock.** A tick adds at most 100 ms and runs at most two steps, whatever the accumulator holds. After the
    end a tick adds nothing.
15. **Score and bonks** both count one a bonk; the stay, the gap and the limit use bonks.

## The screen (400 × 240, draw.h only)

**No words are needed to play.**
- **The top line, 12 px:** in full, a small mole for each life, at the left. The score at the right as 3720's tally:
  strokes in groups of five, five groups folding into a star; at most 30 stars.
- **The field:** nine cells of 133 × 76 px under the top line, each with a hole drawn as a flat oval near its bottom.
  The mole is a 44 × 40 sprite in 1 bit that rises out of the hole over its 4 rising steps, shows dizzy stars over
  its head while bonked, and sinks back. A wrong key puffs 5 dots of dust over its hole for 4 steps. With the digits
  on, the up mole carries its digit at scale 2.
- **The title:** the name, a large mole picture, and two framed pictures: gentle (one mole, sleeping) on the left and
  full (two moles and three small lives) on the right, the chosen one in a thick frame. The 0 keycap with the digit
  picture, the ENTER keycap blinking, and each mode's best as a tally once there is one. The title opens on gentle.
- **The end (full):** every mole down, this game's tally and the best, and after 0.5 s (by the key's time) the ENTER
  keycap blinking.

**What still carries words or digits:** the name on the title, the keycap legends (0, ENTER, matched by shape), and
the digits on the moles when that option is on, which is the point of it.

**Sound:** a pop as a mole rises (a two-note tune, 660 then 880 Hz, 30 ms each); a bonk on a hit (220 Hz then 1,320
Hz, 30 and 15 ms); a soft thunk for a wrong key (150 Hz, 20 ms); a chime every 10 bonks (523, 659, 784 Hz); in full a
falling two-note for a mole that got away and a falling tune at the end. SALVO's order and its no-beep-over-a-tune
rule apply.

## Frame, state, flash

- **Rows:** a step changes the cells with a mole moving in them, about 45 rows each (the mole and its
  dizzy stars), plus 12 for the top line on a bonk. One mole: about 57 rows, 24 ms; two moles in different rows:
  about 102, 43 ms; all under the 50 ms step. These
  are reasoned from the layout, not modelled; Rev A's bench measures them.
- **State:** about 64 B (nine holes' states and timers, the wait or stay, the lives, the draws, the save's fields).
- **Flash:** about 3-4 KB (the mole's frames, the hole, the title pictures, the tally). Internal RAM +0.

## The WebAssembly gate

As 3720: apps.h v1.2 only, integers only, so it builds as a .wasm from the same source and its host check plays the
app scripts through the .wasm with the native frames. It ships native.

## What it saves

16 bytes: 'M', version 1, the best gentle (u32), the best full (u32), the games started (u32), the digits option (0 or
1), then one zero. Taken whole or not at all.

## Where it lives

`games/moles/moles_logic.{h,c}`, `games/moles/moles_app.c`, `games/moles/stu32-game.txt` (name MOLES, folder GAMES,
MIT), `tests/moles_trace.c`.

## The oracle

`oracle/moles/`, from this file alone by a fresh agent:
1. **The model:** every step's holes, timers, score, lives, against the C, for fuzzed key scripts (right keys, wrong
   keys, several in one step, held keys) over many seeds and both modes.
2. **Long games:** an autopilot that bonks every mole after a set delay, in both modes, through W's and S's floors
   and the two-mole stage; another that misses on purpose until the end. Traces identical.
3. **Edges by hand:** a bonk on a mole's first rising step and on its last up step; a bonk on the step it starts
   sinking (not a hit); the same hole twice running refused; the candidates while two moles are up; the wait and the
   stay at their floors; a bonk and a getaway on the same step with one life left; in full, a right key then a wrong
   one in one step (the bonk counts, then the stun), the stun's last ignored step and its first free one.
4. **The app on the runner:** frames, tones, the title's choices, the digits option, the save, odd ticks, damaged
   saves, and the .wasm.
5. **Plants:** each key mapped to its hole (a plant swapping 1 and 7, the rows upside down), the latch dropped, the
   repeat-hole rule off, the floors off by one, a sinking mole counted as a hit, the digits option not saved, the
   stun removed, the stun in gentle, a stun extended by the keys during it.

## On the board

- **Stand-in:** `device/standin/moles.txt` reaches MOLES through GAMES and plays gentle. A sweep presses 1 to 9 in
  turn. A gentle mole waits until it's bonked, so a sweep that starts while a mole is up and ends before the next one
  can rise bonks exactly once, whatever the seed. The script's pacing makes both true (abacus #10278):
  - **The first sweep** starts at least 1.2 s after ENTER. The first mole starts rising at 20 steps (1.0 s) and can be
    bonked while rising.
  - **Each sweep** takes under 1.1 s. After a bonk the next mole can't be bonked for 8 + 4 + W steps (bonked,
    sinking, the wait); W is 30 at first and never below 10, so at least 22 steps, 1.1 s.
  - **The next sweep** starts at least 2.5 s after the last one ends. The bonk came before that end, and 8 + 4 + 30
    steps (2.1 s) covers the longest gap, so a mole is up when it starts.

  Five sweeps paced so give five bonks and 40 thunks. The control, five sweeps of + (no job) paced the same, gives
  none. Any mix-up of keys and holes still gives one bonk a sweep, so the board shows the keys arrive; the host's plants
  show they land on the right holes. Then C and the X lines. `scripts/device_moles.sh` runs the app scripts on the S3
  against the host's frames, with a control.
- **Rollover:** not needed for play (one key at a time). A key pressed while another is still held must count; the
  scripts press with overlap.

GAMES with nine games: see 3720.md, last section.
