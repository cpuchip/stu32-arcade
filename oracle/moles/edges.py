"""MOLES's rules at their edges (docs/moles.md, "The oracle" 3), worked out by hand from the doc; the runner is
oracle/kids_edges.py. Holes are numbered in the order 7 8 9 4 5 6 1 2 3, so key 5 is hole 4 (h4), key 7 hole 0 and
key 1 hole 6. Most cases start with the next mole far off (`set due 100`), so no mole comes unless the case wants
one."""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import kids_edges  # noqa: E402

G = ['game g 1 0', 'set due 100']
F = ['game f 1 0', 'set due 100']
M = ['game m 1 0', 'set due 100']
ONE_R0 = re.compile(r'(E/)*R0(/E)*')

CASES = [
    ('a bonk on the first rising step', G + ['hole 4 R0', 'down 5', 'steps 1'], 1,
     {1: {'h4': 'B0', 'score': '1', 'bonks': '1'}}),
    ('a bonk on the last up step, and the stay down by one', F + ['hole 4 U49', 'down 5', 'steps 1'], 1,
     {1: {'h4': 'B0', 'score': '1', 'stay': '49'}}),
    ('up 50 steps: a getaway', F + ['hole 4 U49', 'steps 1'], 1, {1: {'h4': 'D0', 'lives': '2'}}),
    ('a key on the step it starts sinking is wrong', F + ['hole 4 D0', 'down 5', 'steps 1'], 1,
     {1: {'h4': 'D1', 'score': '0', 'stun': '6'}}),
    ('rising to up, bonked to sinking', G + ['hole 0 R3', 'hole 1 B7', 'steps 1'], 1, {1: {'h0': 'U0', 'h1': 'D0'}}),
    ('sunk: empty, last, and the gentle wait', ['game g 1 0', 'hole 2 D3', 'steps 1'], 1,
     {1: {'h2': 'E', 'last': '2', 'due': '30', 'wait': '30'}}),
    ('the first mole at step 20, the wait kept', ['game g 1 0', 'steps 20'], 20,
     {19: {'holes': 'E/E/E/E/E/E/E/E/E', 'due': '1'}, 20: {'holes': ONE_R0, 'due': '0', 'wait': '30'}}),
    ('a rise after a wait: the wait down by one', ['game g 1 0', 'set due 1', 'set last 4', 'steps 1'], 1,
     {1: {'holes': ONE_R0, 'h4': 'E', 'wait': '29'}}),
    ('the wait at its floor', ['game g 1 0', 'set wait 10', 'set due 1', 'set last 4', 'steps 1'], 1,
     {1: {'holes': ONE_R0, 'wait': '10'}}),
    ('gentle: no mole while one is out', ['game g 1 0', 'set due 1', 'hole 0 D1', 'steps 1'], 1,
     {1: {'holes': 'D2/E/E/E/E/E/E/E/E', 'due': '0'}}),
    ('the one hole left that is not the last', ['game f 1 0', 'set due 1', 'set last 4']
     + [f'hole {i} D1' for i in (0, 1, 2, 3, 5, 6, 8)] + ['steps 1'], 1,
     {1: {'h7': 'R0', 'h4': 'E', 'due': '20'}}),
    ('one mole at a time below 20 bonks', ['game f 1 0', 'set due 1', 'hole 0 U0', 'steps 1'], 1,
     {1: {'holes': 'U1/E/E/E/E/E/E/E/E', 'due': '0'}}),
    ('a bonked mole holds the next back', ['game f 1 0', 'set due 1', 'hole 0 B2', 'steps 1'], 1,
     {1: {'holes': 'B3/E/E/E/E/E/E/E/E', 'due': '0'}}),
    ('a sinking one does not', ['game f 1 0', 'set due 1', 'hole 0 D1', 'steps 1'], 1,
     {1: {'h0': 'D2', 'holes': re.compile(r'D2/(E/)*R0(/E)*'), 'due': '20'}}),
    ('from 20 bonks, two at once', ['game f 1 0', 'set bonks 20', 'set due 1', 'hole 0 U0', 'steps 1'], 1,
     {1: {'h0': 'U1', 'holes': re.compile(r'U1/(E/)*R0(/E)*'), 'due': '20'}}),
    ('the stay at its floor', F + ['set stay 16', 'hole 4 U0', 'down 5', 'steps 1'], 1, {1: {'stay': '16'}}),
    ('the 20th bonk: the gap 14', F + ['set bonks 19', 'hole 4 U0', 'down 5', 'steps 1'], 1,
     {1: {'bonks': '20', 'gap': '14'}}),
    ('a bonk and a getaway on one step, one life left', F + ['set lives 1', 'hole 4 U0', 'hole 0 U49', 'down 5',
                                                             'steps 1'], 1,
     {1: {'score': '1', 'lives': '0', 'over': '1', 'h4': 'D0', 'h0': 'D0'}}),
    ('the end: the moles out go down, a sinking one keeps its count', F + ['set lives 1', 'hole 0 U49', 'hole 1 D2',
                                                                           'hole 2 U5', 'hole 3 R1', 'steps 1'], 1,
     {1: {'over': '1', 'h0': 'D0', 'h1': 'D3', 'h2': 'D0', 'h3': 'D0'}}),
    ('a right key then a wrong one: the bonk, then the stun', F + ['hole 4 U0', 'down 5', 'down 1', 'steps 1'], 1,
     {1: {'score': '1', 'stun': '6', 'h4': 'B0'}}),
    ('a wrong key then a right one: the stun only', F + ['hole 4 U0', 'down 1', 'down 5', 'steps 1'], 1,
     {1: {'score': '0', 'stun': '6', 'h4': 'U1'}}),
    ("the stun's last ignored step and its first free one",
     F + ['hole 4 U0', 'down 1', 'steps 1', 'up 1', 'steps 5', 'down 5', 'steps 1', 'up 5', 'down 5', 'steps 1'], 8,
     {1: {'stun': '6'}, 6: {'stun': '1'}, 7: {'score': '0', 'stun': '0'}, 8: {'score': '1'}}),
    ('a held key pressed again is a repeat', F + ['hole 4 U0', 'down 5', 'down 5', 'steps 1'], 1,
     {1: {'score': '1', 'stun': '0'}}),
    ('down, up, down in one step: a bonk, then a wrong key', F + ['hole 4 U0', 'down 5', 'up 5', 'down 5', 'steps 1'],
     1, {1: {'score': '1', 'stun': '6'}}),
    ('a gentle wrong key costs nothing', G + ['down 1', 'steps 1'], 1, {1: {'stun': '0', 'score': '0'}}),
    ('each key to its hole: 7 8 9, 4 5 6, 1 2 3 in turn', G + [f'hole {i} U0' for i in range(9)]
     + [l for k in '789456123' for l in (f'down {k}', f'up {k}', 'steps 1')], 9,
     {1: {'h0': 'B0', 'h1': 'U1'}, 9: {'holes': 'D0/B7/B6/B5/B4/B3/B2/B1/B0', 'score': '9'}}),
    ('two steps a tick at most', G + ['set acc 150', 'tick 100'], 2, {2: {'acc': '150'}}),
    ("middle's start: five lives, the stay 80, the gap 30, no wait", ['game m 1 0', 'steps 1'], 1,
     {1: {'lives': '5', 'stay': '80', 'gap': '30', 'wait': '0', 'due': '19'}}),
    ('middle: up 79 steps stays, up 80 is a getaway', M + ['hole 4 U78', 'hole 0 U79', 'steps 1'], 1,
     {1: {'h4': 'U79', 'h0': 'D0', 'lives': '4'}}),
    ('middle: a bonk lowers the stay by one', M + ['hole 4 U10', 'down 5', 'steps 1'], 1,
     {1: {'h4': 'B0', 'score': '1', 'stay': '79'}}),
    ("middle: the stay's floor is 40", M + ['set stay 40', 'hole 4 U10', 'down 5', 'steps 1'], 1,
     {1: {'score': '1', 'stay': '40'}}),
    ('middle: the stay 41 drops to 40', M + ['set stay 41', 'hole 4 U10', 'down 5', 'steps 1'], 1,
     {1: {'stay': '40'}}),
    ('middle: a wrong key then a right one, both count, no stun', M + ['hole 4 U0', 'down 1', 'down 5', 'steps 1'], 1,
     {1: {'score': '1', 'stun': '0', 'h4': 'B0'}}),
    ('middle: a rise sets the gap, 30', ['game m 1 0', 'set due 1', 'steps 1'], 1, {1: {'holes': ONE_R0, 'due': '30'}}),
    ('middle: one mole at a time past 20 bonks', ['game m 1 0', 'set due 1', 'set bonks 25', 'hole 0 U0', 'steps 1'],
     1, {1: {'holes': 'U1/E/E/E/E/E/E/E/E', 'due': '0', 'gap': '30'}}),
    ('middle: the gap stays 30 at the 20th bonk', M + ['set bonks 19', 'hole 4 U0', 'down 5', 'steps 1'], 1,
     {1: {'bonks': '20', 'gap': '30'}}),
    ('middle: the fifth getaway ends the game', M + ['set lives 1', 'hole 4 U79', 'hole 0 U3', 'steps 1'], 1,
     {1: {'lives': '0', 'over': '1', 'h4': 'D0', 'h0': 'D0'}}),
]

if __name__ == '__main__':
    kids_edges.main(CASES)
