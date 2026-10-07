#!/usr/bin/env python3
"""The STU-32's games list: export each listed game at its pin and generate the build's part of it.

games.list (in the firmware), one game a line, `#` for comments:

    ID  REPO  PIN  LICENCE  MANIFEST

  ID        [a-z][a-z0-9_]{0,15}, unique
  REPO      a local clone's directory name, found in GAMES_REPOS (default: the firmware's parent)
  PIN       the full 40-hex commit; it must be in the clone and be an ancestor of its origin's
            default branch (a pushed commit), so a build never takes an unpublished one
  LICENCE   an SPDX expression; must equal the manifest's
  MANIFEST  the manifest's path in the game's repo at the pin

The manifest (KEY = VALUE lines, `#` comments), in the game's repo:

    name     what APPS shows: 1 to 5 printable ASCII characters
    folder   GAMES or TOOLS
    app      the C name of the game's app_def
    sources  the .c files and globs (*.c) to build, relative to the repo, no "..", no absolute path
    include  the directories its headers are in (same rules)
    flags    extra compile flags, from an allowed list (today: -ffp-contract=off); may be empty
    author, licence, source   who wrote it, its SPDX licence, where it came from (provenance)

Out, in OUT (default build/games): each game exported to OUT/ID/; games.mk (GAMES_C, GAMES_INC, and a
rule per game building its objects with its flags, -Werror=frame-larger-than=1024 always);
games.cmake (GAMES_SRCS, GAMES_INC, per-file COMPILE_OPTIONS); games.h (GAMES_DECLARE, GAMES_ENTRIES,
GAMES_FOLDERS for apps.c's registry); .stamp (the list's text, so a build re-runs when it changes).
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
KEYS = {"name", "folder", "app", "sources", "include", "flags", "author", "licence", "source"}


def fail(msg):
    sys.stderr.write("gen_games: " + msg + "\n")
    sys.exit(1)


def git(repo, *args, binary=False):
    r = subprocess.run(["git", "-C", repo, *args], capture_output=True)
    if r.returncode:
        return None
    return r.stdout if binary else r.stdout.decode()


def safe_path(p, what):
    if not p or p.startswith("/") or ".." in p.split("/") or "\\" in p:
        fail(f"{what}: unsafe path {p!r}")
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
        if not re.fullmatch(r"[A-Za-z0-9.+-]+( (AND|OR) [A-Za-z0-9.+-]+)*", lic):
            fail(f"{path}:{n}: {gid}: licence {lic!r} is not an SPDX expression")
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
    for k in KEYS - {"flags"}:
        if not m.get(k):
            fail(f"{gid}: manifest lacks {k}")
    if not re.fullmatch(r"[\x21-\x7e]( ?[\x21-\x7e]){0,4}", m["name"]) or len(m["name"]) > 5:
        fail(f"{gid}: name {m['name']!r} must be 1 to 5 printable ASCII characters")
    if m["folder"] not in FOLDERS:
        fail(f"{gid}: folder must be GAMES or TOOLS")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", m["app"]):
        fail(f"{gid}: app {m['app']!r} is not a C name")
    flags = m.get("flags", "").split()
    for fl in flags:
        if fl not in ALLOWED_FLAGS:
            fail(f"{gid}: flag {fl!r} is not allowed (allowed: {' '.join(sorted(ALLOWED_FLAGS))})")
    m["flags"] = flags
    m["sources"] = [safe_path(s, gid) for s in m["sources"].split()]
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
        if git(repo, "cat-file", "-e", g["pin"] + "^{commit}") is None:
            fail(f"{g['id']}: pin {g['pin']} is not in {repo}")
        head = git(repo, "symbolic-ref", "-q", "--short", "refs/remotes/origin/HEAD") or "origin/main"
        if git(repo, "merge-base", "--is-ancestor", g["pin"], head.strip()) is None:
            fail(f"{g['id']}: pin {g['pin'][:12]} is not on {head.strip()} (an unpushed commit?)")
        text = git(repo, "show", f"{g['pin']}:{g['manifest']}")
        if text is None:
            fail(f"{g['id']}: no manifest {g['manifest']} at the pin")
        m = parse_manifest(text, g["id"])
        if m["licence"] != g["licence"]:
            fail(f"{g['id']}: the list says {g['licence']}, the manifest {m['licence']}")
        if m["name"] in names:
            fail(f"{g['id']}: name {m['name']} is taken")
        names.add(m["name"])
        files = git(repo, "ls-tree", "-r", "--name-only", g["pin"]).split("\n")
        srcs = []
        for pat in m["sources"]:
            # a pattern matches in its own directory only (fnmatch's * would cross a /)
            hit = sorted(f for f in files if f.endswith(".c") and os.path.dirname(f) == os.path.dirname(pat)
                         and fnmatch.fnmatchcase(os.path.basename(f), os.path.basename(pat)))
            if not hit:
                fail(f"{g['id']}: sources {pat!r} match no .c file at the pin")
            srcs += [h for h in hit if h not in srcs]
        for inc in m["include"]:
            if not any(f.startswith(inc.rstrip("/") + "/") for f in files):
                fail(f"{g['id']}: include {inc!r} is no directory at the pin")
        plans.append((g, m, srcs, repo))
    # all checked: export and write
    os.makedirs(out, exist_ok=True)
    mk, cm, decl, entries, folders = [], [], [], [], []
    mk.append("# generated by gen_games.py from games.list: do not edit")
    cm.append("# generated by gen_games.py from games.list: do not edit")
    allc, allinc = [], []
    for g, m, srcs, repo in plans:
        d = os.path.join(out, g["id"])
        subprocess.run(["rm", "-rf", d], check=True)
        os.makedirs(d)
        paths = sorted({*srcs, *m["include"]})
        tar = git(repo, "archive", g["pin"], *paths, binary=True)
        if tar is None:
            fail(f"{g['id']}: git archive failed")
        subprocess.run(["tar", "-x", "-C", d], input=tar, check=True)
        rel = os.path.relpath(d, root)
        cs = [f"{rel}/{s}" for s in srcs]
        incs = [f"-I{rel}/{i}" for i in m["include"]]
        allc += cs
        allinc += incs
        flags = " ".join(m["flags"] + ["-Werror=frame-larger-than=1024"])
        mk.append(f"# {g['id']}: {m['name']} ({m['folder']}), {g['licence']}, {g['repo']} at {g['pin']}")
        mk.append(f"GAME_{g['id']}_C := {' '.join(cs)}")
        mk.append(f"build/games-o/{g['id']}/%.o: {rel}/%.c build/games/.stamp")
        mk.append("\t@mkdir -p $(dir $@)")
        mk.append(f"\t$(CC) $(CFLAGS) {flags} -Ifirmware $(GAMES_INC) -c $< -o $@")
        cm.append(f"# {g['id']}: {m['name']} ({m['folder']}), {g['licence']}, {g['repo']} at {g['pin']}")
        cm.append(f"set(GAME_{g['id']}_SRCS {' '.join('${REPO}/' + c for c in cs)})")
        cm.append(f"list(APPEND GAMES_SRCS ${{GAME_{g['id']}_SRCS}})")
        cm.append("if(NOT CMAKE_SCRIPT_MODE_FILE)")
        cm.append(f"    set_source_files_properties(${{GAME_{g['id']}_SRCS}} PROPERTIES COMPILE_OPTIONS \"{';'.join(flags.split())}\"")
        cm.append("                                OBJECT_DEPENDS \"${REPO}/build/games/.stamp\")")
        cm.append("endif()")
        decl.append(f"extern const app_def {m['app']}; /* {m['name']}, {g['id']} */")
        entries.append(f"&{m['app']}")
        folders.append(f"{FOLDERS[m['folder']]} /* {m['folder']} */")
    mk.insert(1, f"GAMES_INC := {' '.join(allinc)}")
    mk.insert(2, f"GAMES_C := {' '.join(allc)}")
    mk.insert(3, "GAMES_O := $(GAMES_C:build/games/%.c=build/games-o/%.o)")
    cm.insert(1, "set(GAMES_SRCS)")
    cm.insert(2, f"set(GAMES_INC {' '.join('${REPO}/' + i[2:] for i in allinc)})")
    open(os.path.join(out, "games.mk"), "w").write("\n".join(mk) + "\n")
    open(os.path.join(out, "games.cmake"), "w").write("\n".join(cm) + "\n")
    h = ["/* generated by gen_games.py from games.list: do not edit */", "#ifndef GAMES_H", "#define GAMES_H",
         "#define GAMES_DECLARE " + " ".join(decl), "#define GAMES_ENTRIES " + "".join(", " + e for e in entries),
         "#define GAMES_FOLDERS " + "".join(", " + f for f in folders), f"#define GAMES_N {len(entries)}", "#endif"]
    open(os.path.join(out, "games.h"), "w").write("\n".join(h) + "\n")
    open(os.path.join(out, ".stamp"), "w").write(open(lst).read())
    print(f"gen_games: {len(plans)} game(s): " + ", ".join(f"{m['name']} ({g['id']} at {g['pin'][:7]})" for g, m, _, _ in plans))


if __name__ == "__main__":
    main()
