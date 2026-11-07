# Architecture — LEAF OS

*LEAF: Lightweight/Lua EPD Application Framework.* Living document; this file
captures decisions and hardware findings as implementation proceeds.

## Layering

```
apps/   home · settings · reader · files          (App interface = future Lua boundary)
ui/     widget tree, screens, status bar
gfx/    font_renderer · font_cache · text_layout
book/   txt/epub parsers · paginator   (host-testable, no hardware deps)
core/   display_manager · framebuffer · event_bus · kernel · settings
hal/    i2c · display · touch · keys · power · rtc · storage · wifi
```

Rule: only `hal/` touches GPIO / I2C / SPI. Everything above talks to `hal/`
through its facade (`hal::`), and all screen drawing goes through
`core::DisplayManager` — widgets never call the panel driver directly.

## SSD1677 panel driver (`components/ssd1677_otp`)

Ported from `m5stack/M5PaperMono-OTP-Demo`
(`components/EDP_OTP_LUT_demo/`). Findings from that source, which drive the
`framebuffer` and `display_manager` design:

### Geometry
- **Controller-native orientation is 800 (W) x 480 (H) landscape.** The visible
  panel is 480 x 800 portrait — a 90 rotation. RAM X selects the visible
  *vertical* position, RAM Y the visible *horizontal*.
- Monochrome RAM is bit-packed **MSB first**: `800 / 8 = 100` bytes per row,
  480 rows, **48000 bytes per full mono plane**.
- In mono RAM, **bit 1 = white, bit 0 = black**.

### Planes and refresh modes
The controller has two RAM planes: `0x24` (RAM1) and `0x26` (RAM2).

| Mode | Command seq | Planes written | Notes |
|---|---|---|---|
| **Partial (mono)** | `UPDATE_CONTROL(0x22) = 0xFF`, then `MASTER_ACTIVATION(0x20)` | RAM1 only | Built-in partial waveform. Needs a valid mono baseline in RAM. Fast, no full flash. |
| **Full (mono)** | `0x22 = 0xF8` + inverted frame, then `0x22 = 0x14` + both planes | RAM1 + RAM2 (same image) | OTP Mode 1 waveform. Two-phase: an inverted sync frame first. Leaves a valid partial baseline. |
| **Full (4-gray)** | write RAM1 + RAM2, `0x22 = 0xD7`, `0x20` | RAM1 + RAM2 (1 bit each) | OTP 4-gray waveform. `white=00, lightgray=10, darkgray=01, black=11`. **Destroys the mono partial baseline** (`baseline_ready = false`). |

**Consequences for `DisplayManager`:**
1. **Partial refresh is monochrome only.** Any content that actually needs gray
   (cover thumbnails, images) forces a full 4-gray refresh, which also forces
   the *next* frame to be a full mono refresh to rebuild the partial baseline.
   Text UI is 1-bit and uses partial refresh normally.
2. The DC-balance rule from `docs.md` (~10 fast refreshes, then one full) maps
   onto the partial-mono counter. Threshold default = 10. **App-to-app screen
   changes are `RefreshHint::Page`, not `Quality`** — they go through the fast
   full-frame partial (~0.7 s), not the ~4 s OTP full-mono. The ~4 s OTP path
   runs only for: the DC-balance forced full every `kPartialLimit` fast
   refreshes, an explicit `requestFullClear()` (ghost clear), the first frame
   after boot, and the frame after any 4-gray frame (baseline rebuild).
3. Dirty rectangles must be **8-pixel aligned on the RAM-X axis**
   (`x &= ~7; width = roundup8(right) - x`). The manager quantizes every dirty
   rect before handing it to the driver.
4. **All refresh modes must use the same RAM data-entry direction.** The 4-gray
   OTP waveform is fixed to `0x02` (RAM-X decreasing, RAM-Y increasing); mono and
   partial now use `0x02` too (`set_ram_window()`). Using `0x03` for mono while
   4-gray stays on `0x02` makes the image scan out mirrored on the RAM-X axis
   (= physical vertical axis) whenever a mono full-refresh follows a gray frame
   or vice-versa — observed as the whole UI "180° rotated + h-mirrored". The
   dormant windowed-partial path in `write_ram()` would need reversed byte order
   before it can be used with X-decreasing; partial always writes the full frame
   for now, so it is unaffected.

### Transport
- SPI2_HOST, **mode 0, 20 MHz**, half-duplex, MOSI only.
- Pins (direct ESP32-S3 GPIO): `MOSI=14 SCLK=15 CS=16 DC=17 BUSY=18`.
  CS and DC are driven manually as GPIO; `spics_io_num = -1`.
- `BUSY` is active-high (`while (gpio_get_level(BUSY)) wait;`), pulled up.
- EPD power-enable and reset are on **M5IOE1** (zero-based pin index):
  `EPD_ENABLE = 2`, `EPD_RESET = 4`. Reset pulse: low 10 ms, high 10 ms.
- The demo calls `M5.Display.releaseBus()` before taking SPI2, i.e. M5Unified /
  M5GFX is initialized for peripherals but its panel driver is **not** used for
  transfer. We keep that split.
- DMA staging buffer must be `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL`; chunk size
  4092 B. Frame planes are `MALLOC_CAP_SPIRAM`.

### Deep sleep between updates
`DEEP_SLEEP(0x10) = 0x01` (Deep Sleep Mode 1) retains RAM for the next partial
update; exit requires a hardware reset. A **software reset (`0x12`) is only sent
for full updates** — sending it before a partial update would wipe the baseline.

## Confirmed on hardware (2026-08-30, interactive verify harness)

Panel driver (partial-mono / full-mono / 4-gray), M5PM1/M5IOE1 direct control,
and FT6336G touch all verified end-to-end. Findings:

- **All three refresh modes work.** 4-gray shows four distinct levels
  (white / light / dark / black). Windowed partial refresh updates only the
  requested sub-rectangle — the rest of the panel does not flash. A full
  refresh clears accumulated ghosting from a burst of partials.
- **Measured timings** (our driver + DisplayManager, full-frame RAM writes,
  20 MHz SPI, hardware reset per call): **full mono ~4.0 s**, **full 4-gray
  ~4.0 s**, **partial-mono ~0.70 s**. The partial cost is dominated by the
  per-call deep-sleep exit + waveform, not the 48 KB SPI transfer (~19 ms).
  Optimization (Phase 4): keep the panel awake across a burst of partials;
  window the RAM1 write once the 0xFF plane-swap semantics are pinned down
  (writing a sub-window instead of the full frame currently corrupts the
  differential — markers alternate on/off — so refreshPartialMono writes the
  whole frame like the OTP demo).
- **Touch <-> framebuffer transform** (re-verified 2026-08-31 after the
  scan-direction fix, with a live `touch down (x,y)` log against visible tiles):
  touch reports **portrait** (x in [5,475], y in [5,795]) and its raw axes are
  already aligned with the un-flipped portrait framebuffer — **no inversion**
  (`hal_touch.cpp kInvertY = false`). Tapping the top-left tile reads ~(89,127).
- **Framebuffer -> RAM transform** (`display_manager.cpp uiToRam` / pack):
  the panel maps controller `RAM(rx,ry)` to physical portrait pixel
  `(x = ry, y = RAM_W-1-rx)`. To land `UI(x,y)` at physical `(x,y)` the pack
  transposes **and** inverts the RAM-X axis:

  ```
  RAM.x = RAM_W - UI.y - UI.h      (vertical-axis inversion)
  RAM.y = UI.x
  pack reads fb.pixel(ux = ry, uy = (RAM_W-1) - rx)
  ```

  The UI works in **portrait 480 x 800**; `flush()` transposes+inverts to RAM
  landscape 800 x 480.

## Framebuffer plan (`core/framebuffer`)

- App-facing buffer: **480 (w) x 800 (h) portrait**, 2 bpp = 96000 B, PSRAM.
  Values map to the 4 gray levels; 1-bit UI uses only white/black.
- Previous-frame copy (another 96000 B) for dirty-rect diffing.
- `flush()` transposes the dirty region to landscape (`RAM(x,y) = (ui_y,ui_x)`),
  packs it MSB-first into the driver's mono plane (or both planes for a gray
  flush), snaps the RAM-x span to a multiple of 8, and calls the driver.
- Partial flush is **mono only**; any dirty rect that contains a gray value
  forces a full 4-gray refresh (which then forces the next frame to be a full
  mono refresh to rebuild the partial baseline).
- **PSRAM read cost of a full pack** is still to be measured before optimizing.

## Front-light (M5PM1 PWM0 on its GPIO3)

`hal_power::init()` order matters and matches M5GFX `Light_M5PaperMono`:
1. **`0x13` (GPIO_DRV) bit 3 -> 0** — GPIO3 to **push-pull**. The M5PM1 default
   is open-drain; left that way the PWM only sinks, the high phase floats, and
   the LED driver never turns on (symptom: no light at any brightness). This
   was the bug that made the back-light look dead.
2. `0x16` (GPIO_FUNC0) bits [7:6] = `0b11` — GPIO3 -> PWM0.
3. `0x34/0x35` PWM freq (we use 2000 Hz; M5GFX uses 5000 — either is fine).
4. Duty: `0x30` = low byte, `0x31` = `[4]EN | [5]POL | [3:0] duty[11:8]`,
   12-bit duty 0..4095. `EN=0` (our 0 %) is a hard off.

The 3.3 V LDO / 5 V DCDC / charger rails (`0x06 |= 0x17`) and the I2C-idle-sleep
and watchdog quiets (`0x09=0`, `0x0A=0`) are done earlier in `hal_board`.

## Open hardware questions

- **G3 pin sharing** — resolved: `M5PM1 G3` is M5PM1-internal (PWM0 for the
  front-light); ESP32 `G3` is Button B. No conflict; both work simultaneously.
- **4-gray partial** — confirmed *not supported* by the OTP demo; if a future
  waveform allows it, revisit the refresh table.
