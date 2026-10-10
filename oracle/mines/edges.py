"""The rules at their edges (docs/mines.md, the oracle's check 3): hand-made fields and moves, each with the lines it
must give written here by hand from the blueprint, not computed.

  edges.py script     the cases as one trace script
  edges.py expect     the lines they must give: each 'cell', 'mine', 'hit' and the 'state=' of each move

Most cases are on the medium board, 16 wide: cell (x, y) is y * 16 + x. The easy board is 9 wide, the hard 30.
"""
import sys

CASES = [
    # one mine in the corner, cell 0 = (0, 0); cell 1 = (1, 0) shows 1, cell 2 = (2, 0) shows 0
    ('a number opened alone', ['field medium 0', 'open 1', 'cell 1', 'cell 2'], ['play', 'cell 1 open', 'cell 2 closed']),
    ('the chord with too few flags: nothing', ['field medium 0', 'open 1', 'open 1', 'cell 2'],
     ['play', 'play', 'cell 2 closed']),
    ('the chord with too many flags: nothing', ['field medium 0', 'open 1', 'flag 0', 'flag 2', 'open 1', 'cell 16'],
     ['play', 'play', 'play', 'play', 'cell 16 closed']),
    ('the chord with the right flag: its 0 neighbour floods the field, a win',
     ['field medium 0', 'open 1', 'flag 0', 'open 1'], ['play', 'play', 'won']),
    ('the chord with a wrong flag: the mine opens, a loss, and the others open first',
     ['field medium 0', 'open 1', 'flag 2', 'open 1', 'cell 18', 'cell 2'],
     ['play', 'play', 'lost', 'cell 18 open', 'cell 2 flag']),
    ('a flagged cell opened: nothing', ['field medium 0', 'flag 5', 'open 5', 'cell 5'], ['play', 'play', 'cell 5 flag']),
    ('a flag on an opened cell: nothing', ['field medium 0', 'open 1', 'flag 1', 'cell 1'],
     ['play', 'play', 'cell 1 open']),
    # cell 100 = (4, 6), cell 99 = (3, 6), cell 200 = (8, 12): far from the mine, so 99 and 200 show 0
    ('the flood stops at a flag, so no win', ['field medium 0', 'flag 100', 'open 200', 'cell 100', 'cell 99'],
     ['play', 'play', 'cell 100 flag', 'cell 99 open']),
    ('a win with the mine unflagged: it is flagged at the end', ['field medium 0', 'open 200', 'cell 0'],
     ['won', 'cell 0 flag']),
    ('the mine opened: a loss', ['field medium 0', 'open 0', 'cell 1'], ['lost', 'cell 1 closed']),
    ('moves after the end do nothing', ['field medium 0', 'open 0', 'open 200', 'flag 5', 'cell 200', 'cell 5'],
     ['lost', 'lost', 'lost', 'cell 200 closed', 'cell 5 closed']),
    ('a chord on an opened 0 with no flags around: its closed neighbours open (here the last safe cell, a win)',
     ['field medium 200', 'flag 1', 'open 50', 'cell 1', 'flag 1', 'open 0', 'cell 1'],
     ['play', 'play', 'cell 1 flag', 'play', 'won', 'cell 1 open']),
    # mines at (0, 0) and (2, 0); cell 1 shows 2; flags on (0, 1) = 16 and (2, 1) = 18, both wrong
    ('a chord onto two mines: a loss, the first in neighbour order shown as the one opened',
     ['field medium 0,2', 'open 1', 'flag 16', 'flag 18', 'open 1', 'hit', 'cell 17'],
     ['play', 'play', 'play', 'lost', 'hit 0', 'cell 17 open']),
    # the first open (cell 100 = (4, 6)) and its neighbours are never mines
    ('the first open and its neighbours are safe',
     ['game 4294967296 0 medium noguess', 'open 100'] + [f'mine {c}' for c in (83, 84, 85, 99, 100, 101, 115, 116, 117)],
     ['play'] + [f'mine {c} no' for c in (83, 84, 85, 99, 100, 101, 115, 116, 117)]),
    ('a flag before the first open, then opening it: nothing is dealt',
     ['game 4294967296 1 medium noguess', 'flag 100', 'open 100', 'cell 100'], ['play', 'play', 'cell 100 flag']),
    # rev 2: the boards' own corners and edges
    ('easy: the mine in the far corner (8, 8) = 80; its neighbour (7, 8) = 79 shows 1, then the far corner floods it',
     ['field easy 80', 'open 79', 'cell 78', 'open 0', 'cell 80'], ['play', 'cell 78 closed', 'won', 'cell 80 flag']),
    ('hard: the mine in the far corner (29, 15) = 479; opening (0, 0) floods every other cell, a win',
     ['field hard 479', 'open 0', 'cell 479', 'cell 478'], ['won', 'cell 479 flag', 'cell 478 open']),
    # a wrong neighbour across the row's end would make the edge cell show 1 and stop the flood; the corner (0, 0)
    # stays closed either way, since it touches the mine and only numbers touch it
    ('medium: (15, 0) = 15 at the right edge has no neighbour on the next row\'s left, so beside a mine at (0, 1) = 16 '
     'it shows 0 and floods',
     ['field medium 16', 'open 15', 'cell 14', 'cell 0'], ['play', 'cell 14 open', 'cell 0 closed']),
    ('hard: the same at its right edge, (29, 0) = 29 beside a mine at (0, 1) = 30',
     ['field hard 30', 'open 29', 'cell 28', 'cell 0'], ['play', 'cell 28 open', 'cell 0 closed']),
    ('easy: the same at its right edge, (8, 0) = 8 beside a mine at (0, 1) = 9',
     ['field easy 9', 'open 8', 'cell 7', 'cell 0'], ['play', 'cell 7 open', 'cell 0 closed']),
    # rev 2: a deal under way, as the app's first open starts it
    ('opens during a deal do nothing, flags still work, and the deal\'s end lets the open through',
     ['game 4294967296 2 medium noguess', 'deal 100', 'open 100', 'cell 100', 'flag 5', 'cell 5', 'step 400', 'open 100',
      'cell 100', 'mine 100'],
     ['play', 'cell 100 closed', 'play', 'cell 5 flag', 'play', 'cell 100 open', 'mine 100 no']),
    ('a deal does not start on a flagged cell, so opening it then does nothing',
     ['game 4294967296 3 hard noguess', 'flag 255', 'deal 255', 'step 400', 'open 255', 'cell 255'],
     ['play', 'play', 'cell 255 flag']),
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
