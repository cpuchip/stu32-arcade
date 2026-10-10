# Changes sent to WAMR

The `.patch` files are changes to WAMR (github.com/wasm-micro-runtime/wasm-micro-runtime), made with `git
format-patch` on its main branch, and are under WAMR's licence, Apache-2.0 WITH LLVM-exception, as its CONTRIBUTING.md
requires of contributions. The drafts (`issue*.md`, `pr*.md`), the scripts and `repro2/` are this repository's, MIT.

| patch | draft | upstream |
|---|---|---|
| 0001-esp-idf-map-PSRAM-AOT-code-on-the-instruction-bus.patch | issue.md, pr.md | #5145, #5146 |
| 0001-xtensa-fix-L32R-relocation-lost-to-strict-aliasing.patch | issue2.md, pr2.md | #5147, #5148 |
| 0001-aot-keep-the-target-symbol-maps-const.patch (pr3-syntax.sh reruns its syntax pass) | issue3.md, pr3.md | not posted |
