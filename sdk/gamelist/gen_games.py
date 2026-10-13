#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# A copy of stu32-firmware's scripts/gen_games.py, the master (soroban's gate), at b31cfd9.
"""The STU-32's games list: export each listed game at its pin and generate the build's part of it.

games.list (in the firmware), one game a line, `#` for comments:

    ID  REPO  PIN  LICENCE  MANIFEST

  ID        [a-z][a-z0-9_]{0,15}, unique
  REPO      a local clone's directory name, found in GAMES_REPOS (default: the firmware's parent)
  PIN       the full 40-hex commit; it must be in the clone and be an ancestor of its origin's
            default branch (refs/remotes/origin/HEAD, which must exist): a pushed commit as of the
            clone's last fetch, so a build never takes an unpublished one
  LICENCE   one SPDX licence id (the list splits on blanks); must equal the manifest's
  MANIFEST  the manifest's path in the game's repo at the pin

The manifest (KEY = VALUE lines, `#` comments), in the game's repo:

    name     what APPS shows: 1 to 5 letters, digits or blanks (app_test checks the app_def's name agrees)
    folder   GAMES or TOOLS
    app      the C name of the game's app_def
    sources  the .c files and globs (*.c) to build, relative to the repo, no "..", no absolute path
    include  the directories its headers are in (same rules)
    flags    extra compile flags, from an allowed list (today: -ffp-contract=off); may be empty
    author, licence, source   who wrote it, its SPDX licence, where it came from (provenance)
    original  optional: for a port, the original's author, home and licence (abacus decision 62)

Without a clone (no git, or a build from an export), a game may be staged by hand: OUT/ID/ holding
the files at the pin, its manifest, and .pin with the pin; the generator then takes it as staged
(whether the pin was pushed cannot be checked there: the one who staged it vouches, and the run says
so). With a clone, the clone wins and the stage is rebuilt.

No symbolic link may be among what is exported (a source, the manifest, anything under an include
directory): it could pull a file from outside the game's repo into the build.

Out, in OUT (default build/games): each game exported to OUT/ID/ (with its manifest and .pin); games.mk
(GAMES_C, and a rule per game building its objects with its flags, -Wframe-larger-than=1024 always (an error by
CFLAGS' -Werror; games.cmake, the device's, -Werror=frame-larger-than=1024) and
-Wno-missing-field-initializers (apps.h v1.2's optional members),
and its own include directories only: a game's headers never reach the firmware's files or another
game's); games.cmake (GAMES_SRCS, per-file COMPILE_OPTIONS and INCLUDE_DIRECTORIES); games.h
(GAMES_DECLARE, GAMES_ENTRIES, GAMES_FOLDERS, GAMES_NAMES for apps.c's registry and app_test); .stamp (the
list's text, so a build re-runs when it changes).
Exits 1 with the reason on anything wrong; writes nothing then.

Usage: gen_games.py GAMES_LIST [OUT]
"""
import fnmatch
import os
import re
import subprocess
import sys

ALLOWED_FLAGS = {"-ffp-contract=off"}
FOLDERS = {"TOOLS": 0, "GAMES": 1}
KEYS = {"name", "folder", "app", "sources", "include", "flags", "author", "licence", "source", "original"}
OPTIONAL = {"flags", "original"}


def fail(msg):
    sys.stderr.write("gen_games: " + msg + "\n")
    sys.exit(1)


def git(repo, *args, binary=False):
    r = subprocess.run(["git", "-C", repo, *args], capture_output=True)
    if r.returncode:
        return None
    return r.stdout if binary else r.stdout.decode()


# every path the generator writes into games.mk, games.cmake or a command line: a name from a game's tree could
# otherwise carry make or shell syntax ($(shell ...), spaces, quotes) into the firmware's build (soroban's gate)
SAFE_PATH = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.-]*(/[A-Za-z0-9_][A-Za-z0-9_.-]*)*/?")


def safe_path(p, what, glob=False):
    if not p or p.startswith("/") or ".." in p.split("/") or "\\" in p:
        fail(f"{what}: unsafe path {p!r}")
    if not SAFE_PATH.fullmatch(p.replace("*", "x") if glob else p):
        fail(f"{what}: path {p!r} has a character outside A-Z a-z 0-9 _ . - /")
    return p


def read_list(path):
    games = []
    for n, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) != 5:
            fail(f"{path}:{n}: want ID REPO PIN LICENCE MANIFEST, got {len(f)} fields")
        gid, repo, pin, lic, manifest = f
        if not re.fullmatch(r"[a-z][a-z0-9_]{0,15}", gid):
            fail(f"{path}:{n}: bad id {gid!r}")
        if not re.fullmatch(r"[0-9a-f]{40}", pin):
            fail(f"{path}:{n}: {gid}: the pin must be a full 40-hex commit")
        if not re.fullmatch(r"[A-Za-z0-9.+-]+", lic):
            fail(f"{path}:{n}: {gid}: licence {lic!r} is not an SPDX licence id")
        if not re.fullmatch(r"[A-Za-z0-9._-]+", repo):
            fail(f"{path}:{n}: {gid}: repo must be a directory name")
        games.append(dict(id=gid, repo=repo, pin=pin, licence=lic, manifest=safe_path(manifest, gid)))
    ids = [g["id"] for g in games]
    if len(set(ids)) != len(ids):
        fail("an id is listed twice")
    return games


def parse_manifest(text, gid):
    m = {}
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        if "=" not in line:
            fail(f"{gid}: manifest line {n} is not KEY = VALUE")
        k, v = (s.strip() for s in line.split("=", 1))
        if k not in KEYS:
            fail(f"{gid}: manifest key {k!r} unknown")
        m[k] = v
    for k in KEYS - OPTIONAL:
        if not m.get(k):
            fail(f"{gid}: manifest lacks {k}")
    if not re.fullmatch(r"[A-Za-z0-9]( ?[A-Za-z0-9]){0,4}", m["name"]) or len(m["name"]) > 5:
        fail(f"{gid}: name {m['name']!r} must be 1 to 5 letters, digits or blanks")
    if m["folder"] not in FOLDERS:
        fail(f"{gid}: folder must be GAMES or TOOLS")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", m["app"]):
        fail(f"{gid}: app {m['app']!r} is not a C name")
    flags = m.get("flags", "").split()
    for fl in flags:
        if fl not in ALLOWED_FLAGS:
            fail(f"{gid}: flag {fl!r} is not allowed (allowed: {' '.join(sorted(ALLOWED_FLAGS))})")
    m["flags"] = flags
    m["sources"] = [safe_path(s, gid, glob=True) for s in m["sources"].split()]
    m["include"] = [safe_path(s, gid) for s in m["include"].split()]
    return m


def main():
    if len(sys.argv) < 2:
        fail("usage: gen_games.py GAMES_LIST [OUT]")
    lst = os.path.abspath(sys.argv[1])
    root = os.path.dirname(lst)
    out = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "build", "games"))
    repos = os.path.abspath(os.environ.get("GAMES_REPOS", os.path.dirname(root)))
    games = read_list(lst)
    plans = []
    names = set()
    for g in games:
        repo = os.path.join(repos, g["repo"])
        staged = os.path.join(out, g["id"])
        if os.path.isdir(repo):
            if git(repo, "cat-file", "-e", g["pin"] + "^{commit}") is None:
                fail(f"{g['id']}: pin {g['pin']} is not in {repo}")
            head = (git(repo, "symbolic-ref", "-q", "refs/remotes/origin/HEAD") or "").strip()
            if not head.startswith("refs/remotes/origin/"):
                fail(f"{g['id']}: {repo} has no refs/remotes/origin/HEAD (git remote set-head origin -a)")
            if git(repo, "merge-base", "--is-ancestor", g["pin"], head) is None:
                fail(f"{g['id']}: pin {g['pin'][:12]} is not on {head} (an unpushed commit?)")
            text = git(repo, "show", f"{g['pin']}:{g['manifest']}")
            if text is None:
                fail(f"{g['id']}: no manifest {g['manifest']} at the pin")
            tree = [l.split("\t", 1) for l in git(repo, "ls-tree", "-r", g["pin"]).split("\n") if "\t" in l]
            files = [f for _, f in tree]
            links = {f for meta, f in tree if meta.split()[0] == "120000"}
        else:
            pin_file = os.path.join(staged, ".pin")
            if not os.path.isfile(pin_file) or open(pin_file).read().strip() != g["pin"]:
                fail(f"{g['id']}: no clone at {repo}, and nothing staged at {staged} with .pin {g['pin'][:12]}")
            mpath = os.path.join(staged, g["manifest"])
            if not os.path.isfile(mpath):
                fail(f"{g['id']}: the stage at {staged} lacks its manifest {g['manifest']}")
            text = open(mpath).read()
            files = [os.path.relpath(os.path.join(d, f), staged) for d, _, fs in os.walk(staged) for f in fs]
            links = {f for f in files if os.path.islink(os.path.join(staged, f))}
            repo = None
        m = parse_manifest(text, g["id"])
        if m["licence"] != g["licence"]:
            fail(f"{g['id']}: the list says {g['licence']}, the manifest {m['licence']}")
        if m["name"] in names:
            fail(f"{g['id']}: name {m['name']} is taken")
        names.add(m["name"])
        srcs = []
        for pat in m["sources"]:
            # a pattern matches in its own directory only (fnmatch's * would cross a /)
            hit = sorted(f for f in files if f.endswith(".c") and os.path.dirname(f) == os.path.dirname(pat)
                         and fnmatch.fnmatchcase(os.path.basename(f), os.path.basename(pat)))
            if not hit:
                fail(f"{g['id']}: sources {pat!r} match no .c file at the pin")
            srcs += [safe_path(h, g["id"]) for h in hit if h not in srcs]   # (a name from the game's tree)
        for inc in m["include"]:
            if not any(f.startswith(inc.rstrip("/") + "/") for f in files):
                fail(f"{g['id']}: include {inc!r} is no directory at the pin")
        exported = set(srcs) | {g["manifest"]} | {f for f in files for inc in m["include"]
                                                  if f.startswith(inc.rstrip("/") + "/")}
        bad = sorted(exported & links)
        if bad:
            fail(f"{g['id']}: {bad[0]!r} is a symbolic link (nothing exported may be one)")
        plans.append((g, m, srcs, repo))
    # all checked: export and write
    os.makedirs(out, exist_ok=True)
    mk, cm, decl, entries, folders, names_c = [], [], [], [], [], []
    mk.append("# generated by gen_games.py from games.list: do not edit")
    cm.append("# generated by gen_games.py from games.list: do not edit")
    allc = []
    for g, m, srcs, repo in plans:
        d = os.path.join(out, g["id"])
        if repo is not None:
            subprocess.run(["rm", "-rf", d], check=True)
            os.makedirs(d)
            paths = sorted({*srcs, *m["include"], g["manifest"]})
            tar = git(repo, "archive", g["pin"], *paths, binary=True)
            if tar is None:
                fail(f"{g['id']}: git archive failed")
            subprocess.run(["tar", "-x", "-C", d], input=tar, check=True)
            open(os.path.join(d, ".pin"), "w").write(g["pin"] + "\n")
        rel = os.path.relpath(d, root)
        cs = [f"{rel}/{s}" for s in srcs]
        incs = [f"-I{rel}/{i}" for i in m["include"]]
        allc += cs
        # apps.h v1.2: an app_def written for 1.1 leaves the new optional members (save_size) to zero, as promised;
        # -Wextra would refuse its positional initializer, so not for games
        # the frame guard: on the host -Wframe-larger-than=1024, made an error by CFLAGS' -Werror (clang takes that
        # spelling too: primer's emcc builds these, abacus #5011/#5012); the device's (gcc only) -Werror=...=1024
        flags = " ".join(m["flags"] + ["-Wframe-larger-than=1024", "-Wno-missing-field-initializers"])
        cflags = " ".join(m["flags"] + ["-Werror=frame-larger-than=1024", "-Wno-missing-field-initializers"])
        mk.append(f"# {g['id']}: {m['name']} ({m['folder']}), {g['licence']}, {g['repo']} at {g['pin']}")
        mk.append(f"GAME_{g['id']}_C := {' '.join(cs)}")
        mk.append(f"GAME_{g['id']}_INC := {' '.join(incs)}")
        mk.append(f"build/games-o/{g['id']}/%.o: {rel}/%.c build/games/.stamp $(wildcard firmware/*.h)")
        mk.append("\t@mkdir -p $(dir $@)")
        mk.append(f"\t$(CC) $(CFLAGS) {flags} -Ifirmware $(GAME_{g['id']}_INC) -c $< -o $@")
        cm.append(f"# {g['id']}: {m['name']} ({m['folder']}), {g['licence']}, {g['repo']} at {g['pin']}")
        cm.append(f"set(GAME_{g['id']}_SRCS {' '.join('${REPO}/' + c for c in cs)})")
        cm.append(f"list(APPEND GAMES_SRCS ${{GAME_{g['id']}_SRCS}})")
        cm.append("if(NOT CMAKE_SCRIPT_MODE_FILE)")
        cm.append(f"    set_source_files_properties(${{GAME_{g['id']}_SRCS}} PROPERTIES COMPILE_OPTIONS \"{';'.join(cflags.split())}\"")
        cm.append(f"                                INCLUDE_DIRECTORIES \"{';'.join('${REPO}/' + i[2:] for i in incs)}\"")
        cm.append("                                OBJECT_DEPENDS \"${REPO}/build/games/.stamp\")")
        cm.append("endif()")
        decl.append(f"extern const app_def {m['app']}; /* {m['name']}, {g['id']} */")
        entries.append(f"&{m['app']}")
        folders.append(f"{FOLDERS[m['folder']]} /* {m['folder']} */")
        names_c.append(f'"{m["name"]}"')
    mk.insert(1, f"GAMES_C := {' '.join(allc)}")
    mk.insert(2, "GAMES_O := $(GAMES_C:build/games/%.c=build/games-o/%.o)")
    cm.insert(1, "set(GAMES_SRCS)")
    open(os.path.join(out, "games.mk"), "w").write("\n".join(mk) + "\n")
    open(os.path.join(out, "games.cmake"), "w").write("\n".join(cm) + "\n")
    h = ["/* generated by gen_games.py from games.list: do not edit */", "#ifndef GAMES_H", "#define GAMES_H",
         "#define GAMES_DECLARE " + " ".join(decl), "#define GAMES_ENTRIES " + "".join(", " + e for e in entries),
         "#define GAMES_FOLDERS " + "".join(", " + f for f in folders),
         "#define GAMES_NAMES " + "".join(", " + n for n in names_c), f"#define GAMES_N {len(entries)}", "#endif"]
    open(os.path.join(out, "games.h"), "w").write("\n".join(h) + "\n")
    open(os.path.join(out, ".stamp"), "w").write(open(lst).read())
    print(f"gen_games: {len(plans)} game(s): " + ", ".join(
        f"{m['name']} ({g['id']} at {g['pin'][:7]}{', staged: not checked as pushed' if r is None else ''})"
        for g, m, _, r in plans))


if __name__ == "__main__":
    main()
