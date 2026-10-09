Title: esp-idf: map PSRAM AOT code on the instruction bus

Fixes #ISSUE.

On ESP32-S3 with PSRAM, the dual bus mirror returned a PSRAM data address minus 0x2000000 as the address of AOT code. That address is not instruction memory, so the first call into an AOT module panics with IllegalInstruction (details in the issue).

What this changes, all on the `WASM_MEM_DUAL_BUS_MIRROR` path in `core/shared/platform/esp-idf/espidf_memmap.c`:

1. `os_mmap()` with `MMAP_PROT_EXEC` allocates a page-aligned PSRAM block and maps its physical pages on the instruction bus with `esp_mmu_map()`. It returns the instruction bus address.
2. `os_get_dbus_mirror()` returns the data bus address of the same block, so the loader copies and relocates the code there. The loader itself is unchanged.
3. `os_munmap()` unmaps the block and frees it.
4. The `wamr` component requires `esp_mm` for `esp_mmu_map.h`.

The mirror is only turned on for the S3 with PSRAM, so other targets, and the S3 without PSRAM, take the same path as before.

**Validation**

On a Seeed XIAO ESP32S3 (8 MB octal PSRAM), ESP-IDF v5.4.2, with memory protection left on (the ESP-IDF default):

- A 92 KB .aot (a small game, built with wamrc from this branch: `--target=xtensa --cpu=esp32s3 --size-level=0`) loads, instantiates, and runs 3,000 frames. Its output is identical to the same C code built natively for the S3. After loading, WAMR uses 5,236 bytes of internal RAM, since the code lives in PSRAM.
- The same .aot on main without this change panics with IllegalInstruction at PC 0x3a0779b4 on the first call.
- `product-mini/platforms/esp-idf` builds for esp32s3 (with and without PSRAM) and esp32c3, with no warnings from the changed file. I built it with `CONFIG_WAMR_ENABLE_LIBC_WASI=n`, because with WASI on that sample fails to build on ESP-IDF v5.4.2 in `posix.c`, on main without this change too.

No test is added: CI has no ESP-IDF job, and this path needs an S3 board with PSRAM. I'm happy to run anything else on the board if that helps review.

I wrote this with Claude's help (the commit carries a Co-Authored-By line); the testing above was done on the hardware listed.
