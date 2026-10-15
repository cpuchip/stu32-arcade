#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Every game's manifest (games/*/stu32-game.txt) names files and directories that are in this tree: its sources (each
glob matching at least one .c), its include directories, and its app's name in one of its sources. The firmware's
gen_games.py checks these at a pin; this catches a move or a rename here before a pin is ever taken.
Usage: check_manifests.py [ROOT]; exit 1 on a miss."""
import glob
import os
import sys

root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..'))
bad = 0
for m in sorted(glob.glob(os.path.join(root, 'games', '*', 'stu32-game.txt'))):
    kv = dict(l.split('=', 1) for l in open(m).read().split('\n') if '=' in l and not l.startswith('#'))
    kv = {k.strip(): v.strip() for k, v in kv.items()}
    rel = os.path.relpath(m, root)
    srcs = []
    for s in kv.get('sources', '').split():
        hits = sorted(glob.glob(os.path.join(root, s)))
        if not hits:
            print(f'MISS {rel}: source {s} matches nothing'); bad += 1
        srcs += hits
    for d in kv.get('include', '').split():
        if not os.path.isdir(os.path.join(root, d)):
            print(f'MISS {rel}: include {d} is no directory'); bad += 1
    app = kv.get('app', '')
    if not app or not any(f'app_def {app} ' in open(f).read() for f in srcs):
        print(f'MISS {rel}: no source defines app_def {app}'); bad += 1
    if not bad:
        print(f'ok   {rel}: {len(srcs)} sources, include {kv.get("include", "")}')
sys.exit(1 if bad else 0)
