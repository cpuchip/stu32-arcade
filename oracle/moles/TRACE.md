# MOLES's trace format (oracle/moles/model.py and tests/moles_trace.c print the same lines)

Both read a script, one command a line (blank lines and lines starting with # are skipped), and print one state line
after every step. The rules are docs/moles.md.

## Commands

- `game MODE SEED GAME` a new game: MODE `g` (gentle), `m` (middle) or `f` (full); the draw state is `mix64(SEED ^ mix64(GAME))`; every hole empty,
  the accumulator 0, no keys latched.
- `down K`, `up K`: K is one of `1`-`9`, `0`, `E` (ENTER) or `P` (+). Only 1-9 bonk in play.
- `tick MS`, `steps N`, `draw?` as in oracle/salvo/TRACE.md.
- `set NAME VALUE` one of the state line's scalars (wait stay gap due bonks score lives stun last acc).
- `hole I CODE` sets hole I (0-8 in the order 7 8 9 4 5 6 1 2 3) to CODE as the state line writes it.

Once the game is over, ticks and steps run nothing and print nothing.

## The state line

    S step=<n> acc=<ms> score=<n> bonks=<n> lives=<n> wait=<n> stay=<n> gap=<n> due=<n> stun=<n> last=<-1|0-8>
      over=<0|1> holes=<9 codes>

(on one line). `due` is the steps until the next mole is due to rise (as the rules count it), `stun` the stun's steps
left, `last` the last hole a mole left from (-1 before any), `lives` 0 in gentle. `holes` is 9 codes joined by `/` in
the order 7 8 9 4 5 6 1 2 3: `E` empty, or a letter and that state's steps so far: `R<n>` rising, `U<n>` up, `B<n>`
bonked, `D<n>` sinking.
