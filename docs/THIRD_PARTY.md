# Third-party / open-source components

The LEAF OS firmware includes or links the following. This file is the
canonical attribution list referenced by the on-device **설정 → 정보** page.

## Frameworks & libraries (linked into the firmware)

| Component | Version | License | Notes |
|---|---|---|---|
| ESP-IDF | 5.5.5 | Apache-2.0 | Espressif IoT Development Framework |
| arduino-esp32 | 3.3.x | LGPL-2.1-or-later | pulled as an IDF managed component |
| M5Unified / M5GFX | as pinned in `main/idf_component.yml` | MIT | framebuffer/util only — the panel is driven by our own `components/ssd1677_otp` |
| M5PM1 / M5IOE1 | as pinned | MIT | power-management / IO-expander helpers |
| ESP32-S3 ROM `miniz` (`tinfl_*`) | on-chip | zlib/MIT (Rich Geldreich) | DEFLATE for EPUB ZIPs; no source vendored |
| Lua | 5.4.8 | MIT | `components/lua/` (core only — io/os/package/debug not built); runs SD apps, see `docs/APP_KIT.md` |
| cJSON | as bundled with ESP-IDF | MIT | `json.decode` for SD apps |

The SSD1677 waveform sequencing in `components/ssd1677_otp` is derived from
M5Stack's `M5PaperMono-OTP-Demo` (MIT).

## Fonts

| Font | License | Source |
|---|---|---|
| 나눔고딕 (Nanum Gothic) | SIL Open Font License 1.1 | NAVER Corp. — full text in `fonts/Nanum_Gothic/OFL.txt` |
| 나눔명조 / 나눔손글씨 펜 (Nanum Myeongjo / Pen Script) | SIL Open Font License 1.1 | `fonts/Nanum_Myeongjo/OFL.txt`, `fonts/Nanum_Pen_Script/OFL.txt` — SD-card faces |
| Noto Sans / Noto Serif CJK JP·SC·TC | SIL Open Font License 1.1 | Google — `fonts/Noto_{Sans,Serif}_{JP,SC,TC}/OFL.txt` — SD-card CJK faces |

Only the embedded 22 px NanumGothic (Korean + Latin) ships in the firmware. All
other faces and every CJK font live on the SD card under `/sd/fonts` (see
`docs/LANGUAGE_PACKS.md`). Fonts are converted to the device `.m5f` bitmap format
by `tools/fontgen`; the converted artifacts remain under the OFL.

## Icons

| Set | License | Source |
|---|---|---|
| Dazzle Line Icons | CC Attribution (per SVGRepo) | https://www.svgrepo.com — SVG sources in `icons/Dazzle Line Icons/`, packed to `.m5i` by `tools/icongen` |

## First-party

Everything under `main/`, `components/ssd1677_otp/`, `apps/`, `tools/`, and
`tests/` is original to this project and released under the MIT License (see
`LICENSE`), except the SSD1677 waveform sequencing noted above, which keeps
M5Stack's MIT notice. Third-party components keep their own licenses.
