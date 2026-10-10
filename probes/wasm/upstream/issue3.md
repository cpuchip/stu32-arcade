Title: aot: the target symbol maps are writable, so a microcontroller keeps them in internal RAM

Each architecture's `target_sym_map` in `core/iwasm/aot/arch/` is declared `static SymbolMap[]`, and `get_target_symbol_map()` returns `SymbolMap *`. Nothing writes to the tables: the loader looks symbols up in them, and the PLT setup copies addresses out. Because they are not `const`, the linker puts them in `.data`, which a microcontroller copies into internal RAM at boot.

On an ESP32-S3 (ESP-IDF v5.4.2, WAMR main ea5dd88, AOT only) the Xtensa map is 1,000 bytes of DRAM. Internal RAM is the scarce resource on that chip: in our firmware those bytes decided whether it booted with WAMR linked in. Declaring the tables `const` moves them to flash rodata, and static internal RAM went down by 1,008 bytes (the table plus alignment).

I have a change that makes the tables, the function's return type and the loader's two locals `const` in all ten arch files, `aot_reloc.h` and `aot_loader.c`, and will open a PR for it.
