# WAMR in the firmware: the blueprint (for abacus and soroban, before anything is built)

Decision 67: WAMR is the runtime; a game ships as one .wasm, and the S3 runs an .aot made from it by our pinned
wamrc. Decision 68: every addition reports its flash, internal RAM (static and at run time), IRAM, stack and S3
time, and nothing new goes into IRAM without a ruling. Decision 59: code enters the firmware through soroban's
gate; the APPS framework is soroban's. This is arcade's proposal for the unit (abacus #5390 item 2), measured
where the probe already measured (docs/wasm-probe.md) and marked "to measure" where only the firmware can say.

## 1. WAMR, pinned (abacus #5403)

- `scripts/fetch-wamr.sh` exports WAMR at one commit (main ea5dd88 today) from a local clone (`WAMR_REPO`, as
  `fetch-casim.sh` does), applies a numbered patch series from `third_party/wamr/patches/`, and checks the
  result with `git write-tree` against a tree hash recorded beside the pin (`third_party/wamr/PIN`: commit,
  patch sha256s, resulting tree). Any other tree refuses to build.
- The series, each under WAMR's licence (Apache-2.0 WITH LLVM-exception), each counted in section 5:
  1. #5146 PSRAM AOT code mapped executable (upstream PR, open)
  2. #5148 L32R relocation fix (upstream PR, open)
  3. the esp_cache_msync cache sync, IRAM 0 (local, 7f55b15)
  4. *if ruled (section 3)*: os_self_thread tolerating a FreeRTOS task (local, not written)
  When an upstream PR merges and the pin moves past it, its patch leaves the series.
- WAMR's ESP-IDF component, configured: AOT loader only (no interpreter, no libc-builtin, no WASI, no
  pthread library), as measured.

## 2. The build option

`STU_WASM` (CMake option and Makefile variable), **off by default** until abacus accepts the unit. Off, the
firmware builds exactly as now: no WAMR source compiled, no bytes added (checked by comparing the images'
size reports with the option off and on main). On, the device images get the WAMR component and the bridge
(section 3), and the host build gets WAMR for Linux for the tests (section 6).

## 3. A .wasm game as an app_def: the bridge

- `firmware/wasm_app.c` (soroban's directory, through the gate): one `app_def` per wasm game, all sharing
  the bridge's calls. `ctx` holds the WAMR module, instance and exec-env handles and the game's frame offset.
- The game side is unchanged SDK C: a guest shim (`sdk/wasm/guest.c`, arcade's) compiles a game's `app_def`
  into a reactor module exporting `game_load(ptr, n)`, `game_start`, `game_key(key, down, ms)`,
  `game_tick(ms)`, `game_draw() -> frame ptr`, `game_save(ptr, cap)`, `game_stop`, and keeps the game's ctx
  and its 12,000-byte frame in linear memory. So Babal, BALL and later games build both ways from one source.
- Host calls the game imports, registered with `wasm_runtime_register_natives("env", ...)`: `app_beep`,
  `app_now_ms`, `app_seed`, `app_tune` (apps.h v1.2), nothing else. `sdk/draw` compiles into the game.
- Each draw copies the game's frame out of linear memory into the framework's `ls027_frame` (12,000 B
  PSRAM to PSRAM; time to measure).
- Lifecycle: `wasm_runtime_full_init` at a wasm game's `load`, `wasm_runtime_destroy` at its `stop`, so
  WAMR holds no RAM while the calculator runs.
- **Open: the thread.** WAMR's ESP-IDF `os_self_thread()` is `pthread_self()`, which asserts on a plain
  FreeRTOS task, and the framework calls apps from the calc task. Options: (a) run the game's calls on a
  persistent pthread, costing a task stack (internal RAM, 4-8 KB) and a hand-off per call; (b) local patch 4,
  `os_self_thread` returning the FreeRTOS task handle when the caller is not a pthread, costing nothing
  measurable but another patch. arcade recommends (b) after measuring that nothing else in the AOT-only
  build needs a real pthread; soroban's call, as it touches the framework's threading.

## 4. Where the .aot comes from (the pinned-wamrc rule)

- A wasm game's repo carries its sources, the built `.wasm` and `.aot`, and in its manifest the provenance:
  wasi-sdk version and sha256, wamrc's WAMR commit and its LLVM commit (espressif/llvm-project 344b928), the
  wamrc flags, and the sha256 of both files.
- `scripts/gen_games.py` gains `kind = wasm`: it embeds the `.aot` in the image (EMBED_FILES) and refuses one
  whose sha256 differs from the manifest. A reproduction check (`scripts/check-aot.sh`, run on a box that has
  the pinned toolchains) rebuilds both files and compares hashes, so an `.aot` from anyone else's wamrc is
  caught. The firmware build itself does not need LLVM.
- Phase 1 is embedded games (compiled in, listed like Babal). Loading from a storage partition (the truly
  loadable game, Michael's roadmap) is a later unit, with the same loader.

## 5. Decision 68's table

Measured on casim's XIAO S3 in the probe bench (WAMR main ea5dd88 + patches 1-3, Babal's default-level .aot),
and in the firmware images (the right column: the S3 stand-in at abacus-firmware ab51544, 2026-10-09, struck
figures corrected in place):

| cost | measured (bench) | measured in the firmware (stand-in) |
|---|---|---|
| flash, WAMR | 35,706 B code + 136 B data | with the bridge, option on vs off |
| flash, per game | Babal: 110,960 B .aot (the .wasm 143,832 B is not stored) | |
| internal RAM, static | ~~1,944 B~~ (counted, but most of it was writable tables in DRAM) | +2,100 B D/IRAM, which stopped the stand-in at boot; +324 B with patch 4 and `WAMR_BUILD_QUICK_AOT_ENTRY=0` |
| internal RAM, run time | ~~5,236 B while Babal is loaded~~ (the bench called WAMR directly) | 13,904 B while BABLW ran (WAMR's allocations under the 16 KB always-internal malloc threshold); 152 B with the bridge's PSRAM allocator (aeb8fca): 104 B once, at the first `wasm_runtime_full_init`, flat over 20 cycles, and 48 B that unload returns |
| IRAM | 0 B | option on vs off (Rev A has 1 B free at be3617e) |
| stack | 2,568 B of the caller's | calc task: BABLW never below boot's mark (29,060 B free); runaway recursion traps with 964 B left (2 KB guard) |
| PSRAM, run time | 539,412 B for Babal: 256 KB linear memory, 128 KB code mapping (64 KB pages), the module copy | whether the .aot can be loaded from flash without the 111 KB copy |
| S3 time | Babal draw 2.535 ms (native 1.527), tick 607 us (native 91) | FRAMEBENCH (draw and blit, 50 frames): BABAL 665 a second, BABLW 381.0 (381.7 with quick entries on) |

Per patch: 1 and 3 change only the PSRAM mapping path (counted in the WAMR row above); 2 changes one function
(no size change measurable at the bench); 4 makes the target symbol maps const, 1,008 B less static internal RAM
on the stand-in (probes/wasm/wamr-local-const-symbol-map.patch).

## 6. The oracle

- Host: app_test plays Babal's app scripts through the native Babal and through Babal-as-wasm (WAMR for
  Linux, x86 AOT from the same wamrc build) and requires identical frame hashes, the way the probe found
  bd0df19696198fe3 everywhere. Planted faults: a wrong import, a dropped frame copy, a skipped save.
- S3: the stand-in image with `STU_WASM=1` replays the same scripts on the chip (soroban's board, a window
  as always) and compares hashes; the table above is filled in from that build.

## 7. Proposed order

1. abacus and soroban rule on sections 1-4 (and the thread option).
2. arcade: guest shim and Babal-as-wasm in stu32-arcade, with the host oracle (no firmware change).
3. arcade, for soroban's gate: fetch-wamr.sh, the patch series, `STU_WASM`, the bridge, gen_games `kind = wasm`;
   the table measured on both images.
4. Later units: loading games from a partition; a second wasm game.
