/*
 * FT6336G capacitive touch (system I2C bus, addr 0x38, INT on GPIO4).
 *
 * Power and reset are gated by M5IOE1: TP_VDD_EN = PYG13, TP_RST = PYG6
 * (docs.md PinMap). Coordinates are reported in the panel's native portrait
 * space (480 x 800); the controller firmware clamps the active area to
 * X 5..475, Y 5..795.
 */
#pragma once

#include <cstdint>

#include <esp_err.h>

namespace hal::touch {

struct Point {
    uint16_t x = 0;  // portrait, 0..479
    uint16_t y = 0;  // portrait, 0..799
};

// Powers the touch rail via M5IOE1, pulses TP_RST, waits for the controller,
// then probes 0x38. Requires hal::i2c::init(). Returns ESP_OK if 0x38 ACKs.
esp_err_t init();

bool ready();

// Fill `out` with up to `max` active touch points; returns the count (0..2).
int read(Point* out, int max);

}  // namespace hal::touch
