# E-paper refresh speed — findings & options

## What the user asked

> The full refresh cycles through all four grey tones and takes ~4 s. Is there
> a mode that only refreshes a specific "depth"? How do we make the full
> refresh faster and rarer?

## There is no "refresh one grey level" mode

The SSD1677 drives every pixel it touches with a **waveform** — a per-frame
sequence of +V / −V / 0 pulses from a LUT. What varies between "modes" is the
*length and shape* of that waveform and *which pixels* it is applied to:

| Idea | What it really is |
|---|---|
| "refresh only changed pixels" | **Mode 2 / differential** — the LUT only drives pixels whose black/white value changed vs. the previous frame. Our `refreshPartialMono` (`0x22 = 0xFF`) already does this. |
| "refresh only some tones" | doesn't exist. But a **short absolute waveform** drives every pixel to its target level in ~0.3–0.5 s instead of the ~2 s multi-oscillation ghost-clear. |
| "the 4-tone cycle" | the long *ghost-clearing prefix* (drive black↔white several times) that precedes the settle. Skipping/shortening it is where the time goes. |

## Why *ours* is ~4 s

`components/ssd1677_otp` `refreshFullMono()` runs the OTP Mode-1 waveform
**twice**: an inverted "sync" pass (`0x22 = 0xF8`) then the real draw
(`0x22 = 0x14`). Each OTP pass is ~2 s → ~4 s total. `refreshFullGray`
(`0x22 = 0xD7`) is one OTP 4-grey pass, also ~4 s. `wait_ready()` has no
artificial floor, so this is genuine panel time, not driver overhead.

`refreshPartialMono` (~0.7 s) is already a differential mono update; its cost is
dominated by the per-call **deep-sleep exit + hardware reset**, not the waveform
(the 48 KB SPI write is ~19 ms).

## What M5GFX's `Panel_SSD1677.cpp` offers (reference, not currently used)

`managed_components/m5stack__m5gfx/.../Panel_SSD1677.cpp` carries four
hand-tuned LUTs with explicit DC-balance engineering
(`|VSH1| == |VSL|`, symmetric net drive per group). Lab timings from
the M5Stack product docs "M5GFX LUT Refresh Speed":

| M5GFX mode | LUT | `0x22` seq | Time | Character |
|---|---|---|---|---|
| `epd_quality` | `lut_quality` (oscillation prefix + 4-grey tail) | `0x34` (Mode 1) | **4.71 s** | full ghost clear, our current equivalent |
| `epd_text` | `lut_text` (64-frame **absolute** 4-grey) | `0xD4` (Mode 1, temp 0x5A) | **0.45 s** | every pixel → absolute target, *no* long prefix ⇒ still clears ghosting |
| `epd_fast` | `lut_fast` (48-frame absolute, Mode 2) | `0x1C` | **0.34 s** | absolute, ping-pong RAM face |
| `epd_fastest` | `lut_fastest` (differential mono, 2-frame prepulse + 8-frame dose) | `0x1C` | **0.07 s** | only flipped pixels driven; needs a "previously displayed" buffer |

the M5Stack product docs still carries the standing warning: *M5GFX PaperMono waveforms have
been "unstable"; prefer the OTP example; a wrong custom waveform can
**permanently damage** the panel via DC imbalance.* This particular file looks
newer and carefully balanced, but it is **unverified on our unit**.

## Options (independent; can stack)

### A. Replace the 4 s full with a short absolute mono waveform  ⟶ biggest win
Port `lut_text` (or a 1-bit-only trim of it) + its voltages into
`components/ssd1677_otp` as `refreshFastFullMono()`:

- load 105 LUT bytes via `0x32`, voltages via `0x03 / 0x04 / 0x2C`
- write the mono image once, `0x22` single-pass activation
- expected **~0.45 s**, still an absolute redraw ⇒ clears ghosting

Then `DisplayManager` uses it for the DC-balance full, boot, and post-grey
rebuild. `RefreshHint::Quality` (status-bar tap = "clean it NOW") could keep the
OTP `refreshFullMono` as the belt-and-braces path, or also move.

**Risk:** custom waveform → our DC-balance responsibility. Mitigation: lift the
exact bytes from the M5GFX file (already balanced), test on-device watching for
ghosting / black deposits over a few hundred cycles, keep OTP full as fallback.

### B. Keep the panel awake across a burst of partials  ⟶ ~0.7 s → ~0.3 s
`refreshPartialMono` currently does `hardware_reset()` + `deepSleep()` every
call. M5GFX powers the analog once (`_screen_on`) and leaves it up between
updates. Add a "session" around bursts: reset once, N partials, sleep once.
Pure driver change, **no waveform risk**. Already listed as a Phase-4 item in
`ARCHITECTURE.md`.

### C. Reduce how often the 4 s full happens  ⟶ zero waveform risk
- `kPartialLimit` 10 → 12–14. the M5Stack product docs says "approximately 10", a soft number.
- Consume the DC-balance full **at the next screen transition** instead of
  mid-idle, so the flash reads as intentional (plan already suggested this).
- Already done: no-op flushes don't count toward the limit.

## Recommendation

Do **C** now (safe, immediate: fewer 4 s flashes). Prototype **A** behind a
build flag and bake it on-device for a day of normal use before making it the
default — it is the only thing that makes the full refresh itself fast, and the
LUT is already DC-balanced in the reference. **B** is a nice follow-up that
helps every partial.

## Status (2026-09-01)

- **C — applied.** `DisplayManager::kPartialLimit` 10 → 13, and a
  `RefreshHint::Page` flush at ≥ `kPartialFoldIn` (9) partials is promoted to a
  full so the DC-balance clear rides a screen transition.
- **B — deferred, flagged.** No panel risk (no custom waveform; the periodic
  full is kept). Would roughly halve every partial. TODO note in
  `components/ssd1677_otp/src/ssd1677.cpp`.
- **A — deferred, flagged.** Custom-waveform / DC-balance risk; needs a
  build-flag prototype + on-device soak before it can be default.
