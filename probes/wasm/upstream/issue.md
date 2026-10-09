Title: [BUG] ESP32-S3 with PSRAM: AOT code is called at an address that is not instruction memory

**Is it a security vulnerability?**
No. A valid .aot from wamrc crashes because the ESP-IDF port computes the wrong address for the module's code. Nothing in the .aot or the .wasm has to be crafted for it.

**Describe the bug**
On ESP32-S3 with PSRAM, the first call into an AOT module panics with IllegalInstruction.

With `CONFIG_ESP32S3_SPIRAM_SUPPORT` set, `core/shared/platform/esp-idf/shared_platform.cmake` turns on `WASM_MEM_DUAL_BUS_MIRROR`. `os_mmap()` then allocates the code from PSRAM and returns that pointer plus `MEM_DUAL_BUS_OFFSET`, which is `SOC_IROM_LOW - SOC_IROM_HIGH` (-0x2000000 on the S3). A PSRAM data address like 0x3C0779B4 becomes 0x3A0779B4, which is not instruction memory. I don't think any fixed offset can work here: heap pages in PSRAM are mapped on the data bus only, so there is nothing on the instruction bus to point at until those pages are mapped there too.

**Version**
WAMR-2.4.5 (25bd7eb) and main (ea5dd88). ESP-IDF v5.4.2.

**To Reproduce**
1. Build WAMR for esp32s3 with AOT enabled and PSRAM on (`CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`).
2. `wamrc --target=xtensa --cpu=esp32s3 --size-level=0 -o test.aot test.wasm` (I used `--size-level=0` because our module built with the default size level fails to load on this target for a different reason, which I can report separately.)
3. Load the .aot, instantiate it, and call any exported function.
4. `Guru Meditation Error: Core 0 panic'ed (IllegalInstruction)` with PC 0x3a0779b4, called from `invoke_native_internal` (aot_runtime.c).

**Expected behavior**
The AOT code runs from PSRAM.

**Actual Result**
The panic above, on the first call.

**Desktop (please complete the following information):**

- Arch: xtensa (ESP32-S3)
- Board: Seeed XIAO ESP32S3 (8 MB octal PSRAM)
- OS: ESP-IDF v5.4.2 (FreeRTOS)
- Version: WAMR-2.4.5 and main at ea5dd88

**Additional context**
With the mirror turned off, the code goes to `MALLOC_CAP_EXEC` internal RAM and runs, but that needs `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE` disabled (with it on, our firmware has 0 bytes of `MALLOC_CAP_EXEC` heap), and the code then takes internal RAM (91 KB for our module, more than our firmware has free). Mapping the PSRAM block's physical pages on the instruction bus with `esp_mmu_map(..., MMU_MEM_CAP_EXEC, ...)` works with memory protection left on. I'll open a PR with that change.
