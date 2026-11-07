/*
 * SSD1677 panel driver for PaperMono-Lite.
 *
 * Command sequences are lifted from m5stack/M5PaperMono-OTP-Demo
 * (MIT, SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD) and reshaped
 * into a driver that operates on caller-owned plane buffers instead of holding
 * its own framebuffers. core::DisplayManager is the only intended caller.
 *
 * See docs/ARCHITECTURE.md for geometry, plane semantics and refresh modes.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "epd_hooks.h"  // ssd1677::HwHooks

namespace ssd1677 {

// Visible panel: 480 x 800 portrait (what the UI composes in).
constexpr int PANEL_W = 480;
constexpr int PANEL_H = 800;

// Controller-native: 800 x 480 landscape. Plane buffers are packed in THIS
// orientation, 1 bit per pixel, MSB first. bit 1 = white, bit 0 = black.
constexpr int RAM_W        = 800;
constexpr int RAM_H        = 480;
constexpr int RAM_STRIDE   = RAM_W / 8;                       // 100 bytes/row
constexpr size_t PLANE_BYTES = static_cast<size_t>(RAM_STRIDE) * RAM_H;  // 48000

// Rectangle in controller RAM (landscape) coordinates.
struct Rect {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
};

// Transport bring-up + load a white RAM baseline WITHOUT a visible refresh, so
// the first on-screen operation after boot can be a clean partial update.
// `hooks` supplies panel power/reset (see epd_hooks.h).
bool init(const HwHooks& hooks);

// True once a valid monochrome baseline exists in controller RAM (required by
// refreshPartialMono). Cleared by refreshFullGray().
bool baselineValid();

// Partial monochrome refresh. `plane` is a full RAM_W x RAM_H MSB-packed mono
// buffer; the entire frame is streamed to the controller (not just the rect).
// `rect` is kept for a future windowed path; currently ignored.
// No-op returning false if no baseline is valid.
bool refreshPartialMono(const uint8_t* plane, Rect rect);

// Full-frame monochrome refresh using OTP Mode 1 (two-phase, brief flash).
// Rebuilds the partial baseline. `plane` is RAM_W x RAM_H MSB-packed.
bool refreshFullMono(const uint8_t* plane);

// Full-frame 4-gray refresh using the OTP gray waveform. `plane1`/`plane2` each
// contribute one bit per pixel: white=00 lightgray=10 darkgray=01 black=11.
// INVALIDATES the partial baseline: the next update must be refreshFullMono().
bool refreshFullGray(const uint8_t* plane1, const uint8_t* plane2);

// Enter Deep Sleep Mode 1 (RAM retained; exited by the next refresh call's
// hardware reset). Called automatically at the end of each refresh.
void deepSleep();

}  // namespace ssd1677
