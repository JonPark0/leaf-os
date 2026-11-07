/*
 * SSD1677 SPI transport for PaperMono-Lite.
 *
 * SPI logic ported from m5stack/M5PaperMono-OTP-Demo
 * (components/EDP_OTP_LUT_demo/src/EDP_SPI.h), MIT licensed,
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD.
 * Local change: the panel power / reset lines (on M5IOE1) are reached through
 * injected hooks instead of M5.getIOExpander(), so this component has no
 * M5Unified dependency.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "epd_hooks.h"

namespace edp_spi {

using HwHooks = ssd1677::HwHooks;

// One-time transport bring-up: stores the hooks, configures CS/DC/BUSY GPIO,
// initializes SPI2 at 20 MHz mode 0 half-duplex, allocates the DMA staging
// buffer, and issues a hardware reset. Returns false and logs on any failure.
bool begin(const HwHooks& hooks);

// Bus session bracketing. begin_write() acquires the SPI bus and asserts CS;
// end_write() releases both and returns the accumulated write_ok status.
bool begin_write();
bool end_write();

// Poll BUSY (active high) until low or timeout. Returns false on timeout.
bool wait_ready(uint32_t timeout_ms = 15000);

// Pulse the panel reset line via M5IOE1 (also drives EPD power-enable high).
void hardware_reset();

void write_command(uint8_t command);
void write_data(const uint8_t* data, size_t length, bool invert = false);
void write_register(uint8_t command, std::initializer_list<uint8_t> data);
void write_u16(uint16_t value);  // little-endian

}  // namespace edp_spi
