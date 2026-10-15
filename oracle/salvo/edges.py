"""The rules at their edges (docs/salvo.md, "The oracle" 3), each case set up by hand with what it must print worked
out by hand from the doc, not from either implementation.

  edges.py script        the trace script, every case after a '# case NAME' line
  edges.py check OUTPUT  checks a trace's output against the hand-written values; prints one line a case, and exits 1
                         on a miss

A slot's place: x 108 + 24 c, y 20 + 16 r at fx 108 and fy 20, slot i = 8 r + c. Each case starts from P: a game,
the entry over (fy 20), the dive counter far off (99), and the sway's count at 1 (so the step leaves fx alone)."""
import re
import sys

P = ['game 1 0', 'set fy 20', 'set dive 99', 'set sway 1']
ALL_DEAD = [f'alien {i} X' for i in range(32)]


def dead_but(*keep):
    return [f'alien {i} X' for i in range(32) if i not in keep]


# (name, script lines, the lines it must print, {step: {field: value or /regex/}})
CASES = [
    ('a shot at the top line', P + ['shot 100 18', 'shot 300 19', 'steps 1'], 1,
     {1: {'shots': '300,7'}}),                          # 18 - 12 = 6, bottom 12: gone; 19 -> 7, bottom 13: kept
    ('a third shot refused', P + ['shot 50 200', 'shot 60 200', 'down 8', 'steps 1'], 1,
     {1: {'shots': '50,188;60,188', 'cool': '0'}}),
    ('the cooldown at 1 fires', P + ['set cool 1', 'down 8', 'steps 2'], 2,
     {1: {'shots': '199,222', 'cool': '6'}, 2: {'shots': '199,210', 'cool': '5'}}),
    ('the cooldown at 2 does not', P + ['set cool 2', 'down 8', 'steps 1'], 1,
     {1: {'shots': '-', 'cool': '1'}}),
    ('the sway turns at 208', P + ['set sway 3', 'set fx 208', 'steps 1'], 1,
     {1: {'fx': '204', 'fdir': '-1', 'sway': '4'}}),
    ('the sway turns at 8', P + ['set sway 3', 'set fx 8', 'set fdir -1', 'steps 1'], 1,
     {1: {'fx': '12', 'fdir': '1'}}),
    ('the sway reaches 208 without turning', P + ['set sway 3', 'set fx 204', 'steps 1'], 1,
     {1: {'fx': '208', 'fdir': '1'}}),
    ('a dive from the one column left', P + dead_but(2, 26) + ['set dive 1', 'steps 1'], 1,
     {1: {'dive': '40', 'a26': re.compile(r'D156,68,0,0,(6|7|8|9|1[0-5])'), 'a2': 'S'}}),
    ('no dive with none in a slot', P + dead_but(0) + ['alien 0 D100,100,1,0,-1', 'set dive 1', 'steps 1'], 1,
     {1: {'dive': '40', 'a0': 'D100,105,2,0,-1'}}),
    ('vx truncated toward zero', P + ['set sx 0', 'alien 26 D26,100,0,0,-1', 'alien 27 D13,100,0,0,-1',
                                      'alien 25 D12,100,0,0,-1', 'steps 1'], 1,
     {1: {'a26': 'D24,105,1,-2,-1', 'a27': 'D12,105,1,-1,-1', 'a25': 'D12,105,1,0,-1'}}),
    ('a diver fires at y 179', P + ['alien 26 D100,174,7,0,7', 'steps 1'], 1,
     {1: {'a26': 'D100,179,8,0,-1', 'eshots': '106,195'}}),
    ('a diver at y 180 does not', P + ['alien 26 D100,175,7,0,7', 'steps 1'], 1,
     {1: {'a26': 'D100,180,8,0,7', 'eshots': '-'}}),
    ('a diver at 239 stays, at 240 returns', P + ['alien 26 D100,234,1,0,-1', 'alien 27 D300,235,1,0,-1', 'steps 1'], 1,
     {1: {'a26': 'D100,239,2,0,-1', 'a27': 'R180,-10'}}),
    ('returns: exactly, past, short', P + ['alien 26 R156,64', 'alien 27 R180,66', 'alien 25 R132,63', 'steps 1'], 1,
     {1: {'a26': 'S', 'a27': 'S', 'a25': 'R132,67'}}),
    ('a kill and a lost ship on one step', P + ['shot 112 30', 'eshot 199 224', 'steps 1'], 1,
     {1: {'a0': 'X', 'score': '150', 'lives': '2', 'ship': '40', 'shots': '-', 'eshots': '-'}}),
    ('the last alien shot while diving', P + dead_but(31) + ['alien 31 D150,100,1,0,-1', 'shot 155 125', 'steps 1'], 1,
     {1: {'a31': 'X', 'score': '100', 'wavet': '30'}}),
    ('an extra ship at 10,000', P + ['set score 9950', 'shot 112 80', 'steps 1'], 1,
     {1: {'a24': 'X', 'score': '10000', 'lives': '4', 'next': '20000'}}),
    ('no extra ship past 5', P + ['set score 9950', 'set lives 5', 'shot 112 80', 'steps 1'], 1,
     {1: {'score': '10000', 'lives': '5', 'next': '20000'}}),
    ('a ram', P + ['alien 26 D190,216,1,0,-1', 'steps 1'], 1,
     {1: {'a26': 'X', 'lives': '2', 'ship': '40', 'score': '0'}}),
    ('the last ship', P + ['set lives 1', 'eshot 199 224', 'steps 60'], 41,
     {1: {'lives': '0', 'ship': '40'}, 40: {'ship': '1', 'over': '0'}, 41: {'ship': '0', 'over': '1'}}),
    ('the clamps', P + ['set sx 2', 'down 4', 'steps 1', 'up 4', 'set sx 383', 'down 6', 'steps 1'], 2,
     {1: {'sx': '0'}, 2: {'sx': '385'}}),
    ('a tap shorter than a step', P + ['down 4', 'up 4', 'steps 2'], 2,
     {1: {'sx': '187'}, 2: {'sx': '187'}}),
    ('both directions', P + ['down 4', 'down S6', 'steps 1'], 1,
     {1: {'sx': '192'}}),
    ('an enemy shot and a diver on the ship', P + ['eshot 199 224', 'alien 26 D190,216,1,0,-1', 'steps 1'], 1,
     {1: {'lives': '2', 'ship': '40', 'a26': 'S', 'eshots': '-'}}),     # the shot first: the diver goes home, alive
    ('two divers on the ship', P + ['alien 25 D192,216,1,0,-1', 'alien 26 D195,216,1,0,-1', 'steps 1'], 1,
     {1: {'lives': '2', 'a25': 'X', 'a26': 'S'}}),
    ('the sixth enemy shot', P + [f'eshot 10 {y}' for y in (20, 30, 40, 50, 60)]
     + ['alien 26 D300,100,7,0,7', 'steps 1'], 1,
     {1: {'eshots': '10,26;10,36;10,46;10,56;10,66;306,121', 'a26': 'D300,105,8,0,-1'}}),
    ('a seventh refused, not tried again', P + [f'eshot 10 {y}' for y in (20, 30, 40, 50, 60)]
     + ['eshot 10 234', 'alien 26 D300,100,7,0,7', 'steps 2'], 2,    # the 6th leaves at 240 after the refusal
     {1: {'a26': 'D300,105,8,0,7', 'eshots': '10,26;10,36;10,46;10,56;10,66'},
      2: {'a26': 'D297,110,9,-3,7', 'eshots': '10,32;10,42;10,52;10,62;10,72'}}),   # 8th move: vx -107 / 12 -> -3
    ("a diver's x clamps", P + ['alien 26 D386,100,1,3,-1', 'alien 27 D1,100,1,-3,-1', 'steps 1'], 1,
     {1: {'a26': 'D387,105,2,3,-1', 'a27': 'D0,105,2,-3,-1'}}),
    ('a shot meets a diver head-on', P + ['alien 26 D100,100,1,0,-1', 'shot 105 127', 'steps 2'], 2,
     {1: {'a26': 'D100,105,2,0,-1', 'shots': '105,115'}, 2: {'a26': 'X', 'score': '100', 'shots': '-'}}),
    ('a shot meets a returning alien', P + dead_but(31) + ['alien 31 R276,30', 'shot 280 56', 'steps 2'], 2,
     {1: {'a31': 'R276,34', 'shots': '280,44'}, 2: {'a31': 'X', 'score': '50'}}),
    ('a shot over two aliens hits the lower', P + ['shot 280 40', 'steps 1'], 1,
     {1: {'a15': 'X', 'a7': 'S', 'score': '80', 'shots': '-'}}),          # its box 28-46 meets rows at 20 and 36
    ('a shot leaving at the top still hits', P + dead_but(31) + ['alien 31 R276,-7', 'shot 280 18', 'steps 1'], 1,
     {1: {'a31': 'X', 'score': '50', 'shots': '-'}}),
    ("an enemy shot by the ship box's corner", P + ['eshot 192 224', 'steps 1'], 1,
     {1: {'lives': '3', 'ship': '0', 'eshots': '192,230'}}),
    ('the clock', P + ['set acc 49', 'tick 100', 'tick 250', 'tick 0', 'tick 1'], 5,
     {1: {'acc': '99'}, 2: {'acc': '49'}, 3: {'acc': '99'}, 4: {'acc': '49'}, 5: {'acc': '0'}}),
]


def fields(line):
    out = {}
    for w in line.split()[1:]:
        k, v = w.split('=', 1)
        if k == 'aliens':
            for i, c in enumerate(v.split('/')):
                out[f'a{i}'] = c
        else:
            out[k] = v
    return out


def main():
    if sys.argv[1] == 'script':
        for name, lines, _, _ in CASES:
            print(f'# case {name}')
            print('\n'.join(lines))
        return
    out = [l for l in open(sys.argv[2]).read().split('\n') if l.startswith('S ')]
    ok, at = True, 0
    for name, _, n, want in CASES:
        got, at = out[at:at + n], at + n
        bad = [] if len(got) == n and (n == 0 or fields(got[0])['step'] == '1') else [f'{len(got)} lines, want {n}']
        for step, fs in want.items():
            if step > len(got):
                continue
            have = fields(got[step - 1])
            for k, v in fs.items():
                if not (v.fullmatch(have.get(k, '')) if isinstance(v, re.Pattern) else have.get(k) == v):
                    bad.append(f'step {step} {k}={have.get(k)}, want {v.pattern if isinstance(v, re.Pattern) else v}')
        print(f"{'ok  ' if not bad else 'MISS'} {name}" + ('' if not bad else ': ' + '; '.join(bad)))
        ok = ok and not bad
    if at != len(out):
        print(f'MISS {len(out) - at} state lines past the last case')
        ok = False
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
