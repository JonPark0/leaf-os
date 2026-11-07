/*
 * Front-light and battery, both via the M5PM1 on the system I2C bus.
 *
 * Front-light: M5PM1 GPIO3 routed to PWM0 (12-bit duty, 0..4095). This is the
 * BL_FB line in product docs; it is the M5PM1's own pin, not ESP32 GPIO3 (which is
 * Button B) — no conflict.
 *
 * Battery: M5PM1 exposes VBAT directly (reg 0x22/0x23, millivolts), so the
 * gated IP2315 path is not needed for a state-of-charge reading.
 */
#pragma once

#include <cstdint>

#include <esp_err.h>

namespace hal::power {

// Routes M5PM1 GPIO3 -> PWM0, sets the PWM frequency, and applies a default
// brightness. Requires hal::i2c::init().
esp_err_t init();

// 0..1000 (per-mille) so the side keys can step finely. 0 turns PWM0 off.
void setBrightness(int per_mille);
int  brightness();

// Convenience for a settings slider / status bar.
void setBrightnessPercent(int pct);
int  brightnessPercent();

// Single-shot battery terminal voltage in millivolts (0 on read error). Jitters
// ~+/-10 mV — prefer batteryMilliVolts() for anything the user sees.
int batteryMilliVoltsRaw();

// Smoothed battery voltage (exponential moving average, seeded on first call).
// Each call folds in one fresh sample, so call it at a steady cadence (~1 Hz).
int batteryMilliVolts();

// Rough single-cell Li-Po state of charge from the smoothed voltage, 0..100
// (-1 if no reading yet).
int batteryPercent();

// Restart the SoC (esp_restart) — used to apply boot-time settings.
[[noreturn]] void reboot();

// Ask the M5PM1 to cut system power (SYS_CMD 0x0C <- 0xA1). On battery this
// never returns. On USB power the rail usually stays up — then this returns
// false after a short wait so the caller can tell the user.
bool shutdown();

}  // namespace hal::power
