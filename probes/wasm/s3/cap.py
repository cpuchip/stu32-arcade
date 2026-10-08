# SPDX-License-Identifier: MIT
# Read the probe's console until PROBE DONE, printing every line. On FAIL or a panic, keep reading
# 4 s more so the registers and the backtrace are kept (the first version stopped at the panic line).
import sys, time, serial
port, timeout = sys.argv[1], float(sys.argv[2])
end, stop_at, rc = time.time() + timeout, None, 2
with serial.Serial(port, 115200, timeout=0.5) as s:
    while time.time() < end and (stop_at is None or time.time() < stop_at):
        line = s.readline().decode("utf-8", "replace").rstrip("\r\n")
        if not line:
            continue
        print(line, flush=True)
        if line == "PROBE DONE":
            sys.exit(0)
        if stop_at is None and (line.startswith("FAIL") or "Guru Meditation" in line or "abort()" in line
                                or "Stack canary" in line):
            stop_at, rc = time.time() + 4, 1
print("TIMEOUT" if rc == 2 else "STOPPED AFTER A FAILURE", flush=True)
sys.exit(rc)
