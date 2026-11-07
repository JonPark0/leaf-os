/*
 * System I2C bus (GPIO47 SDA / GPIO48 SCL).
 *
 * We drive it ourselves with the ESP-IDF i2c_master driver instead of relying
 * on M5Unified, whose released build does not autodetect the PaperMono-Lite
 * (no NFC) and therefore never creates the IO expander.
 *
 * All access is serialized by a single mutex. The charger IC (IP2315, 0x75) is
 * NOT touched here; per product docs it must be gated onto the bus via M5IOE1
 * PYG11 only for the duration of a transaction (see hal_power, later phase).
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_err.h>

namespace hal::i2c {

// System-bus device addresses.
constexpr uint8_t kAddrRtc    = 0x32;  // RX8130CE
constexpr uint8_t kAddrTouch  = 0x38;  // FT6336G (only ACKs after IOE1 powers TP)
constexpr uint8_t kAddrIoe1   = 0x4F;  // M5IOE1
constexpr uint8_t kAddrImu    = 0x68;  // BMI270
constexpr uint8_t kAddrPm1    = 0x6E;  // M5PM1
constexpr uint8_t kAddrCharger = 0x75; // IP2315 (gated; do not access directly)

// Create the shared bus. Safe to call once; further calls are no-ops.
esp_err_t init();

// Mutex-protected 8-bit register access. `freq_hz` selects the per-transfer
// clock (100k default; some devices tolerate 400k).
esp_err_t readReg8(uint8_t dev_addr, uint8_t reg, uint8_t* out, uint32_t freq_hz = 100000);
esp_err_t readRegN(uint8_t dev_addr, uint8_t reg, uint8_t* out, size_t len, uint32_t freq_hz = 100000);
esp_err_t writeReg8(uint8_t dev_addr, uint8_t reg, uint8_t val, uint32_t freq_hz = 100000);

// Read-modify-write: `*reg = (*reg & ~clear_mask) | set_mask`.
esp_err_t rmw8(uint8_t dev_addr, uint8_t reg, uint8_t clear_mask, uint8_t set_mask,
               uint32_t freq_hz = 100000);

// Probe: returns ESP_OK if the address ACKs.
esp_err_t probe(uint8_t dev_addr);

// Take / release the bus mutex around a multi-step sequence that must be
// atomic with respect to other callers (e.g. the gated charger read).
void lock();
void unlock();

}  // namespace hal::i2c
