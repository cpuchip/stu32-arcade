# stu32-arcade

Games for the STU-32, a programmable RPN calculator built on an ESP32-S3 with a 400 × 240 one-bit memory LCD. Each
game is an app on the calculator's APPS menu, written in C against the firmware's app interface (`apps.h`), and each
also builds as a WebAssembly module that the firmware can run through WAMR.

## The games

| APPS name | what it is | design |
|---|---|---|
| BABAL | a ball on a track of slabs, jump the gaps: a port of Babal by Christian Mauduit (gitlab.com/liberecofr/babal), first an HP 48 game | docs/stage1-rules.md, docs/stage2.md |
| AIM | four tiles from 1 to 9 and a target: reach it in RPN with + − × ÷, each tile used once | docs/aim.md |
| LAND | set a lander down on a pad, with fuel, integer physics and a terrain whose left and right edges meet | docs/land.md |
| SNAKE | the snake on a 40 × 22 grid, faster as it grows | docs/snake.md |
| MINES | clear a minefield (9 × 9, 16 × 16 or 30 × 16); every board can be cleared without a guess, or play classic rules | docs/mines.md |

Babal's rules (rust/babalcore) are ported module by module to C in `games/babal/core/`; the renderer and the app are in `games/babal/app/`.
Babal's music and fonts are not included and were never ported. The other games are in `games/<name>/`, each as a
logic file with no drawing or framework calls and an app file with the screens, keys and sound.

## How the games are checked

Each game has a design document written before its code. From that document alone, a separate Python model of the
rules is written (`oracle/<name>/`), and the C must print the same state as the model after every move of many
fuzzed and hand-made games. Scripted games are then played through the app on a host runner (`sdk/runner/apprun.c`),
and every frame's hash, the save and every tone are compared with committed values. Finally, faults are planted in
copies of the sources, and each must turn its check red.

    make test          Babal's own tests (babalcore's unit and doc tests, ported)
    make oracle        Babal against the original Rust, byte for byte (needs cargo)
    make aimcheck      AIM; likewise landcheck, snakecheck, minescheck
    make appcheck      the Babal app against babalcore, end to end
    make sdkcheck      the drawing library against a per-pixel reference
    make wasmcheck     Babal built as WebAssembly against Babal native (needs the variables below)

Each game check also builds the game as WebAssembly and plays the same scripts through the WAMR bridge when
`WASI_SDK`, `WAMRC` and `WAMR_TREE` are set (a WASI SDK, a `wamrc` and a WAMR source tree).

`device/` holds an ESP32-S3 bench for each game, built with ESP-IDF v5.4.2 in Espressif's Docker image
(`scripts/device*.sh`): it plays the same scripts on the chip and compares each frame's hash with the host's,
with a control that must fail.

## Building

You need a C11 compiler, `make`, `git` and `python3`; `cargo` for `make oracle`; Docker for the device benches.
The game checks also use `xxd`. On a kernel with `vm.mmap_rnd_bits` at 32, gcc 14.4 runs their sanitized legs
(AddressSanitizer) clean and gcc 12.2 doesn't: its sanitizer runtime dies at random with "AddressSanitizer:DEADLYSIGNAL"
(measured on 2026-10-23: 3 of 5 runs of one script with gcc 12.2, 0 of 5 with gcc 14.4). With gcc 12.2, turn off
address-space randomization (`setarch -R`): on 2026-10-23 every check run that way passed.

The checks read nine files of the calculator's firmware (github.com/cpuchip/stu32-firmware): the 5 × 7 font, the
screen, key and app headers, and the drawing library with its test. The Makefile exports them at the commit named in
`FIRMWARE_PIN` from a clone of that repository, by default `../stu32-firmware` (`make FIRMWARE_REPO=path/to/clone
...` for another place).

## Writing a game

`docs/sdk.md` describes the app interface, the drawing library, the runner and the games list; `sdk/template/` is a
small game to start from. `sdk/wasm/` is the bridge that runs a game built as WebAssembly; `probes/wasm/` holds the
measurements behind it and the patches to WAMR it needs on the ESP32-S3.

## How this was built

By Claude, as a team of Claude Code sessions with one job each, on Michael Stufflebeam's direction:
docs/how-this-was-built.md names them and explains the message and decision numbers in the history.

## Licence

MIT (LICENSE), for everything here except:

- `games/babal/core/` is a port of rust/babalcore, which its author dedicated to the public domain under the Unlicense;
  LICENSE-babalcore keeps that notice.
- The patches to WAMR in `probes/wasm/` are modifications of WAMR and are under its licence, Apache-2.0 WITH
  LLVM-exception (probes/wasm/NOTICE).

## Trademarks

HP is a trademark of HP Development Company, L.P. This project is not affiliated with or endorsed by HP; the HP 48 is
named only as the calculator Babal was first written for.
