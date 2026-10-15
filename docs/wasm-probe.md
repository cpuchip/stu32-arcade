# WebAssembly runtimes on the S3: the probe (abacus #5025)

Michael (through abacus): "Go ahead with a WebAssembly runtime … measure both then decide which one."
This measures WAMR and wasm3 running one game file, Babal's own app, on the host and on an ESP32-S3,
against the same code built native. Building the loader waits for abacus's ruling on these numbers.
Measured 2026-10-08 by arcade; the S3 runs on casim's XIAO ESP32-S3 (8 MB octal PSRAM), window on
casim's go #5044.

## What was used, and their licences (read from each repository)

| | source | commit | licence (LICENSE, sha256 prefix) |
|---|---|---|---|
| WAMR (main) | github.com/wasm-micro-runtime/wasm-micro-runtime (moved from bytecodealliance) | 38b044a676893e678ceb19babaa431ca5e460114 (2026-10-08) | Apache-2.0 WITH LLVM-exception (0a274df3755e61b1, 219 lines) |
| WAMR 2.4.5 (for AOT) | same, tag WAMR-2.4.5 | 25bd7eb63e828e4bd242cc9b38d260b4b31c6605 | the same file, byte for byte |
| wamrc 2.4.5 | the release's wamrc-2.4.5-x86_64-ubuntu-22.04.tar.gz | sha256 fedda999…329f, as the release lists | as WAMR |
| wasm3 | github.com/wasm3/wasm3 | 28ecb9af6d2040e474a70f7cb7f43666740141fb (2026-09-29) | MIT, Steven Massey and Volodymyr Shymanskyy (c67aa5c0d77ea831, 21 lines) |
| wasi-sdk 34 | github.com/WebAssembly/wasi-sdk release wasi-sdk-34 | sha256 b761e3a0…b2c4, as the release lists | Apache-2.0 WITH LLVM-exception; wasi-libc: Apache-2.0 WITH LLVM-exception, Apache-2.0 or MIT, with musl (MIT) and others permissive |

All of it was cloned or unpacked in a scratch directory, not a repository (basecamp #5029). No runtime
code is in this repository: probes/wasm/ holds only our bench, guest driver and build files, which
name the runtimes' headers. A game built with wasi-libc carries wasi-libc's and musl's notices.

## The game file

probes/wasm/drive.c around Babal's app (games/babal/app/babal_app.c, games/babal/app/render.c, games/babal/core/): a noob game played as
app-far plays it (4 and 6 in turn every 300 ms, a jump every 700 ms, 20 ms ticks), 3,000 ticks, each
followed by a draw, the frames hashed (FNV-1a 64, chained), plus a 1M-step integer loop and a 1M-step
f64 loop. Built with wasi-sdk 34's clang as a reactor with no imports: 143,832 bytes, 4 pages
(256 KB) of linear memory. **It must be built `-mcpu=mvp`:** clang 23's default turns on reference
types, which WAMR's default build refuses ("zero byte expected").

## Every backend draws the same frames

Hash `bd0df19696198fe3` from native on the host and the S3, wasm3, WAMR classic interpreter, WAMR
fast interpreter (host) and WAMR AOT (host and S3). The same .wasm gives the same game everywhere it ran:
wasm's f64 is strict IEEE and has no fused multiply-add.

## On the S3 (ESP32-S3 at 240 MHz, ESP-IDF 5.4.2, -O2; bench in a pthread with a 64 KB stack)

| backend | tick avg | draw avg (worst) | int loop 1M | f64 loop 1M | native stack used | RAM after load: internal + PSRAM |
|---|---|---|---|---|---|---|
| native | 91 us | 1.53 ms (4.2) | 33 ms | 1.23 s | 2,024 B | 0 |
| wasm3, core in IRAM (its ESP-IDF example) | 840 us | 57.9 ms (148.7) | 2.01 s | 2.68 s | 37,352 B | 17,448 + 304,136 B |
| wasm3, core in flash | 1.41 ms | 56.7 ms (157.6) | 1.90 s | 2.68 s | 50,456 B | 11,304 + 451,604 B |
| WAMR classic interpreter | 1.66 ms | 54.1 ms (125.5) | 1.87 s | 2.86 s | 2,216 B | 13,408 + 436,236 B |
| WAMR fast interpreter | crashed in its loader (below) | | | | | |

(The f64 loop is slow even native: the S3's FPU is single precision, so f64 is software.) RAM after
load includes the bench's own copy of the module for WAMR (143,832 B; wasm_runtime_load may write its
buffer) and the 256 KB linear memory in each.

**Code size** (idf.py size-components, the runtime's library): wasm3 121,670 B, of which 75,404 B in
internal RAM (its IRAM placement); wasm3 in flash 157,560 B, none internal; WAMR fast 65,628 B
(1,104 B RAM); WAMR classic 58,039 B (72 B RAM).

**Interpreted, the game is about 35x native in its draw and 57x in a tight loop.** A Babal frame
takes 54-58 ms to draw against 1.53 ms native: past a 20-50 ms tick. Keeping the interpreters' stacks
and wasm3's compiled code in internal RAM (allocations up to 64 KB kept internal) moved 18-34 KB from
PSRAM to internal RAM and changed no time (wasm3's loop: 2,009,655 us both ways): the cost is the
interpretation, not memory placement.

**WAMR's fast interpreter (38b044a) crashes on the S3** in its loader (LoadProhibited at address 0x1
in wasm_loader_check_br, from wasm_loader_prepare_bytecode) loading the same module the host loads
and runs. **Open**, left unchased by abacus's ruling (#5092): if the fast interpreter is ever wanted on
the chip, this is the first thing to look at (and to report upstream).

**wasm3 needs more stack than a game gets.** 37-50 KB of the task's stack against APP_STACK_BYTES
16 KB; WAMR's interpreter keeps its own stack and used 2.2 KB of the task's.

## AOT: about 1.5x native on the S3

wamrc compiles the .wasm ahead of time to the target's machine code; WAMR loads that (.aot,
target-specific). The prebuilt wamrc has no Xtensa backend, so wamrc 2.4.5 was built (in Docker,
16 CPUs and 24 GB capped) on Espressif's LLVM fork, github.com/espressif/llvm-project branch
xtensa_release_18.1.2 at 344b928e67bfc1d07cb150609c55548e86a9bf19 (LICENSE.TXT: Apache-2.0 WITH
LLVM-exception plus the legacy University of Illinois/NCSA notice; sha256 prefix 8d85c1057d742e59),
targets X86 and Xtensa, Release, no tests or tools (abacus #5092, basecamp #5096). ESP-IDF's prebuilt
esp-clang (esp-18.1.2_20240912, sha256 as idf_tools lists) was checked first: it has libLLVM.so but no
LLVM headers or CMake package, so it could not build wamrc. The runtime is WAMR 2.4.5 (25bd7eb), whose
AOT format (5) matches; WAMR main is at format 6.

Babal's game compiled with `wamrc --target=xtensa --cpu=esp32s3 --size-level=0`, on casim's XIAO:

| | tick avg | draw avg (worst) | int loop 1M | f64 loop 1M | task stack used | internal RAM after load |
|---|---|---|---|---|---|---|
| native | 91 us | 1.53 ms (4.2) | 33 ms | 1.23 s | 2,024 B | 0 |
| WAMR AOT | 146 us | 2.32 ms (4.97) | 48 ms | 1.21 s | 2,568 B | 91,284 B (+ 381,452 PSRAM) |

The hash is bd0df19696198fe3 again. Draw 1.5x native, the tight loop 1.4x, f64 the same (both call
the same soft-float routines). **The .aot is 92,368 B** (the .wasm 143,832 B). **The AOT runtime** (AOT
loader only, no interpreter) is 37,086 B: 35,110 B of code in flash, 1,840 B of RAM. **The task stack**,
2.6 KB, is well inside APP_STACK_BYTES (16 KB). **The cost is internal RAM:** the game's code runs from
internal RAM, 91 KB for Babal after load, out of the 341,760 B of DIRAM the S3's build reports, which
also holds the calculator's own state and stacks.

What it took, each found on the chip and recorded:
1. **WAMR's PSRAM code mapping is wrong on the S3.** With PSRAM on, WAMR 2.4.5 (shared_platform.cmake)
   puts AOT code in PSRAM and computes its instruction address as data address + (SOC_IROM_LOW -
   SOC_IROM_HIGH) = - 0x2000000: 0x3C0779b4 became 0x3a0779b4, not instruction memory
   (IllegalInstruction there). Disabled in the scratch copy (WASM_MEM_DUAL_BUS_MIRROR off), so the
   code goes to internal executable RAM (MALLOC_CAP_EXEC).
2. **ESP-IDF's memory protection off** (CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n), as WAMR's own ESP-IDF
   example sets it: memory protection forbids executable heap. **This is a firmware-wide setting
   (soroban's) with a security cost:** it is what stops code from being written into executable memory.
3. **`--size-level=0`.** The default build's .aot (110,960 B) is refused: "invalid relocation offset"
   (aot_reloc_xtensa.c). The object's relocations all fit their sections, so the fault is in the .aot
   or its loader; open. size-level 0 (WAMR's own hint for Xtensa's L32R range) loads.
4. **XIP is not available on Xtensa:** `--xip` still leaves 574 L32R relocations in .text, which XIP
   mode refuses ("cannot apply relocation to text section ... --enable-indirect-mode").

The scratch copy of WAMR 2.4.5 carries two changes for this: the mirror disabled (1), and a print in
check_reloc_offset's failure branch (diagnostic only).

## Internal RAM in the real firmware: it does not fit there (abacus #5157)

The stand-in at stu32-firmware be82607 (a scratch clone with one print added, on soroban's S3, go
#5159), after its tasks start: internal free 50,779 B, largest block 31,744 (its own boot line, before
the last tasks: 59,779, as soroban measured, #5133); executable 0 (memory protection on). With WAMR's AOT
runtime linked and initialised and memory protection off: internal 49,883 B, executable free 10,408 B,
largest block 7,680 B. Babal's AOT code is about 73 KB (91 KB internal after load on the bench). So in
this firmware a game's code cannot run from internal RAM, by about ten times; the calculator's own state,
stacks and panel buffers already hold the rest.

## The game's code in PSRAM, mapped executable: it fits, with memory protection on

ESP-IDF can map PSRAM's physical pages into the instruction space: esp_mmu_vaddr_to_paddr then
esp_mmu_map(paddr, size, MMU_TARGET_PSRAM0, MMU_MEM_CAP_EXEC | READ | 32BIT, PADDR_SHARED). That is
what WAMR's dual-bus mirror meant to do. The scratch copy of WAMR 2.4.5 now does it (the change is
probes/wasm/wamr-2.4.5-psram-exec.patch, under WAMR's licence, applied by no build) in os_mmap
(page-aligned PSRAM, 64 KB pages; the data address kept for os_get_dbus_mirror, through which the loader
copies and relocates the code; os_munmap unmaps and frees), with the mirror on for CONFIG_SPIRAM.

On casim's XIAO (go #5165), Babal's .aot, **memory protection on** (ESP-IDF's default):

| | tick avg | draw avg (worst) | int loop 1M | internal RAM after load | PSRAM |
|---|---|---|---|---|---|
| native | 91 us | 1.53 ms (4.2) | 33 ms | 0 | 0 |
| AOT, code in internal RAM (protection off) | 146 us | 2.32 ms (4.97) | 48 ms | 91,284 B | 381,452 B |
| **AOT, code in PSRAM (protection on)** | 631 us | 2.54 ms (5.28) | 48 ms | **5,236 B** | 520,980 B |

The hash is bd0df19696198fe3. With protection off the PSRAM run is the same to within 1%. The tick is
slower from PSRAM (rules code fetched through the cache; 631 us of a 20-50 ms tick), the draw barely.
**Both of abacus's tensions go:** 5 KB of internal RAM fits the firmware's 49 KB free, and memory
protection stays on (code is mapped, not heap-executable).

## Rules that go with AOT (abacus #5157)

**The S3 loads only an .aot that our own pinned wamrc made from the game's .wasm.** An .aot is native
code: WAMR's sandbox (bounds checks on linear memory) is only as good as the compiler that emitted it, so
an .aot from anyone else is never loaded. The .wasm is the file a game ships as; the .aot is built from it
when the game is listed. soroban owns the memory-protection setting itself.

## Draft upstream report (outward: needs Michael's yes; not sent)

Title: ESP32-S3 with PSRAM: AOT code's instruction address is wrong (WASM_MEM_DUAL_BUS_MIRROR)

> WAMR 2.4.5 (25bd7eb), ESP-IDF v5.4.2, ESP32-S3 with 8 MB octal PSRAM, AOT from wamrc 2.4.5
> (--target=xtensa --cpu=esp32s3 --size-level=0). With CONFIG_SPIRAM set,
> core/shared/platform/esp-idf/shared_platform.cmake defines WASM_MEM_DUAL_BUS_MIRROR=1, and os_mmap
> returns PSRAM data-bus memory plus MEM_DUAL_BUS_OFFSET = SOC_IROM_LOW - SOC_IROM_HIGH (-0x2000000)
> as the code's address. On the S3 PSRAM's data address 0x3C0779b4 becomes 0x3A0779b4, which is not
> instruction memory: the first call into the module panics with IllegalInstruction at PC 0x3a0779b4.
> A fixed offset cannot work here, since heap pages in PSRAM are not mapped for instruction fetch; mapping
> the block's physical pages with esp_mmu_vaddr_to_paddr and esp_mmu_map(paddr, len, target,
> MMU_MEM_CAP_EXEC | MMU_MEM_CAP_READ | MMU_MEM_CAP_32BIT, ESP_MMU_MMAP_FLAG_PADDR_SHARED, &ibus) on a
> page-aligned PSRAM block, and returning that block's data address from os_get_dbus_mirror, runs the
> module correctly with CONFIG_ESP_SYSTEM_MEMPROT_FEATURE left on (same results as native, 5 KB of
> internal RAM). With the mirror off (code in MALLOC_CAP_EXEC) it also runs, but only with memory
> protection disabled. We can share the patch.

## Recommendation (arcade, for abacus's ruling)

**WAMR, its AOT mode on the S3, the game's code in PSRAM.** One .wasm per game is the portable file
(the browser runs it as it is; Android and the S3 through WAMR); the S3 runs an .aot compiled from it with
our pinned wamrc, per target, when a game is listed (an .aot is target-specific). On the S3 that is about
1.7x native in Babal's draw, a 2.6 KB task stack, 5 KB of internal RAM, memory protection on, for 37 KB
of runtime in flash. Interpreted (wasm3 or WAMR), a Babal-class game is about 35x
native: too slow to draw at frame rate. wasm3 also needs 37-50 KB of task stack, past APP_STACK_BYTES.

**What abacus and soroban would be ruling with it:** a patch to WAMR's ESP-IDF port (the PSRAM mapping,
about 60 lines in espidf_memmap.c, plus three component requirements), carried by us or upstreamed (the
draft above); wamrc built on Espressif's LLVM (a build tool, not shipped); `--size-level=0` until the
default build's relocation fault is understood; only .aot from our pinned wamrc.

## How to run it

probes/wasm/: drive.c (the game file's driver), bench.c (one bench, -DB_NATIVE, -DB_WASM3, -DB_WAMR),
host-wamr.cmake (WAMR on the host), s3/ (the ESP-IDF project: BACKEND=native|wasm3|wasm3flash|wamr in
the environment, WASM= the scratch directory, ARCADE= this repository; fast.defaults or
classic.defaults for WAMR; cap.py reads the console until PROBE DONE).
