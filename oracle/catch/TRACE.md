# CATCH's trace format (oracle/catch/model.py and tests/catch_trace.c print the same lines)

Both read a script, one command a line (blank lines and lines starting with # are skipped), and print one state line
after every step. The rules are docs/catch.md.

## Commands

- `game MODE SEED GAME` a new game: MODE `g` (gentle), `m` (middle) or `f` (full); the draw state is `mix64(SEED ^ mix64(GAME))`; bx 176, `last` -1, no stars,
  the accumulator 0, no keys held or latched.
- `down K`, `up K`: K is one of `4 S1 6 S6 E P`. Only the first four move the basket.
- `tick MS`, `steps N`, `draw?` as in oracle/salvo/TRACE.md.
- `set NAME VALUE` one of the state line's scalars (bx score lives fall gap max count last acc).
- `star X Y` adds a star as the newest (Y in 1/16 px; `last` is not changed); `clear stars` removes all (and leaves `last`).

Once the game is over, ticks and steps run nothing and print nothing.

## The state line

    S step=<n> acc=<ms> bx=<px> score=<n> lives=<n> fall=<n> gap=<n> max=<n> count=<n> last=<-1|px> bump=<0|1>
      over=<0|1> stars=<list>

(on one line). `fall` is the fall a step in 1/16 px, `gap` the steps between stars, `max` the most falling at once,
`count` the gap counter, `last` the x of the last star that came (-1 before any: the reach's `last`), `bump` 1 on a step whose move brought the basket to a wall (the step the bump sounds),
`lives` 0 in gentle. `stars` is `x,y` pairs oldest first joined by `;`, or `-`.
