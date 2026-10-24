"""The edge cases' runner for 3720, MOLES and CATCH (oracle/<game>/edges.py; SALVO's edges.py the same way): each case
is set up by hand with the values it must print worked out by hand from the game's doc, not from either side.

  edges.py script        the trace script, every case after a '# case NAME' line
  edges.py check OUTPUT  checks a trace's output against the hand-written values; one line a case, exit 1 on a miss

A case is (name, script lines starting with `game`, the lines it must print, {step: {field: value or re pattern}}).
A list field (`rocks`, `holes`) also gives each item as field + index (r0..r23, h0..h8)."""
import re
import sys

LISTS = {'rocks': 'r', 'holes': 'h'}


def fields(line):
    out = {}
    for w in line.split()[1:]:
        k, v = w.split('=', 1)
        out[k] = v
        if k in LISTS:
            for i, c in enumerate(v.split('/')):
                out[f'{LISTS[k]}{i}'] = c
    return out


def main(cases):
    if sys.argv[1] == 'script':
        for name, lines, _, _ in cases:
            print(f'# case {name}')
            print('\n'.join(lines))
        return
    out = [l for l in open(sys.argv[2]).read().split('\n') if l.startswith('S ')]
    ok, at = True, 0
    for name, _, n, want in cases:
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
