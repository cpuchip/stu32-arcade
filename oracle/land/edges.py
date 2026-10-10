"""The landing rule at its edges (docs/land.md, the oracle's check 3): each case is a hand-made terrain and a ship
placed so that its first step makes contact, with the end written here by hand from the blueprint, not computed.

  edges.py script     the cases as a trace script (one 'case' and 'step 1' each)
  edges.py expect     the end each case must give, in order
"""
import sys

Q = 65536
G = 655
VY, VX = 26214, 13107
# a pad from 100 to 200 px at y 200, the ground falling away on both sides
PAD = '0,226 100,200 200,200 400,226'
# a peak 36 px high and 20 px wide, narrower than the ship: at x 150 its tip is 4 px above the ship's reference point
PEAK = '0,226 140,226 150,190 160,226 400,226'
# flat at 201 px to x 160, then rising 1 px over 3 px
SLOPE = '0,201 160,201 163,200 400,200'
FOOT = 201 * Q - 21846


def case(x, y, vx, vy, a, terrain):
    """the ship's state before the step; the step adds G to vy and moves by vx and vy"""
    return f'case {x} {y} {vx} {vy} {a} {terrain}'


def on_pad(vy_contact, vx=0, a=0, right_foot_px=150 + 6, deeper=0):
    """placed so that after the step the feet are exactly on the pad's line (y 200), or deeper px below it"""
    y_after = (200 - 6 + deeper) * Q
    x_after = (right_foot_px - 6) * Q
    return case(x_after - vx, y_after - vy_contact, vx, vy_contact - G, a, PAD)


CASES = [
    ('a slow, upright landing in the middle', on_pad(1000), 'land 5'),
    ('vy exactly VY', on_pad(VY), 'land 5'),
    ('vy one over VY', on_pad(VY + 1), 'crash 0'),
    ('vx exactly VX', on_pad(1000, vx=VX), 'land 5'),
    ('vx one over VX', on_pad(1000, vx=VX + 1), 'crash 0'),
    ('vx exactly -VX', on_pad(1000, vx=-VX), 'land 5'),
    ('vx one under -VX', on_pad(1000, vx=-VX - 1), 'crash 0'),
    ('upright, 2 px into the pad', on_pad(1000, deeper=2), 'land 5'),
    ('one step right of upright (a = 1), 2 px in', on_pad(1000, a=1, deeper=2), 'crash 0'),
    ('one step left of upright (a = 23), 2 px in', on_pad(1000, a=23, deeper=2), 'crash 0'),
    ('the right foot exactly at the pad end', on_pad(1000, right_foot_px=200), 'land 5'),
    ('the right foot one pixel past the pad end', on_pad(1000, right_foot_px=201), 'crash 0'),
    ('the left foot exactly at the pad start', on_pad(1000, right_foot_px=100 + 12), 'land 5'),
    ('the left foot one pixel before the pad start', on_pad(1000, right_foot_px=100 + 11), 'crash 0'),
    # the peak's tip inside the outline while every outline point is above the ground: the vertex test alone
    ('a peak between the feet, every point above the ground', case(150 * Q, 194 * Q - 1000, 0, 1000 - G, 0, PEAK),
     'crash 0'),
    # the same ship 12 px higher: the tip is below it, so it flies on (a step, then the next is not run)
    ('the same ship clear of the peak', case(150 * Q, 182 * Q - 1000, 0, 1000 - G, 0, PEAK), 'flying'),
    # rising: the feet 3 px under the pad's line after the step, moving up; |vy| <= VY holds both ways
    ('rising at exactly VY, the feet under the pad', on_pad(-VY, deeper=3), 'land 5'),
    ('rising at one over VY, the feet under the pad', on_pad(-VY - 1, deeper=3), 'crash 0'),
    # the ground's rounding: on SLOPE, at x 161 px the ground is 201 px - 1/3 px, which C's / gives as
    # 201 Q - 21,845 and a floor would give as 201 Q - 21,846. The right foot (x 161) put at 201 Q - 21,846 is
    # above the ground as the blueprint has it; the left foot (x 149) is over the flat at 201 px, above it too.
    ('a foot 1/65,536 px above a rising slope', case(155 * Q, FOOT - 6 * Q - 1000, 0, 1000 - G, 0, SLOPE), 'flying'),
    ('the same foot exactly on it', case(155 * Q, FOOT + 1 - 6 * Q - 1000, 0, 1000 - G, 0, SLOPE), 'crash 0'),
]


def main():
    if sys.argv[1] == 'script':
        for name, line, _ in CASES:
            print(f'# {name}')
            print(line)
            print('step 1')
            print('end?')
    elif sys.argv[1] == 'expect':
        for _, _, end in CASES:
            print(end)


if __name__ == '__main__':
    main()
