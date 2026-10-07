#!/bin/sh
# Clone the reference (Babal, the Unlicense) at the one commit stage 1 is judged against.
# Never edited; refuses any other commit.
set -eu
PIN=fa985cd49111270edade2c7bebdab7ce204be6a8
DIR="$(dirname "$0")/babal"
if [ ! -d "$DIR/.git" ]; then
    git clone -q https://gitlab.com/liberecofr/babal.git "$DIR"
fi
git -C "$DIR" checkout -q "$PIN"
HEAD=$(git -C "$DIR" rev-parse HEAD)
[ "$HEAD" = "$PIN" ] || { echo "babal is at $HEAD, not $PIN" >&2; exit 1; }
if [ -n "$(git -C "$DIR" status --porcelain)" ]; then
    echo "babal checkout is modified; the reference is read-only" >&2; exit 1
fi
echo "babal $PIN"
