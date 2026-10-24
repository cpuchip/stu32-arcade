# 3720's trace format (oracle/rocks/model.py and tests/rocks_trace.c print the same lines)

Both read a script, one command a line (blank lines and lines starting with # are skipped), and print one state line
after every step, so a diff of the two outputs finds the first step where they disagree. The rules are docs/3720.md.

## Commands

- `game MODE SEED GAME` a new game: MODE `g` (gentle) or `f` (full); SEED is app_seed()'s u32, GAME the count of games
  started before this one; the draw state is `mix64(SEED ^ mix64(GAME))`. Wave 1's rocks are made at once (their draws
  first), the ship at its start with the start's shield, the accumulator 0, no keys held or latched.
- `down K`, `up K` a key's down or up. K is one of `4 S1 6 S6 7 8 9 S2 G 5 X S5 E P` (4, SOFT1, 6, SOFT6, 7, 8, 9,
  SOFT2, GOLD, 5, ×, SOFT5, ENTER, +). ENTER and + have no job in play.
- `tick MS` a tick of MS ms, capped at 100, into the accumulator; each step it runs prints its line.
- `steps N` N steps straight away (the accumulator untouched), each printing its line.
- `set NAME VALUE` sets one of the state line's scalars by its name (x y vx vy a turn cool shield lost ships score
  next wave pause clock acc).
- `rock I CODE` sets slot I (0-23) to CODE as the state line writes it; `clear rocks` empties every slot.
- `shot X Y VX VY LIFE` adds a shot as the newest (values in Q, LIFE its steps left); `clear shots` removes all.
- `draw?` prints `draw N` with the next value of draw(1000), consuming it.

Once the game is over, ticks and steps run nothing and print nothing.

## The state line

One line, fields in this order, separated by single spaces:

    S step=<n> acc=<ms> x=<q> y=<q> vx=<q> vy=<q> a=<0-23> turn=<n> cool=<n> shield=<n> lost=<n> ships=<n>
      score=<n> next=<n> wave=<n> pause=<n> clock=<n> over=<0|1> shots=<list> rocks=<24 codes>

(on one line in the output). `step` counts the game's steps from 1. Positions and speeds are in Q (1/65,536 px), x in
0..400 × Q − 1 and y in 0..228 × Q − 1 after wrapping. `turn` is the turn timer, `cool` the shot cooldown, `shield` the
shield's steps left, `lost` the lost ship's burst steps left (0 while the ship is in play), `ships` the ships held
including the one in play, `next` the next multiple of 5,000 for an extra ship (5000 at the start; 0 in gentle), `pause`
the wave's pause steps left (0 when not pausing), `clock` the play clock (steps with the ship in play). `shots` is the
shots oldest first, each `x,y,vx,vy,life`, joined by `;`, or `-` when there are none. `rocks` is 24 codes joined by
`/`, in slot order: `-` for an empty slot, or `<size>,<x>,<y>,<vx>,<vy>,<outline>` with size `L`, `M` or `S`.

Numbers are plain decimal integers, negative ones with a leading `-`.
