/*
 * Board bring-up that M5Unified would normally do on a recognized board:
 * quiet the M5PM1 (disable its I2C idle-sleep and watchdog, enable the 3V3
 * rails + charger), and configure the M5IOE1 pins that gate E-Paper power and
 * reset.
 *
 * Exposes the two hooks the SSD1677 driver needs so that component stays free
 * of any I2C / M5Unified dependency.
 */
#pragma once

#include <esp_err.h>

namespace hal::board {

// Requires hal::i2c::init() to have succeeded (called internally if not yet).
// Leaves E-Paper power ENABLED and the panel held out of reset.
esp_err_t init();

// SSD1677 driver hooks (function-pointer compatible).
void epdPower(bool on);     // M5IOE1 PYG3 (EPD_3V3_EN)
void epdResetPulse();       // M5IOE1 PYG5 (EPD_RST): low 10 ms, high 10 ms

}  // namespace hal::board
