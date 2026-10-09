Title: xtensa: fix L32R relocation lost to strict aliasing

Fixes #5147.

`put_imm16_to_addr()` in `core/iwasm/aot/arch/aot_reloc_xtensa.c` patches the 16-bit immediate of an L32R instruction. It copied the aligned word into an `int8` buffer, stored the immediate through an `int16` pointer, and read the buffer back through an `int32` pointer. That breaks C's strict aliasing rule, and GCC at -O2 (ESP-IDF v5.4.2) uses it: in the compiled `apply_relocation()`, the whole branch for an immediate that does not cross a word boundary is gone, so nothing is written. Those L32R instructions keep the unrelocated offset of -1 and load the first word of their own function, which is the 0x81006136 in the issue.

`--size-level=0` hides it because that build has no L32R relocations at all; its literals sit inside `.text`.

The fix copies the immediate with `memcpy` into a `uint32` array and writes the result back with 32-bit stores only, which instruction memory needs. The old code also wrote the word-crossing case back with `memcpy`, which can turn into byte stores.

**Validation**

On a Seeed XIAO ESP32S3 (8 MB octal PSRAM), ESP-IDF v5.4.2, wamrc built from main against espressif/llvm-project `xtensa_release_18.1.2`:

- Before this change, after loading the 41-byte module from the issue, the instruction bytes still read `ff ff` where the new immediate (-3) belongs. With it, `add(2, 3)` returns 5.
- The same `add` module returns 5 with the default size level and with `--size-level=0`, with the AOT code in internal RAM (`MALLOC_CAP_EXEC`, memory protection off) and in PSRAM (with #5146 applied).
- A larger module (110,960-byte .aot, a small game) built with the default size level now loads and runs 3,000 frames, and its output is identical to the same C code built natively for the S3. Before this change it crashed on the first call.
- The disassembly of `put_imm16_to_addr()` now has the 32-bit store back to the instruction on both paths.
- `product-mini/platforms/esp-idf` builds for esp32 and esp32s3 with AOT on (and WASI off, as in #5146), with no warnings from the changed file.

No test is added: the function is static and Xtensa-only, CI has no Xtensa job, and the failure only shows with GCC's optimizer on the target. I'm happy to run anything else on the board.

I wrote this with Claude's help (the commit carries a Co-Authored-By line); the testing above was done on the hardware listed.
