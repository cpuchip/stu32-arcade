"""The rules at their edges (docs/snake.md, the oracle's check 3): hand-made states and keys, each with the lines it
must give written here by hand from the blueprint, not computed.

  edges.py script     the cases as one trace script
  edges.py expect     the lines they must give: each key's verdict, 'end ...' or 'flying', 'head ...', 'interval ...'
"""
import sys

import cycle

FAR = '30,20'  # food out of the way


def body(*cells):
    return ' '.join(f'{x},{y}' for x, y in cells)


CASES = [
    # (name, script lines, the lines they must give)
    ('the head into the cell the tail is leaving: safe',
     [f'case 0 {FAR} ' + body((5, 5), (6, 5), (6, 6), (5, 6)), 'step 1', 'end?', 'head?'],
     ['flying', 'head 5,5 dir=0 len=4']),
    ('the head into a body cell that is not the tail: dead, nothing moves',
     [f'case 3 {FAR} ' + body((5, 5), (6, 5), (7, 5), (7, 6), (6, 6)), 'key 8', 'step 1', 'head?'],
     ['key 8 taken', 'end dead', 'head 6,6 dir=0 len=5']),
    ('the left wall', [f'case 3 {FAR} ' + body((2, 10), (1, 10), (0, 10)), 'step 1'], ['end dead']),
    ('the right wall', [f'case 1 {FAR} ' + body((37, 10), (38, 10), (39, 10)), 'step 1'], ['end dead']),
    ('the top wall', [f'case 0 {FAR} ' + body((10, 2), (10, 1), (10, 0)), 'step 1'], ['end dead']),
    ('the bottom wall', [f'case 2 {FAR} ' + body((10, 19), (10, 20), (10, 21)), 'step 1'], ['end dead']),
    ('one cell short of the right wall: safe',
     [f'case 1 {FAR} ' + body((36, 10), (37, 10), (38, 10)), 'step 1', 'end?', 'head?'],
     ['flying', 'head 39,10 dir=1 len=3']),
    ('a reverse key dropped', [f'case 1 {FAR} ' + body((8, 10), (9, 10), (10, 10)), 'key 4'], ['key 4 dropped']),
    ('the current direction again, dropped', [f'case 1 {FAR} ' + body((8, 10), (9, 10), (10, 10)), 'key 6'],
     ['key 6 dropped']),
    ('two turns inside one step, taken on two steps; a third dropped',
     [f'case 1 {FAR} ' + body((8, 10), (9, 10), (10, 10)), 'key 8', 'key 4', 'key 2', 'step 1', 'head?', 'step 1',
      'head?'],
     ['key 8 taken', 'key 4 taken', 'key 2 dropped', 'head 10,9 dir=0 len=3', 'head 9,9 dir=3 len=3']),
    ('a turn judged against the last queued one, not the current direction',
     [f'case 1 {FAR} ' + body((8, 10), (9, 10), (10, 10)), 'key 8', 'key 2'], ['key 8 taken', 'key 2 dropped']),
    ('food ahead: one longer, the tail stays',
     ['case 1 11,10 ' + body((8, 10), (9, 10), (10, 10)), 'step 1', 'end?', 'head?'],
     ['flying', 'head 11,10 dir=1 len=4']),
    ('the interval', ['interval 0', 'interval 1', 'interval 17', 'interval 18', 'interval 19'],
     ['interval 0 150', 'interval 1 145', 'interval 17 65', 'interval 18 60', 'interval 19 60']),
]


def full_grid_case():
    """879 cells along the cycle from (0, 0), the head at (0, 2) moving up, the food on (0, 1), the last free cell"""
    seq = [0]
    for _ in range(879):
        c = seq[-1]
        d = cycle.next_dir(c)
        seq.append(c + cycle.M.DX[d] + cycle.M.DY[d] * cycle.W)
    cells = [(c % cycle.W, c // cycle.W) for c in seq[:879]]
    assert cells[-1] == (0, 2) and (seq[879] % cycle.W, seq[879] // cycle.W) == (0, 1)
    return ('the last free cell eaten: won at 880', ['case 0 0,1 ' + body(*cells), 'step 1', 'head?'],
            ['end won', 'head 0,1 dir=0 len=880'])


def main():
    cases = CASES + [full_grid_case()]
    if sys.argv[1] == 'script':
        for name, lines, _ in cases:
            print(f'# {name}')
            print('\n'.join(lines))
    elif sys.argv[1] == 'expect':
        for _, _, want in cases:
            print('\n'.join(want))


if __name__ == '__main__':
    main()
