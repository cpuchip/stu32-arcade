#!/bin/bash
# SPDX-License-Identifier: MIT
# The 0004 syntax pass (gcc -fsyntax-only, x86_64 host) on a WAMR tree: every arch file and aot_loader.c, then a
# planted write to the Xtensa map, which must fail on a const tree and compile on an unpatched one.
# Usage: syntax.sh TREE
set -u
T=$1; P=$(mktemp -d)
cd "$T" || exit 1
I="-Icore/iwasm/aot -Icore/iwasm/common -Icore/iwasm/include -Icore/shared/utils -Icore/shared/platform/linux -Icore/shared/platform/include -Icore/shared/mem-alloc -Icore"
G="gcc -fsyntax-only -std=gnu99 -DWASM_ENABLE_AOT=1 -DBH_PLATFORM_LINUX -Werror=discarded-qualifiers -Werror=incompatible-pointer-types $I"
ok=0; bad=0
for f in core/iwasm/aot/arch/aot_reloc_*.c core/iwasm/aot/aot_loader.c; do
    if $G $f 2> "$P/err"; then ok=$((ok+1)); else bad=$((bad+1)); echo "FAIL $f: $(head -2 "$P/err")"; fi
done
echo "syntax: $ok pass, $bad fail"
for f in aarch64 arm thumb; do $G -DBUILD_TARGET=\"t\" core/iwasm/aot/arch/aot_reloc_$f.c && echo "pass $f with BUILD_TARGET"; done
sed 's|    \*sym_num = sizeof(target_sym_map) / sizeof(SymbolMap);|    target_sym_map[0].symbol_addr = NULL; &|' \
    core/iwasm/aot/arch/aot_reloc_xtensa.c > "$P/plant_xtensa.c"
echo "plant lines: $(grep -c 'symbol_addr = NULL' "$P/plant_xtensa.c")"
if $G -Icore/iwasm/aot/arch "$P/plant_xtensa.c" 2> "$P/err"; then echo "plant: COMPILES"; else echo "plant: refused: $(grep -m1 error "$P/err" | sed 's/.*error: //')"; fi
rm -r "$P"
