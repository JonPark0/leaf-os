# Host tests

Fast, hardware-free unit tests for `main/book/` (and later `main/gfx/text_layout`).
The parsers are written to compile on both the host and ESP-IDF.

```sh
cmake -S tests -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

## Toolchain

Needs a host C++17 compiler on PATH — `g++`, `clang++`, or MSVC `cl`. The
ESP-IDF xtensa cross-compiler in `C:/Espressif` **cannot** build these (bare-metal
target, no host libstdc++).

This dev box currently has **no host compiler installed**. Options:
- `winget install --id MSYS2.MSYS2` then `pacman -S mingw-w64-ucrt-x86_64-gcc`
- `winget install --id LLVM.LLVM` (clang) + the Windows SDK, or
- Visual Studio Build Tools ("Desktop development with C++").

Until then, `app_files` runs the txt parser on any `.txt` you tap and logs the
run/char counts over serial — a coarse on-device smoke check.
