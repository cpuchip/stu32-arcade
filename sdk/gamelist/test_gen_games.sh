#!/bin/bash
# SPDX-License-Identifier: MIT
# A copy of stu32-firmware's tests/gen_games_test.sh at b31cfd9 (the master), run on sdk/gamelist's copy.
# scripts/gen_games.py on throwaway git repos: a valid list generates, and each wrong one fails with its
# reason and writes nothing. The generated games.h is compiled into a registry. (From arcade's
# stu32-arcade sdk/gamelist/test_gen_games.sh; the firmware's copy of the generator is the master,
# soroban's gate. The default branch is set per command, never in the box's global git config.)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
GEN=$HERE/gen_games.py
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
ok=1
mkrepo() { # mkrepo NAME: a bare origin and a clone with a game in it, pushed
    git -c init.defaultBranch=main init -q --bare "$T/$1.git"
    git clone -q "$T/$1.git" "$T/repos/$1" 2>/dev/null
    (cd "$T/repos/$1" && mkdir -p src src/deeper inc && printf 'int a;\n' > src/a.c && printf 'int b;\n' > src/b.c \
        && printf 'int z;\n' > src/deeper/z.c && printf '#define G 1\n' > inc/g.h \
        && printf 'name = %s\nfolder = GAMES\napp = %s_app\nsources = src/*.c\ninclude = inc\nflags = -ffp-contract=off\nauthor = t\nlicence = MIT\nsource = t\n' "$2" "$1" > game.txt \
        && git add -A && git -c user.email=t@t -c user.name=t commit -q -m one && git push -q origin HEAD:main 2>/dev/null \
        && git remote set-head origin main >/dev/null)
}
mkrepo one ONE
mkrepo two TWO
PIN1=$(git -C "$T/repos/one" rev-parse HEAD)
PIN2=$(git -C "$T/repos/two" rev-parse HEAD)
mkdir -p "$T/fw"
run() { # run EXPECT(0|1) WHAT [WANTED-MESSAGE]: the list in $T/fw/games.list
    rm -rf "$T/fw/build"
    GAMES_REPOS="$T/repos" python3 "$GEN" "$T/fw/games.list" > "$T/out" 2> "$T/err"; rc=$?
    if [ "$1" = 0 ]; then
        [ "$rc" = 0 ] && echo "  ok: $2" || { echo "FAIL: $2: $(cat "$T/err")"; ok=0; }
    else
        if [ "$rc" = 1 ] && grep -q -- "$3" "$T/err" && [ ! -e "$T/fw/build" ]; then echo "  refused: $2"
        else echo "FAIL: $2 should be refused with \"$3\" and write nothing (rc $rc: $(cat "$T/err"))"; ok=0; fi
    fi
}
printf 'one one %s MIT game.txt\ntwo two %s MIT game.txt\n' "$PIN1" "$PIN2" > "$T/fw/games.list"
run 0 "two games"
mk=$T/fw/build/games/games.mk
grep -q "GAME_one_C := build/games/one/src/a.c build/games/one/src/b.c$" "$mk" && ! grep -q deeper "$mk" \
    && echo "  ok: src/*.c takes src/a.c and src/b.c, not src/deeper/z.c" || { echo "FAIL: sources: $(grep GAME_one_C "$mk")"; ok=0; }
[ -f "$T/fw/build/games/one/inc/g.h" ] && [ -f "$T/fw/build/games/one/src/a.c" ] && echo "  ok: exported at the pin" || { echo "FAIL: export"; ok=0; }
# the registry macros compile into a registry
cat > "$T/reg.c" <<'C'
typedef struct { const char *name; } app_def;
#include "games.h"
GAMES_DECLARE
const app_def keys_app = {"KEYS"}, one_app = {"ONE"}, two_app = {"TWO"};
const app_def *const reg[] = {&keys_app GAMES_ENTRIES};
const unsigned char folder[] = {0 GAMES_FOLDERS};
int main(void) { return !(sizeof reg / sizeof reg[0] == 3 && GAMES_N == 2 && folder[1] == 1 && reg[2] == &two_app); }
C
cc -std=c11 -Wall -Werror -I"$T/fw/build/games" "$T/reg.c" -o "$T/reg" && "$T/reg" && echo "  ok: games.h builds a registry of 3" || { echo "FAIL: games.h"; ok=0; }
# staged by hand, without a clone: taken; with a wrong .pin: refused
printf 'one one %s MIT game.txt\n' "$PIN1" > "$T/fw/games.list"
GAMES_REPOS="$T/repos" python3 "$GEN" "$T/fw/games.list" > /dev/null 2>&1
cp -r "$T/fw/build/games/one" "$T/stage-one"
mv "$T/repos/one" "$T/one-away"
mkdir -p "$T/fw/build/games" && rm -rf "$T/fw/build/games/one" && cp -r "$T/stage-one" "$T/fw/build/games/one"
GAMES_REPOS="$T/repos" python3 "$GEN" "$T/fw/games.list" > "$T/out" 2> "$T/err" && grep -q "staged: not checked as pushed" "$T/out" \
    && grep -q "GAME_one_C := build/games/one/src/a.c build/games/one/src/b.c$" "$T/fw/build/games/games.mk" \
    && echo "  ok: staged without a clone (and said so)" || { echo "FAIL: staged: $(cat "$T/err")"; ok=0; }
echo 0123456789abcdef0123456789abcdef01234567 > "$T/fw/build/games/one/.pin"
GAMES_REPOS="$T/repos" python3 "$GEN" "$T/fw/games.list" > "$T/out" 2> "$T/err"
[ $? = 1 ] && grep -q "nothing staged" "$T/err" && echo "  refused: a stage with another pin" || { echo "FAIL: a stage with another pin was taken"; ok=0; }
mv "$T/one-away" "$T/repos/one"
# the wrong ones
printf 'one one %s MIT game.txt\n' "0123456789abcdef0123456789abcdef01234567" > "$T/fw/games.list"; run 1 "a pin not in the repo" "is not in"
(cd "$T/repos/one" && printf 'int c;\n' > src/c.c && git add -A && git -c user.email=t@t -c user.name=t commit -q -m local)
printf 'one one %s MIT game.txt\n' "$(git -C "$T/repos/one" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "an unpushed pin" "is not on"
printf 'one one %s Apache-2.0 game.txt\n' "$PIN1" > "$T/fw/games.list"; run 1 "a licence the manifest disagrees with" "the list says"
printf 'one one %s MIT game.txt\none one %s MIT game.txt\n' "$PIN1" "$PIN1" > "$T/fw/games.list"; run 1 "an id twice" "listed twice"
mkrepo three ONE
printf 'one one %s MIT game.txt\nthree three %s MIT game.txt\n' "$PIN1" "$(git -C "$T/repos/three" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "a name twice" "is taken"
bad() { # bad WHAT SED-ON-MANIFEST MESSAGE: a fresh repo whose manifest is changed, pushed
    rm -rf "$T/repos/bad" "$T/bad.git"; mkrepo bad BAD
    (cd "$T/repos/bad" && sed -i "$2" game.txt && git add -A && git -c user.email=t@t -c user.name=t commit -q -m bad && git push -q origin HEAD:main 2>/dev/null)
    printf 'bad bad %s MIT game.txt\n' "$(git -C "$T/repos/bad" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "$1" "$3"
}
# a port's credit (original) is taken; a key nobody knows is not
rm -rf "$T/repos/port" "$T/port.git"; mkrepo port PORT
(cd "$T/repos/port" && printf 'original = Someone, example.org/x (Unlicense)\n' >> game.txt && git add -A \
    && git -c user.email=t@t -c user.name=t commit -q -m port && git push -q origin HEAD:main 2>/dev/null)
printf 'port port %s MIT game.txt\n' "$(git -C "$T/repos/port" rev-parse HEAD)" > "$T/fw/games.list"; run 0 "a port with original"
bad "a key nobody knows" "\$a credit = x" "unknown"
bad "a six-character name" "s/^name = .*/name = BABALS/" "must be 1 to 5"
bad "a folder that is not GAMES or TOOLS" "s/^folder = .*/folder = MUSIC/" "GAMES or TOOLS"
bad "a source outside the repo" "s|^sources = .*|sources = ../x.c|" "unsafe path"
bad "a flag not allowed" "s/^flags = .*/flags = -O0/" "is not allowed"
bad "sources matching nothing" "s|^sources = .*|sources = nowhere/*.c|" "match no .c file"
bad "an app that is not a C name" "s/^app = .*/app = 9lives/" "not a C name"
# a file name from the game's tree carrying make syntax into games.mk (soroban's gate)
rm -rf "$T/repos/bad" "$T/bad.git"; mkrepo bad BAD
(cd "$T/repos/bad" && printf 'int e;\n' > 'src/e$(shell touch PWNED).c' && git add -A && git -c user.email=t@t -c user.name=t commit -q -m bad \
    && git push -q origin HEAD:main 2>/dev/null)
printf 'bad bad %s MIT game.txt\n' "$(git -C "$T/repos/bad" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "a file name with make syntax" "a character outside"
bad "a source pattern with a space" "s|^sources = .*|sources = src/a b.c|" "match no .c file\\|a character outside"
bad "a name that would close a C comment" "s|^name = .*|name = */;//|" "letters, digits or blanks"
# a symbolic link among what is exported: a source, or a file under an include directory (fresh eyes on the gate)
rm -rf "$T/repos/bad" "$T/bad.git"; mkrepo bad BAD
(cd "$T/repos/bad" && ln -s /etc/hostname src/z.c && git add -A && git -c user.email=t@t -c user.name=t commit -q -m link \
    && git push -q origin HEAD:main 2>/dev/null)
printf 'bad bad %s MIT game.txt\n' "$(git -C "$T/repos/bad" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "a source that is a symbolic link" "symbolic link"
rm -rf "$T/repos/bad" "$T/bad.git"; mkrepo bad BAD
(cd "$T/repos/bad" && ln -s ../src/a.c inc/x.h && git add -A && git -c user.email=t@t -c user.name=t commit -q -m link \
    && git push -q origin HEAD:main 2>/dev/null)
printf 'bad bad %s MIT game.txt\n' "$(git -C "$T/repos/bad" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "a symbolic link under an include directory" "symbolic link"
# no refs/remotes/origin/HEAD, and a local branch named origin/main on an unpushed commit (fresh eyes on the gate)
rm -rf "$T/repos/bad" "$T/bad.git"; mkrepo bad BAD
(cd "$T/repos/bad" && git remote set-head origin -d && printf 'int u;\n' > src/u.c && git add -A \
    && git -c user.email=t@t -c user.name=t commit -q -m unpushed && git branch origin/main)
printf 'bad bad %s MIT game.txt\n' "$(git -C "$T/repos/bad" rev-parse HEAD)" > "$T/fw/games.list"; run 1 "no origin HEAD (a local origin/main branch)" "no refs/remotes/origin/HEAD"
# a game's include directories reach its own objects only, never the firmware's (fresh eyes on the gate)
printf 'one one %s MIT game.txt\n' "$PIN1" > "$T/fw/games.list"; run 0 "one game"
grep -q "^GAME_one_INC := -Ibuild/games/one/inc$" "$mk" && ! grep -q "GAMES_INC" "$mk" && grep -q 'INCLUDE_DIRECTORIES "${REPO}/build/games/one/inc"' "$T/fw/build/games/games.cmake" \
    && echo "  ok: a game's include directories on its own rule only" || { echo "FAIL: include scope: $(grep INC "$mk")"; ok=0; }
[ "$ok" = 1 ] && { echo "GAMES-LIST GENERATOR GREEN"; exit 0; }
echo "GAMES-LIST GENERATOR RED"; exit 1
