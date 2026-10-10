Title: aot: keep the target symbol maps const

Each architecture's `target_sym_map` in `core/iwasm/aot/arch/` is declared `static SymbolMap[]` without `const`, and `get_target_symbol_map()` returns it as `SymbolMap *`. Nothing writes to it. The loader's two lookups (`get_native_symbol_by_name()` and `resolve_target_sym()`) read `symbol_name` and `symbol_addr`, and the PLT setup in aarch64, arm, thumb, riscv and x86_64 copies `symbol_addr` out.

On a microcontroller this decides where the table lives. Without `const` it goes to `.data`, which is copied into internal RAM at boot; with it, the table stays in flash. On an ESP32-S3 the Xtensa map is 1,000 bytes, and our firmware's static internal RAM went down by 1,008 bytes with this change (the table plus alignment). Internal RAM is the scarce resource on that chip: in our case those bytes decided whether the firmware booted with WAMR linked in.

The change makes the tables, the function's return type and the loader's two locals `const` in all ten arch files, `aot_reloc.h` and `aot_loader.c`. A write to a table no longer compiles.

No test is added, since the change adds `const` and nothing else and the compiler now refuses a write to a table. The reads are covered by what CI already runs: on x86_64, aarch64, arm, thumb and riscv every AOT load with code fills its PLT from the table (`init_plt_table()`), and the Ubuntu workflow runs the spec tests in AOT mode on x86_64.

**Validation**

- ESP32-S3 with 8 MB octal PSRAM, ESP-IDF v5.4.2, AOT only, with #5146, #5148 and a local change to `os_self_thread()` applied: `target_sym_map` moves from DRAM to the flash rodata segment, static internal RAM drops by 1,008 bytes, and a 143,096-byte .aot (a small game) loads and runs.
- x86_64 Linux: the same game's x86_64 .aot, run through our embedder, gives output identical to the game built natively, over 22,641 frames.
- gcc 15.2 `-fsyntax-only` with `-Werror=discarded-qualifiers -Werror=incompatible-pointer-types` passes on all ten arch files and `aot_loader.c` (aarch64, arm and thumb need `-DBUILD_TARGET`, as on main), and a planted write to the Xtensa table fails with "assignment of member 'symbol_addr' in read-only object". This is a syntax pass on an x86_64 host, so code under another architecture's own `#if`s was not compiled.
- clang-format with the repo's `.clang-format` (23.1, from wasi-sdk; CI uses 21) reports nothing on the twelve changed files, and a misformatted line planted in `aot_loader.c` is reported.

I wrote this with Claude's help (the commit carries a Co-Authored-By line); the testing above was done on the hardware listed.
