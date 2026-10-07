# stu32-arcade

Games for the STU-32 calculator's APPS menu (working name arcade). First: a port of Babal by
Christian Mauduit (gitlab.com/liberecofr/babal, the Unlicense), its rules (rust/babalcore) in C.
Babal's music and fonts are third parties' and are never ported.

## Stage 1: the rules, judged against babalcore

    make test     # babalcore's own tests (unit and doc), ported: tests/babal_test.c
    make oracle   # fetch babal at the pinned commit, build babalcore's reference driver,
                  # compare pins, levels and games byte for byte, plant faults (each must go red)

`make oracle` needs git, python3, and cargo (for the reference only). The contract, what is not a
one-to-one translation, and babalcore's quirks kept on purpose: docs/stage1-rules.md.

    babal/        the port: one C file per babalcore module, babal.h
    tests/        babalcore's tests in C
    oracle/       fetch-babal.sh (pin fa985cd), ref/ (Rust driver), driver.c (C driver),
                  gen_scripts.py (game scripts), run.sh (the gate)
