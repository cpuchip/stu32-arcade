"""The rules at their edges (docs/mines.md, the oracle's check 3): hand-made fields and moves, each with the lines it
must give written here by hand from the blueprint, not computed.

  edges.py script     the cases as one trace script
  edges.py expect     the lines they must give: each 'cell', 'mine', 'hit' and the 'state=' of each move
"""
import sys

CASES = [
    # one mine in the corner, cell 0 = (0, 0); cell 1 = (1, 0) shows 1, cell 2 = (2, 0) shows 0
    ('a number opened alone', ['field 0', 'open 1', 'cell 1', 'cell 2'], ['play', 'cell 1 open', 'cell 2 closed']),
    ('the chord with too few flags: nothing', ['field 0', 'open 1', 'open 1', 'cell 2'],
     ['play', 'play', 'cell 2 closed']),
    ('the chord with too many flags: nothing', ['field 0', 'open 1', 'flag 0', 'flag 2', 'open 1', 'cell 24'],
     ['play', 'play', 'play', 'play', 'cell 24 closed']),
    ('the chord with the right flag: its 0 neighbour floods the field, a win',
     ['field 0', 'open 1', 'flag 0', 'open 1'], ['play', 'play', 'won']),
    ('the chord with a wrong flag: the mine opens, a loss, and the others open first',
     ['field 0', 'open 1', 'flag 2', 'open 1', 'cell 26', 'cell 2'], ['play', 'play', 'lost', 'cell 26 open', 'cell 2 flag']),
    ('a flagged cell opened: nothing', ['field 0', 'flag 5', 'open 5', 'cell 5'], ['play', 'play', 'cell 5 flag']),
    ('a flag on an opened cell: nothing', ['field 0', 'open 1', 'flag 1', 'cell 1'], ['play', 'play', 'cell 1 open']),
    ('the flood stops at a flag, so no win', ['field 0', 'flag 100', 'open 300', 'cell 100', 'cell 99'],
     ['play', 'play', 'cell 100 flag', 'cell 99 open']),
    ('a win with the mine unflagged: it is flagged at the end', ['field 0', 'open 300', 'cell 0'],
     ['won', 'cell 0 flag']),
    ('the mine opened: a loss', ['field 0', 'open 0', 'cell 1'], ['lost', 'cell 1 closed']),
    ('moves after the end do nothing', ['field 0', 'open 0', 'open 300', 'flag 5', 'cell 300', 'cell 5'],
     ['lost', 'lost', 'lost', 'cell 300 closed', 'cell 5 closed']),
    ('a chord on an opened 0 with no flags around: its closed neighbours open (here the last safe cell, a win)',
     ['field 300', 'flag 1', 'open 50', 'cell 1', 'flag 1', 'open 0', 'cell 1'],
     ['play', 'play', 'cell 1 flag', 'play', 'won', 'cell 1 open']),
    ('a chord onto two mines: a loss, the first in neighbour order shown as the one opened',
     ['field 0,2', 'open 1', 'flag 24', 'flag 26', 'open 1', 'hit', 'cell 25'],
     ['play', 'play', 'play', 'lost', 'hit 0', 'cell 25 open']),
    # the first open (cell 100 = (4, 4)) and its neighbours are never mines
    ('the first open and its neighbours are safe',
     ['game 4294967296 0', 'open 100'] + [f'mine {c}' for c in (75, 76, 77, 99, 100, 101, 123, 124, 125)],
     ['play'] + [f'mine {c} no' for c in (75, 76, 77, 99, 100, 101, 123, 124, 125)]),
    ('a flag before the first open, then opening it: nothing is dealt',
     ['game 4294967296 1', 'flag 100', 'open 100', 'cell 100'], ['play', 'play', 'cell 100 flag']),
]


def main():
    if sys.argv[1] == 'script':
        for name, lines, _ in CASES:
            print(f'# {name}')
            print('\n'.join(lines))
    elif sys.argv[1] == 'expect':
        for _, _, want in CASES:
            print('\n'.join(want))


if __name__ == '__main__':
    main()
