#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Write the oracle's game scripts: seeded play-like runs plus hand-written edges.

Script words (read by oracle/ref and oracle/driver.c alike):
  seed U64 | skill NAME | steer_factor F | best I32 | stick F|none | sens MOUSE KBD
  wait NS | at NS | L 0|1 [INTENSITY [ID]] | R 0|1 [INTENSITY [ID]] | J | M PX | tick NS | tickd DELTA
(at sets the clock: tools/apphost.c writes it, since the app's keys carry their own times)
At most two key ids per side: babalcore sums a side's keys in HashMap order, and two terms
commute while three need not (quirk 3 in docs/stage1-rules.md).
Usage: gen_scripts.py OUTDIR [N_SEEDS] [SCRIPTS_PER_SEED]
"""
import os
import random
import sys

SKILLS = ["noob", "easy", "medium", "hard", "expert"]


def play(rng, seed, skill, ticks):
    out = [f"seed {seed}", f"skill {skill}"]
    if rng.random() < 0.3:
        out.append(f"steer_factor {rng.choice(['0.5', '1.5', '2', '0', '-1'])}")
    if rng.random() < 0.5:
        out.append(f"best {rng.choice([1, 500, 1500, 4000, 20000])}")
    if rng.random() < 0.3:
        out.append(f"stick {rng.choice(['none', '0.05', '0.3', '0.0001'])}")
    if rng.random() < 0.2:
        out.append(f"sens {rng.choice(['1', '2.5'])} {rng.choice(['1000', '400', '2500'])}")
    held = {}
    for _ in range(ticks):
        for _ in range(rng.choice([0, 0, 0, 1, 1, 2])):
            r = rng.random()
            if r < 0.35:
                side = rng.choice("LR")
                # mostly the side's default id; sometimes a second one (3 on the left, 4 on the right)
                key = (side, None) if rng.random() < 0.8 else (side, 3 if side == "L" else 4)
                held[key] = not held.get(key, False)
                intensity = rng.choice(['0.3', '0.75', '1', '1.5', '0', '-0.2']) if rng.random() < 0.25 else '1'
                if key[1] is not None:
                    out.append(f"{side} {int(held[key])} {intensity} {key[1]}")
                elif held[key] and intensity != '1':
                    out.append(f"{side} 1 {intensity}")
                else:
                    out.append(f"{side} {int(held[key])}")
            elif r < 0.6:
                out.append("J")
            elif r < 0.7:
                out.append(f"M {rng.choice(['-40', '-3.5', '2', '25', '0.125'])}")
            out.append(f"wait {rng.randrange(0, 8_000_000)}")
        r = rng.random()
        if r < 0.97:
            ns = rng.randrange(14_000_000, 19_000_000)
        elif r < 0.985:
            ns = 0
        else:
            ns = rng.randrange(50_000_000, 400_000_000)
        out.append(f"tick {ns}")
    return out


def edges():
    s = {}
    s["idle"] = ["seed 7", "skill medium"] + ["tick 16666667"] * 600
    s["jump-spam"] = ["seed 8", "skill hard"] + ["J", "tick 16666667"] * 900
    hold = ["seed 9", "skill expert", "R 1"]
    for i in range(1500):
        hold += ["J"] if i % 40 == 0 else []
        hold += ["tick 16666667"]
    s["hold-right"] = hold
    s["deltas"] = (["seed 10", "skill easy", "tick 0", "tick 1000000000", "tickd -0.5", "tickd 1e-12",
                    "tickd 0", "R 1", "tick 16666667", "tickd 2.5", "R 0", "L 1", "tickd 0.001"]
                   + ["tick 16666667"] * 300
                   # Bounded extremes: a 30 s step and a negative step with a key held. Past these,
                   # babalcore itself hangs (quirks 7 and 8 in docs/stage1-rules.md).
                   + ["tickd 30", "tick 16666667", "tickd -0.5"] + ["tick 16666667"] * 60)
    s["nan-mouse"] = ["seed 13", "skill medium"] + ["tick 16666667"] * 30 + ["M NaN"] + ["tick 16666667"] * 30
    s["nan-delta"] = ["seed 14", "skill medium"] + ["tick 16666667"] * 30 + ["tickd NaN"] + ["tick 16666667"] * 30
    s["sticky"] = (["seed 11", "skill medium", "stick none", "J", "tick 16666667", "J", "wait 1000", "tick 16666667",
                    "stick 0.5", "J", "tick 1", "tick 1", "wait 600000000", "tick 1"] + ["tick 16666667"] * 200)
    # Noob to its finish (30 s of play on the best row): steer away from falls by brute repetition.
    noob = ["seed 12", "skill noob", "best 300"]
    for i in range(9000):
        if i % 90 == 0:
            noob += ["L 0", "R 1"] if (i // 90) % 2 else ["R 0", "L 1"]
        if i % 25 == 0:
            noob.append("J")
        noob.append("tick 16666667")
    s["noob-finish"] = noob
    return s


def main():
    outdir = sys.argv[1]
    n_seeds = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    per_seed = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    os.makedirs(outdir, exist_ok=True)
    rng = random.Random(20261007)
    seeds = [0, 123, 2**64 - 1] + [rng.randrange(2**64) for _ in range(max(0, n_seeds - 3))]
    for seed in seeds[:n_seeds]:
        for k in range(per_seed):
            skill = SKILLS[k % len(SKILLS)]
            lines = play(rng, seed, skill, rng.randrange(600, 3600))
            with open(os.path.join(outdir, f"play-{seed}-{k}.txt"), "w") as f:
                f.write("\n".join(lines) + "\n")
    # Three long games (30,000 ticks, about 8 minutes of play each).
    for k, skill in enumerate(["noob", "easy", "expert"]):
        seed = rng.randrange(2**64)
        with open(os.path.join(outdir, f"long-{skill}-{seed}.txt"), "w") as f:
            f.write("\n".join(play(rng, seed, skill, 30000)) + "\n")
    for name, lines in edges().items():
        with open(os.path.join(outdir, f"edge-{name}.txt"), "w") as f:
            f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
