#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare the S3 app bench with the host's frame hashes; summarise tick and draw times.
Every script must match; the control must not. Usage: device_app_compare.py EXPECTED REPORT"""
import sys

expected = dict(line.split() for line in open(sys.argv[1]) if line.strip())
lines = open(sys.argv[2]).read().split("\n")
rows = [l.split() for l in lines if l.startswith("A ")]
bad, frames, tick_t, draw_t, tick_m, draw_m = 0, 0, 0, 0, 0, 0
for _, name, h, n, tt, tm, dt, dm in rows:
    want = expected.get(name)
    if name.startswith("control"):
        ok = want is not None and h != want
        print(f"{'red' if ok else 'CONTROL DID NOT FAIL'}: {name}")
    else:
        ok = h == want
        if not ok:
            print(f"MISMATCH {name}: device {h}, host {want}")
        frames += int(n)
        tick_t += int(tt); draw_t += int(dt)
        tick_m = max(tick_m, int(tm)); draw_m = max(draw_m, int(dm))
    bad += not ok
missing = set(expected) - {r[1] for r in rows}
bad += len(missing)
if missing:
    print("missing:", " ".join(sorted(missing)))
g = next((l.split() for l in lines if l.startswith("G ")), None)
print(f"{len(rows) - 1} scripts, {frames} frames: tick {tick_t / max(frames, 1):.0f} us average, {tick_m} us worst; "
      f"draw {draw_t / max(frames, 1):.0f} us average, {draw_m} us worst")
if g:
    print(f"row generation: 100 rows at once {g[1]} us; one at a time {int(g[2]) / 100:.0f} us average, {g[3]} us worst")
print("DEVICE PASS" if bad == 0 else f"DEVICE FAIL ({bad})")
sys.exit(1 if bad else 0)
