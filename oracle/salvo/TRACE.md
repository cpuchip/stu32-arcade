# Salvo's trace format (oracle/salvo/model.py and tests/salvo_trace.c print the same lines)

Both read a script, one command a line (blank lines and lines starting with # are skipped), and print one state line
after every step, so a diff of the two outputs finds the first step where they disagree.

## Commands

- `game SEED GAME` a new game: SEED is app_seed()'s u32, GAME the count of games started; the draw state is
  `mix64(SEED ^ mix64(GAME))`. sx 192, lives 3, score 0, wave 1, the accumulator 0, no keys held or latched, and
  the wave's start (docs/salvo.md).
- `down K`, `up K` a key's down or up. K is one of `4 S1 6 S6 8 E 9 X P` (4, SOFT1, 6, SOFT6, 8, ENTER, 9, ×, +).
- `tick MS` a tick of MS ms, capped at 100, into the accumulator; each step it runs prints its line.
- `steps N` N steps straight away (the accumulator untouched), each printing its line.
- `set NAME VALUE` sets one of the scalars below by its trace name (sx lives score wave over ship wavet fx fy fdir
  sway dive cool next acc).
- `alien I CODE` sets alien I (slot order, r × 8 + c) to CODE as the state line writes it (below).
- `shot X Y` and `eshot X Y` add a shot or an enemy shot (as the newest); `clear shots`, `clear eshots` remove all.
- `draw?` prints `draw N` with the next value of draw(1000), consuming it (for checking the generator).

Once the game is over, ticks and steps run nothing and print nothing.

## The state line

One line, fields in this order, separated by single spaces:

    S step=<n> acc=<ms> sx=<x> lives=<n> score=<n> next=<n> wave=<n> over=<0|1> ship=<t> wavet=<t>
      fx=<x> fy=<y> fdir=<1|-1> sway=<n> dive=<n> cool=<n> shots=<list> eshots=<list> aliens=<32 codes>

(on one line in the output). `step` counts the game's steps from 1. `next` is the next multiple of 10,000 the score
must reach for an extra ship (10000 at a game's start). `ship` is the ship's timer (0 while it is in play, which is
the only time it is alive), `wavet` the wave's timer (0 when not running). `fdir` is 1 for rightward. `sway` is
the sway's count, `dive` the dive counter (both 0 until the entry ends). A list is `x,y` pairs oldest first joined
by `;`, or `-` when empty. `aliens` is 32 codes joined by `/`, in slot order:

- `S` in its slot;
- `X` dead;
- `D<x>,<y>,<k>,<vx>,<f>` diving at (x, y), k its dive's steps taken so far (0 before its first move), vx its
  current vx (0 before its first move), f its fire_at, written `-1` once it has fired;
- `R<x>,<y>` returning at (x, y).

Numbers are plain decimal integers, negative ones with a leading `-`.
