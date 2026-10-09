Title: [BUG] Xtensa AOT built with the default size level crashes on the first call (LoadProhibited)

**Is it a security vulnerability?**
No. A valid .aot from wamrc crashes on its first call. Nothing in the .wasm has to be crafted for it; a 41-byte module that adds two numbers is enough.

**Describe the bug**
On ESP32-S3, an .aot built by wamrc with the default size level crashes on the first call into any function, with LoadProhibited and EXCVADDR 0x81006136. The same module built with `--size-level=0` runs.

The difference I can see is the literal pool. With the default size level, wamrc's object keeps a separate `.literal` section (8 bytes for this module), and each function reads it with `l32r`; the first one loads the address of `.aot_stack_sizes` for the stack check. With `--size-level=0` the literals are placed inside `.text` and there is no `.literal` section.

The faulting address is the function's own first bytes. `36 61 00` is `entry a1, 48` and `81` is the first byte of the `l32r` after it; read as one little-endian word that is 0x81006136. So the first `l32r` seems to load from the start of the function's code instead of from the literal pool. From `--format=object` of the same module:

```
00000000 <aot_func#0>:
   0:	006136        	entry	a1, 48
   3:	ffff81        	l32r	a8, 0 <aot_func#0>
   6:	0888      	l32i.n	a8, a8, 0
```

with `R_XTENSA_SLOT0_OP .literal + 0` at offset 3 and `R_XTENSA_32 .aot_stack_sizes + 0` at `.literal` offset 0.

**Version**
main at ea5dd88, with wamrc built from the same commit against espressif/llvm-project `xtensa_release_18.1.2` (344b928, the branch `build-scripts/build_llvm.py` uses for xtensa). ESP-IDF v5.4.2. On WAMR-2.4.5 with wamrc 2.4.5, a larger module of ours built with the default size level fails earlier, at load, with "AOT module load failed: invalid relocation offset"; I have not tried the 41-byte module there.

**To Reproduce**

1. `add.wat`:
   ```
   (module
     (func (export "add") (param i32 i32) (result i32)
       local.get 0
       local.get 1
       i32.add))
   ```
   `wat2wasm add.wat -o add.wasm`
2. `wamrc --target=xtensa --cpu=esp32s3 -o add.aot add.wasm` (556 bytes)
3. On the S3: `wasm_runtime_load`, `wasm_runtime_instantiate`, look up `add`, `wasm_runtime_call_wasm` with (2, 3).
4. `Guru Meditation Error: Core 0 panic'ed (LoadProhibited)`, EXCVADDR 0x81006136.

**Expected behavior**
`add(2, 3)` returns 5.

**Actual Result**
The panic above. Built with `wamrc --target=xtensa --cpu=esp32s3 --size-level=0` (484 bytes), the same steps return 5.

The result is the same wherever the AOT code is placed:

| AOT code in | default size level | `--size-level=0` |
|---|---|---|
| PSRAM, mapped executable (#5146 applied) | LoadProhibited, EXCVADDR 0x81006136 | returns 5 |
| internal RAM, `MALLOC_CAP_EXEC` (mirror off, memory protection off) | LoadProhibited, EXCVADDR 0x81006136 | returns 5 |

**Desktop (please complete the following information):**

- Arch: xtensa (ESP32-S3)
- Board: Seeed XIAO ESP32S3 (8 MB octal PSRAM)
- OS: ESP-IDF v5.4.2 (FreeRTOS)
- Version: main at ea5dd88

**Additional context**
`--size-level=0` works for us for now. I can run other builds or debug output on the board if that helps.
