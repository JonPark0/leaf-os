/*
 * Shared screen geometry. Every app screen lays out from these instead of its
 * own numbers, so the header and body line up on all of them.
 *
 * The status bar height is set at runtime from the UI font (StatusBar::setFont,
 * run by Kernel::setUiFont before the first app is created), so everything
 * below it is a function, read in onCreate, not a constant.
 *
 *   +------------------------------------------+  y = 0
 *   | status bar (system strip, StatusBar)      |
 *   +------------------------------------------+  y = kHeight
 *   | [back]        title                       |  header
 *   +------------------------------------------+  y = bodyTop()
 *   | body                                      |
 *   +------------------------------------------+  y = kScreenH
 */
#pragma once

#include "framebuffer.h"
#include "geometry.h"
#include "status_bar.h"

namespace ui::metrics {

constexpr int kScreenW = core::Framebuffer::kW;   // 480, portrait
constexpr int kScreenH = core::Framebuffer::kH;   // 800
constexpr int kMargin  = 16;                      // left/right inset for body content

inline int statusBarHeight() { return StatusBar::kHeight; }

// The back control at the left of the header.
inline core::Rect backButton() { return {8, StatusBar::kHeight + 8, 104, 42}; }

// A title centred on the screen, `w` px wide (220 by default; 240 when the
// right side of the header is free).
inline core::Rect titleCentered(int w = 220)
{
    return {(kScreenW - w) / 2, StatusBar::kHeight + 10, w, 40};
}

// A left-aligned title that runs from the back button to the right edge, for
// long titles such as a folder path.
inline core::Rect titleBesideBack() { return {120, StatusBar::kHeight + 12, kScreenW - 128, 40}; }

// Top of the body, below the header.
inline int bodyTop() { return StatusBar::kHeight + 60; }

// The whole body: full width, header to the bottom of the screen.
inline core::Rect body() { return {0, bodyTop(), kScreenW, kScreenH - bodyTop()}; }

// Below the status bar: the whole area an app owns.
inline core::Rect content() { return {0, StatusBar::kHeight, kScreenW, kScreenH - StatusBar::kHeight}; }

}  // namespace ui::metrics
