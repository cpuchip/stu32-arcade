#!/usr/bin/env python3
"""Read the bench's serial console until one whole report (BENCH BEGIN .. BENCH DONE) is seen.
Usage: capture.py PORT LOG TIMEOUT_S"""
import sys
import time

import serial

port, log, timeout = sys.argv[1], sys.argv[2], float(sys.argv[3])
end = time.time() + timeout
lines, report = [], None
with serial.Serial(port, 115200, timeout=1) as s, open(log, "w") as f:
    current = None
    while time.time() < end:
        raw = s.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", "replace").rstrip("\r\n")
        f.write(line + "\n")
        f.flush()
        if line.startswith("ran "):
            print(line, flush=True)
        if line.startswith("BENCH BEGIN"):
            current = [line]
        elif current is not None:
            current.append(line)
            if line == "BENCH DONE":
                report = current
                break
if report is None:
    print("no whole report before the timeout")
    sys.exit(1)
with open(log + ".report", "w") as f:
    f.write("\n".join(report) + "\n")
print(f"report: {len(report) - 2} jobs")
