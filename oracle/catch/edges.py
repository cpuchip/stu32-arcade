"""CATCH's rules at their edges (docs/catch.md, "The oracle" 3), worked out by hand from the doc; the runner is
oracle/kids_edges.py. Most cases start from G: a gentle game with the gap counter far off (1000), so no star comes
unless the case wants one. In 1/16 px a star crosses the mouth's row when y + 176 goes from under 3,232 to 3,232 or
more, so a star at 3,024 crosses on a gentle step (fall 32) and one at 3,023 doesn't; the basket is at 176."""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import kids_edges  # noqa: E402

G = ['game g 1 0', 'set count 1000']
F = ['game f 1 0', 'set count 1000']
ANY_X = r'([0-9]|[1-9][0-9]|[1-3][0-9][0-9])'

CASES = [
    ('touching the rim is a catch', G + ['star 200 3024', 'steps 1'], 1, {1: {'score': '1', 'stars': '-'}}),
    ('one short of the rim is not yet', G + ['star 200 3023', 'steps 1'], 1, {1: {'score': '0', 'stars': '200,3055'}}),
    ('past the row already is never a catch', G + ['star 200 3060', 'steps 1'], 1,
     {1: {'score': '0', 'stars': '200,3092'}}),
    ('the mouth at its left end: 170 in, 169 out', G + ['star 169 3024', 'star 170 3024', 'steps 1'], 1,
     {1: {'score': '1', 'stars': '169,3056'}}),
    ('the mouth at its right end: 219 in, 220 out', G + ['star 219 3024', 'star 220 3024', 'steps 1'], 1,
     {1: {'score': '1', 'stars': '220,3056'}}),
    ('a star at x 0 into a basket at the left wall', G + ['set bx 0', 'star 0 3024', 'steps 1'], 1,
     {1: {'score': '1'}}),
    ('a star at x 389 into a basket at the right wall', G + ['set bx 352', 'star 389 3024', 'steps 1'], 1,
     {1: {'score': '1'}}),
    ('the left wall: clamped, a bump, then none while pressed', G + ['set bx 4', 'down 4', 'steps 2'], 2,
     {1: {'bx': '0', 'bump': '1'}, 2: {'bx': '0', 'bump': '0'}}),
    ('onto the wall exactly is a bump', G + ['set bx 6', 'down S1', 'steps 1'], 1, {1: {'bx': '0', 'bump': '1'}}),
    ('the right wall', G + ['set bx 350', 'down 6', 'steps 1'], 1, {1: {'bx': '352', 'bump': '1'}}),
    ('full moves 8', F + ['down S6', 'steps 1'], 1, {1: {'bx': '184', 'bump': '0'}}),
    ('a tap shorter than a step moves once', G + ['down 4', 'up 4', 'steps 2'], 2, {1: {'bx': '170'}, 2: {'bx': '170'}}),
    ('both directions cancel', G + ['down 4', 'down 6', 'steps 1'], 1, {1: {'bx': '176'}}),
    ('a tap of 6 against a held SOFT1 cancels that step', G + ['down S1', 'steps 1', 'down 6', 'up 6', 'steps 2'], 3,
     {1: {'bx': '170'}, 2: {'bx': '170'}, 3: {'bx': '164'}}),
    ('the first star on step 1, not falling yet', ['game g 1 0', 'steps 1'], 1,
     {1: {'count': '30', 'stars': re.compile(ANY_X + ',-176')}}),
    ('the counter at 1: a star, and the gap again', F + ['set count 1', 'steps 2'], 2,
     {1: {'count': '30', 'stars': re.compile(ANY_X + ',-176')}, 2: {'count': '29'}}),
    ('no room: the counter waits at 0', G + ['set max 1', 'set count 1', 'star 10 0', 'steps 2'], 2,
     {1: {'count': '0', 'stars': '10,32'}, 2: {'count': '0', 'stars': '10,64'}}),
    ('a gentle miss costs nothing', G + ['star 10 3640', 'steps 1'], 1,
     {1: {'stars': '-', 'lives': '0', 'over': '0'}}),
    ('a star at 3,615 stays, at 3,616 is missed', G + ['star 10 3615', 'star 30 3616', 'steps 1'], 1,
     {1: {'stars': '10,3647'}}),
    ('a full miss costs a life', F + ['star 10 3640', 'steps 1'], 1, {1: {'lives': '2', 'over': '0'}}),
    ('two misses on one step with one life left', F + ['set lives 1', 'star 10 3640', 'star 20 3640', 'steps 1'], 1,
     {1: {'lives': '0', 'over': '1', 'stars': '-'}}),
    ('the 10th catch: the pace', F + ['set score 9', 'star 200 3024', 'steps 1'], 1,
     {1: {'score': '10', 'fall': '40', 'gap': '27', 'max': '4'}}),
    ('two catches across one ten: the pace once', F + ['set score 18', 'star 200 3024', 'star 210 3024', 'steps 1'],
     1, {1: {'score': '20', 'fall': '40', 'gap': '27', 'max': '4'}}),
    ('two catches past a ten without landing on it: the pace once', F + ['set score 19', 'star 200 3024',
                                                                         'star 210 3024', 'steps 1'], 1,
     {1: {'score': '21', 'fall': '40', 'gap': '27', 'max': '4'}}),
    ('a deep catch: the rim crossed by a whole fall', F + ['set fall 96', 'star 200 3040', 'steps 1'], 1,
     {1: {'score': '1', 'stars': '-'}}),
    ('the limits', F + ['set score 79', 'set fall 88', 'set gap 12', 'set max 5', 'star 200 2984', 'steps 1'], 1,
     {1: {'score': '80', 'fall': '96', 'gap': '9', 'max': '6'}}),
    ('past the limits nothing moves', F + ['set score 89', 'set fall 96', 'set gap 9', 'set max 6',
                                         'star 200 2960', 'steps 1'], 1,
     {1: {'score': '90', 'fall': '96', 'gap': '9', 'max': '6'}}),
    ('gentle has no pace', G + ['set score 9', 'star 200 3024', 'steps 1'], 1,
     {1: {'score': '10', 'fall': '32', 'gap': '30', 'max': '3'}}),
    ('the game over line, and nothing after', F + ['set lives 1', 'star 10 3640', 'steps 3'], 1,
     {1: {'over': '1'}}),
]

if __name__ == '__main__':
    kids_edges.main(CASES)
