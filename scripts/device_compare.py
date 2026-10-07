#!/usr/bin/env python3
"""Compare the S3 bench's report with the host's expected hashes, and summarise the tick times.
Every game must match; the control (one tick changed) must not. Usage: device_compare.py EXPECTED REPORT"""
import sys

expected = dict(line.split() for line in open(sys.argv[1]) if line.strip())
rows = [line.split() for line in open(sys.argv[2]) if line.startswith("J ")]
bad, ticks, total, worst, worst_name, deepest = 0, 0, 0, 0, "", (0, 0)
for _, name, h, n, us_total, us_max, d_level, d_log, rc in rows:
    want = expected.get(name)
    if name.startswith("control"):
        ok = want is not None and h != want
        print(f"{'red' if ok else 'CONTROL DID NOT FAIL'}: {name} (device {h}, unchanged script {want})")
    else:
        ok = want == h and rc == "0"
        if not ok:
            print(f"MISMATCH {name}: device {h} rc {rc}, host {want}")
        ticks += int(n)
        total += int(us_total)
        if int(us_max) > worst:
            worst, worst_name = int(us_max), name
        deepest = (max(deepest[0], int(d_level)), max(deepest[1], int(d_log)))
    bad += not ok
missing = set(expected) - {r[1] for r in rows}
if missing:
    print("missing from the report:", " ".join(sorted(missing)))
    bad += len(missing)
games = sum(1 for r in rows if not r[1].startswith("control"))
print(f"{games} games, {ticks} ticks: rules {total / max(ticks, 1):.1f} us a tick on average, "
      f"{worst} us at worst ({worst_name}); deepest reads {deepest[0]} rows, log {deepest[1]}")
print("DEVICE PASS" if bad == 0 else f"DEVICE FAIL ({bad})")
sys.exit(1 if bad else 0)
