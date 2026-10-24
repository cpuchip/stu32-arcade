"""3720's rules at their edges (docs/3720.md, "The oracle" 3), worked out by hand from the doc; the runner is
oracle/kids_edges.py. Values are in Q (65,536 a px). Most cases start from D: a gentle game with the rocks cleared but
one still medium rock parked at (0, 0), far from everything, so no pause starts; the ship is at (200, 114) px with the
start's shield. Slot i is r<i>."""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import kids_edges  # noqa: E402

Q = 65536
D = ['game g 1 0', 'clear rocks', 'rock 23 M,0,0,0,0,0']
DF = ['game f 1 0', 'clear rocks', 'rock 23 S,0,0,0,0,0']
PARK = [f'rock {i} M,{300 * Q},{200 * Q},0,0,0' for i in range(1, 22)]   # slots 1-21 taken, far from the play
N = r'-?[0-9]+'


def piece(size, x, y):
    return re.compile(f'{size},{x},{y},{N},{N},[0-3]')


CASES = [
    ('x wraps across 0', D + [f'rock 0 L,{Q // 2},{100 * Q},{-Q},0,0', 'steps 1'], 1,
     {1: {'r0': f'L,{400 * Q - Q // 2},{100 * Q},{-Q},0,0'}}),
    ('y wraps across 0', D + [f'rock 0 M,{100 * Q},{Q // 4},0,{-Q // 2},1', 'steps 1'], 1,
     {1: {'r0': f'M,{100 * Q},{228 * Q - Q // 4},0,{-Q // 2},1'}}),
    ('a hit across the x seam', D + [f'rock 0 M,{399 * Q},{100 * Q},0,0,0', f'shot {Q} {100 * Q} 0 0 5', 'steps 1'], 1,
     {1: {'r0': '-', 'score': '1', 'shots': '-'}}),
    ('a shot exactly r away misses', D + [f'rock 0 M,{100 * Q},{100 * Q},0,0,0', f'shot {112 * Q} {100 * Q} 0 0 5',
                                          'steps 1'], 1,
     {1: {'r0': f'M,{100 * Q},{100 * Q},0,0,0', 'shots': f'{112 * Q},{100 * Q},0,0,4'}}),
    ('a shot a Q inside r hits', D + [f'rock 0 M,{100 * Q},{100 * Q},0,0,0', f'shot {112 * Q - 1} {100 * Q} 0 0 5',
                                      'steps 1'], 1, {1: {'r0': '-', 'score': '1'}}),
    ('a shot only its first half meets', D + [f'rock 0 M,{100 * Q},{100 * Q},0,0,0',
                                              f'shot {84 * Q} {100 * Q} {28 * Q} 0 5', 'steps 1'], 1,
     {1: {'r0': '-', 'score': '1', 'shots': '-'}}),
    ('a fourth gentle shot refused', D + [f'shot {300 * Q} {50 * Q} 0 0 10'] * 3 + ['down G', 'steps 1'], 1,
     {1: {'shots': ';'.join([f'{300 * Q},{50 * Q},0,0,9'] * 3), 'cool': '0'}}),
    ('the cooldown at 1 holds, at 0 fires', D + ['set cool 1', 'down G', 'steps 2'], 2,
     {1: {'shots': '-', 'cool': '0'}, 2: {'shots': f'{200 * Q},{114 * Q - 32 * 16384},0,{-28 * 16384},20', 'cool': '3'}}),
    ('a turn tap during the timer turns once it is 0', D + ['set turn 2', 'down 4', 'up 4', 'steps 2'], 2,
     {1: {'a': '0', 'turn': '1'}, 2: {'a': '23', 'turn': '2'}}),
    ('both turn keys: no turn, no timer', D + ['down 4', 'down 6', 'steps 1'], 1, {1: {'a': '0', 'turn': '0'}}),
    ('a held turn every 2 steps', D + ['down 6', 'steps 4'], 4,
     {1: {'a': '1', 'turn': '2'}, 2: {'a': '1', 'turn': '1'}, 3: {'a': '2', 'turn': '2'}, 4: {'a': '2', 'turn': '1'}}),
    ('a turn tap while the ship is lost is cleared', D + ['set lost 2', 'down 6', 'up 6', 'steps 2'], 2,
     {2: {'a': '0', 'lost': '0'}}),
    ('one step of thrust, and drag', D + ['down 8', 'steps 1'], 1, {1: {'vy': '-3175', 'y': str(114 * Q - 3175)}}),
    ('drag leaves a speed under 32 Q', D + ['set vx 31', 'steps 1'], 1, {1: {'vx': '31', 'x': str(200 * Q + 31)}}),
    ('drag rounds toward zero going left', D + ['set vx -100', 'steps 1'], 1, {1: {'vx': '-97'}}),
    ('the cap after drag', D + ['set vy -140000', 'steps 1'], 1, {1: {'vy': '-131072', 'y': str(114 * Q - 131072)}}),
    ('a gentle large splits in two', D + [f'rock 0 L,{100 * Q},{100 * Q},0,0,0', f'shot {100 * Q} {100 * Q} 0 0 5',
                                          'steps 1'], 1,
     {1: {'r0': piece('M', 100 * Q, 100 * Q), 'r1': piece('M', 100 * Q, 100 * Q), 'r2': '-', 'score': '1'}}),
    ('a split into the last empty slot', D + PARK + [f'rock 0 L,{100 * Q},{100 * Q},0,0,0',
                                                     f'shot {100 * Q} {100 * Q} 0 0 5', 'steps 1'], 1,
     {1: {'r0': piece('M', 100 * Q, 100 * Q), 'r22': piece('M', 100 * Q, 100 * Q),
          'r21': f'M,{300 * Q},{200 * Q},0,0,0'}}),
    ('a split with no empty slot keeps one piece', D + PARK + [f'rock 22 M,{300 * Q},{200 * Q},0,0,0',
                                                               f'rock 0 L,{100 * Q},{100 * Q},0,0,0',
                                                               f'shot {100 * Q} {100 * Q} 0 0 5', 'steps 1'], 1,
     {1: {'r0': piece('M', 100 * Q, 100 * Q), 'r22': f'M,{300 * Q},{200 * Q},0,0,0', 'r23': 'M,0,0,0,0,0'}}),
    ('full: a small one breaks to nothing for 50', DF + [f'rock 0 S,{100 * Q},{100 * Q},0,0,0',
                                                         f'shot {100 * Q} {100 * Q} 0 0 5', 'steps 1'], 1,
     {1: {'r0': '-', 'score': '50', 'next': '5000'}}),
    ('full: an extra ship at 5,000', DF + ['set score 4990', f'rock 0 L,{100 * Q},{100 * Q},0,0,0',
                                           f'shot {100 * Q} {100 * Q} 0 0 5', 'steps 1'], 1,
     {1: {'score': '5000', 'ships': '4', 'next': '10000'}}),
    ('full: none past 5', DF + ['set score 4990', 'set ships 5', f'rock 0 L,{100 * Q},{100 * Q},0,0,0',
                                f'shot {100 * Q} {100 * Q} 0 0 5', 'steps 1'], 1,
     {1: {'score': '5000', 'ships': '5', 'next': '10000'}}),
    ('a rock 17 px away hits the ship, free in the first minute', D + ['set shield 0', 'set clock 1199',
                                                                       f'rock 0 M,{217 * Q},{114 * Q},0,0,0', 'steps 1'],
     1, {1: {'lost': '20', 'ships': '3', 'r0': '-', 'score': '0', 'clock': '1199'}}),
    ('at play clock 1,200 a loss costs a ship', D + ['set shield 0', 'set clock 1200',
                                                     f'rock 0 M,{217 * Q},{114 * Q},0,0,0', 'steps 1'], 1,
     {1: {'lost': '20', 'ships': '2'}}),
    ('18 px away misses', D + ['set shield 0', f'rock 0 M,{218 * Q},{114 * Q},0,0,0', 'steps 1'], 1,
     {1: {'lost': '0', 'r0': f'M,{218 * Q},{114 * Q},0,0,0'}}),
    ("the shield's last step still protects", D + ['set shield 1', f'rock 0 M,{217 * Q},{114 * Q},0,0,0', 'steps 1'], 1,
     {1: {'lost': '0', 'shield': '0'}}),
    ('full: every loss costs a ship', DF + ['set shield 0', f'rock 0 M,{215 * Q},{114 * Q},0,0,0', 'steps 1'], 1,
     {1: {'lost': '20', 'ships': '2', 'r0': piece('S', 215 * Q, 114 * Q)}}),
    ('the return: the start, the shield, the clock', D + ['set lost 1', f'set x {50 * Q}', 'set a 5', 'steps 1'], 1,
     {1: {'lost': '0', 'shield': '60', 'x': str(200 * Q), 'a': '0', 'clock': '1'}}),
    ('no ship left: the game is over', D + ['set lost 1', 'set ships 0', 'steps 3'], 1,
     {1: {'over': '1', 'lost': '0'}}),
    ('the last rock gone: the pause, the shots gone', ['game g 1 0', 'clear rocks', 'shot 0 0 0 0 9', 'steps 41'], 41,
     {1: {'pause': '40', 'shots': '-'}, 40: {'pause': '1', 'wave': '1'},
      41: {'pause': '0', 'wave': '2', 'r0': re.compile(f'L,{N},0,{N},{N},[0-3]'), 'r3': re.compile(f'L,{N},0,.*'),
           'r4': '-'}}),
    ('a pause on the step the ship returns', ['game g 1 0', 'clear rocks', 'set lost 1', 'steps 1'], 1,
     {1: {'pause': '40', 'lost': '0'}}),
    ('no pause while the ship is lost', ['game g 1 0', 'clear rocks', 'set lost 5', 'steps 1'], 1,
     {1: {'pause': '0', 'lost': '4'}}),
    ('full wave 4: six large', ['game f 1 0', 'clear rocks', 'set wave 3', 'set pause 1', 'steps 1'], 1,
     {1: {'wave': '4', 'r5': re.compile(f'L,{N},0,.*'), 'r6': '-'}}),
    ('gentle wave 1: three', ['game g 1 0', 'steps 1'], 1, {1: {'r2': re.compile('L,.*'), 'r3': '-'}}),
    ('a shot at the end of its life', D + [f'shot {300 * Q} {50 * Q} 0 0 1', 'steps 1'], 1, {1: {'shots': '-'}}),
]

if __name__ == '__main__':
    kids_edges.main(CASES)
