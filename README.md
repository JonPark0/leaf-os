# LEAF OS

**LEAF** — *Lightweight/Lua EPD Application Framework.* A PDA-style e-reader OS
for the M5Stack PaperMono-Lite (SKU C153-Lite): home / app grid, status bar,
settings, file manager, and an e-book viewer (EPUB + TXT), plus a host-side tool
that bakes fonts into device binaries and a Lua SD-app kit.

Post-plan roadmap (SD app kit, localization): `docs/ROADMAP.md`
Design notes & hardware findings: `docs/ARCHITECTURE.md`

## Toolchain

- **ESP-IDF 5.5.x** (developed against 5.5.5). The top-level `CMakeLists.txt`
  hard-fails on any other minor line — arduino-esp32 3.3.x is pinned to 5.5.x.
- arduino-esp32, M5Unified, M5PM1, M5IOE1 are pulled as managed / git
  dependencies (`main/idf_component.yml`).

## Build & flash

```bash
idf.py set-target esp32s3
idf.py reconfigure          # resolves managed components -> dependencies.lock
idf.py build
idf.py -p <PORT> flash monitor
```

Download mode: hold the power button ~2 s until the red LED flashes, release.
After the first flash, USB-CDC reset usually re-flashes without the button.

## Releases

Pushing a `v*` tag builds the firmware in CI and publishes a GitHub release
(`.github/workflows/release.yml`). A tag with a hyphen, such as `v0.2.0-rc1`,
is marked as a pre-release.

| Asset | Use |
|---|---|
| `leaf-os-<tag>-full.bin` | First install, written at `0x0`. Erases saved settings and Wi-Fi. |
| `leaf-os-<tag>-update.zip` | Update that keeps settings: `esptool.py --chip esp32s3 write_flash @flash_args` |
| `leaf-os-<tag>-sd.zip` | SD card root: Lua apps and sample language packs |
| `leaf-os-<tag>-elf.zip` | Debug symbols |
| `SHA256SUMS` | Checksums |

## Status — Phase 1 (display foundation)

| Step | Item | State |
|---|---|---|
| 1 | Project skeleton, sdkconfig, partitions, IDF guard | **done** |
| 2 | `hal_i2c` + `hal_board` (system bus, PM1/IOE1 direct control) | **done** |
| 3 | `components/ssd1677_otp` panel driver | **done, verified on HW** |
| 4 | `core/framebuffer` + `core/display_manager` (refresh policy) | **done, verified on HW** |
| 5 | `hal_touch` (FT6336G, portrait coords, clamp 5..475 / 5..795) | **done, verified on HW** |
| 6 | `hal_power` frontlight (M5PM1 PWM0, 12-bit) + battery (M5PM1 VBAT) | **done** — needs M5PM1 0x13 bit3=0 (GPIO3 push-pull) or the light stays dark |
| 7 | `hal_keys` (GPIO2=A, GPIO3=B; no M5PM1-G3 conflict) | **done** |
| 8 | `tools/fontgen` + `.m5f` format + `gfx/font` + `gfx/text_layout` | **done** (HW check pending) |
| 9 | `ui/` widget base + Screen + Label/Button, `core/kernel` tick loop | **done, verified on HW** |
| 10 | On-hardware verification + flush timing measurement | **done** |

**Phase 1 complete.** Display path, refresh policy (incl. no-op skip), touch,
side keys, front-light, battery, embedded-font text rendering, line wrapping,
the widget layer and the two-task kernel are all verified on hardware.

## Phase 2 — OS shell (in progress)

| Item | State |
|---|---|
| `core/settings` (NVS), `App` interface + app stack + registry | **done, verified on HW** |
| `core/kernel` app stack, deferred nav (`processNav`), `RefreshHint::Page` | **done, verified on HW** |
| `ui/status_bar` + `ui/widgets` list_view, `app_home`, `app_settings` | **done, verified on HW** |
| `hal_storage` (SDMMC 4-bit mount + M5IOE1 hot-plug detect) | **done** (HW check pending) |
| `hal_rtc` (RX8130CE) | **done** |
| `hal_wifi` (STA + reconnect back-off) + SNTP → RTC (local, UTC+9 hard-coded) | **done, verified on HW** |
| `ui::TextField` + `ui::Keyboard` (ASCII, 3 layers) widgets | **done** (HW check pending) |
| `app_wifi` — async scan + saved networks (`/sd/.wifi/known.txt`, de-duped), key icon, tap-to-prefill, 연결/삭제 buttons | **done** (HW check pending) |
| `tools/icongen` + `.m5i` sheet (24 & 48 px) + `gfx::IconSet` | **done, verified on HW** |
| icons in status bar & Wi-Fi list; `ui::AppTile` icon+label home drawer | **done** (HW check pending) |
| `app_time` — steppers + Save + NTP-sync with completion line | **done** (HW check pending) |
| `app_files` — SD browser (folders first, ".." up, ext icons, sizes); pops on card removal | **done** (HW check pending) |
| dropdown / slider / popup widgets | todo (not blocking — settings uses tap-to-cycle) |
| known-network auto-join on boot from `/sd/.wifi/known.txt` | todo |

**Phase 2 is effectively complete.**

## Phase 3 — e-book viewer (in progress)

| Item | State |
|---|---|
| `book/book.h` — neutral `TextRun` stream + `MemoryBook` (scalar char offsets) | **done** |
| `book/txt_parser` — BOM/UTF-16 → UTF-8, paragraph/rule blocking, hard-wrap join | **done** |
| `tests/` host harness + `test_txt_parser` (12 cases) + fixtures | **written; needs a host C++ compiler to run** — see `tests/README.md` |
| `gfx/paginator` — page-boundary table for a fixed box + font, scalar-offset index | **done** |
| `app_reader` — full-screen TXT read view: L/R-third tap = page, centre = status bar, `<file>.pos` sidecar resume | **done** (HW check pending) |
| `book/xml_lite` (pull XML) + `book/xhtml_parser` (→ TextRuns) | **done** (host-testable) |
| `book/zip_reader` (in-mem ZIP, ROM `tinfl` inflate) + `book/epub_parser` | **done** (HW check pending) |
| `app_books` — home "책" bookshelf: `/sd` + `/sd/Books`, most-recent book pinned to top ("읽던 책") | **done** (HW-verified) |
| `app_settings` sub-pages: `app_display` (밝기 + 시스템/읽기 글꼴 family·크기 + 기본값 복원), `app_about` (버전 · OSS 고지), `app_language` (한국어 / English), `app_power` (재시작 / 전원 끄기) | **done** (HW-verified) |
| `core/i18n` — `tr(Msg)` string table (X-macro → `kKo`/`kEn`); all UI literals routed through it | **done** (HW-verified) |
| `core/lang_pack` — `/sd/lang/<code>.lang` packs + `/sd/fonts/<script>-<family>-<px>.m5f` (`gfx/font_scan`); `tools/langgen`; see `docs/LANGUAGE_PACKS.md` | **done** (HW-verified) |
| reader settings popup (글자 크기 / 여백 / 줄 간격) + SD `/fonts/` per-size loader | **done** (HW-verified) |
| pagination cache (step 19), cover thumbnails | todo |

Reader entry points: the home "책" tile (bookshelf list) or tapping a `.txt` /
`.epub` in `app_files`. Tap centre → status bar + `설정` button; the settings
panel steps font size / margin / line spacing (family is set in 설정 → 화면/
글꼴). Each change re-paginates keeping the reading position (scalar offset).
The reader body font resolves from `/sd/fonts/<script>-<family>-<px>.m5f`
independently of the system font; with no SD match it falls back to the embedded
22 px NanumGothic. One size is resident in PSRAM at a time (≤1.4 MB); the plan's
glyph-LRU (step 21) is unnecessary at 1-bit. Cover-thumbnail grid needs an image
decoder and is deferred.

Generate more faces/sizes: `tools/fontgen/.venv` (freetype-py + pillow) —
`python fontgen.py --font fonts/Nanum_Myeongjo/NanumMyeongjo-Regular.ttf --size
24 --ranges ui,korean --bpp 1 --out out/ko-myeongjo-24.m5f`, then copy to SD
`/fonts/` (staged set in `tools/fontgen/out/sd-bundle/`).
`zip_reader`/`epub_parser` use the ESP32-S3 ROM `tinfl_*` DEFLATE
(no miniz.c vendored) so they build under ESP-IDF only; `xml_lite` /
`xhtml_parser` / `txt_parser` stay host-clean.

No host C++ compiler is installed on this box, so the host tests can't run yet.
As an interim check, tapping a `.txt` in `app_files` parses it and logs the run
/ char counts over serial.

Refresh-speed work is flagged for later in **`docs/REFRESH_MODES.md`**: option C
(fewer 4 s fulls — `kPartialLimit` 13 + fold the DC-balance full into a screen
transition) is **applied**; option B (keep the panel awake across a partial
burst, ~0.7 s → ~0.3 s, no waveform risk) and option A (port a short absolute
mono waveform to replace the 4 s full, custom-waveform risk) are **deferred**.

Wi-Fi scan is now asynchronous (`hal::wifi::scanStart` / `scanBusy` /
`scanCollect`): the Wi-Fi screen shows "검색 중…" immediately and fills in when
the driver signals `SCAN_DONE` — no input freeze. While the password keyboard
is up, `DisplayManager::holdFullRefresh(true)` suppresses the DC-balance full
flash so typing is never interrupted; the deferred full runs when the keyboard
closes.

Wi-Fi bring-up before the on-screen keyboard exists: put `SSID` on line 1 and
`password` on line 2 of `/sd/.wifi.txt` (or the older `/sd/wifi.txt`); on a boot
with no stored SSID it is copied into NVS (`main.cpp provisionWifiFromSd`).

System font (설정 → 화면/글꼴 → 시스템 글꼴/크기): resolves at boot via
`gfx/font_scan` from the active language script + the saved family/size (`main.cpp
pickUiFont`), falling back to the embedded 22 px NanumGothic. Reader font is a
separate family/size on the same page. Changing the system font or language sets
`core::Settings::restartPending()`, which adds a **재시작하여 적용** row at the
top of 설정 (→ `app_power`); 설정 → 전원 also has 재시작 / 전원 끄기 (two-tap
confirm, `hal::power::reboot/shutdown` via the M5PM1). The C library clock
is seeded from the RX8130CE at boot and on every RTC write (NTP / manual) so
`.pos` sidecar mtimes — which decide the pinned "읽던 책" — order correctly
across reboots.

The file manager hides dot-prefixed names, so the reader writes its
reading-position sidecar as `/<dir>/.<book>.pos` and saved Wi-Fi networks live in
`/sd/.wifi/known.txt` (`ssid\tpassword` per line, de-duplicated on load).

M5Unified's released build does **not** autodetect the PaperMono-Lite, so we do
not call `M5.begin()`; `hal_i2c` / `hal_board` drive M5PM1 and M5IOE1 directly.
See `docs/ARCHITECTURE.md`.

The current `main/main.cpp` is an **interactive verification harness**: 6
touch-advanced stages (full mono, windowed partial, ghosting, forced full,
4-gray, touch-accuracy grid) with on-screen targets and serial prompts.
Replace it with the kernel boot once `core/` lands.

## Repository layout

```
components/ssd1677_otp/   SSD1677 driver (partial-mono / full-mono / 4-gray)
main/hal/                  platform drivers — the only code touching GPIO/I2C/SPI
main/core/                 display manager, framebuffer, event bus, kernel
main/gfx/                  font binary loader, glyph cache, text layout
main/ui/                   self-contained widget library
main/book/                 txt/epub parsers, paginator (also builds on host)
main/apps/                 home / settings / reader / files
tools/fontgen/             host font -> .m5f converter (Python)
tests/                     host-native tests for book/ and gfx/text_layout
```
