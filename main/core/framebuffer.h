/*
 * Portrait 480 x 800 framebuffer, one byte per pixel holding a gray level:
 *
 *   0 = white   1 = light gray   2 = dark gray   3 = black
 *
 * 1-bit UI uses only 0 and 3. Backed by PSRAM. DisplayManager owns the
 * instance; widgets draw through it, then DisplayManager transposes + packs
 * dirty regions for the SSD1677 (see framebuffer.cpp packing helpers).
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "geometry.h"

namespace core {

enum Gray : uint8_t { kWhite = 0, kLight = 1, kDark = 2, kBlack = 3 };

class Framebuffer {
public:
    static constexpr int kW = 480;
    static constexpr int kH = 800;
    static constexpr size_t kPixels = static_cast<size_t>(kW) * kH;

    bool init();                       // allocates PSRAM; false on OOM
    int width() const { return kW; }
    int height() const { return kH; }

    uint8_t* data() { return buf_; }
    const uint8_t* data() const { return buf_; }
    uint8_t* row(int y) { return buf_ + static_cast<size_t>(y) * kW; }

    void clear(uint8_t gray);
    void setPixel(int x, int y, uint8_t gray)
    {
        if (static_cast<unsigned>(x) < kW && static_cast<unsigned>(y) < kH) {
            buf_[static_cast<size_t>(y) * kW + x] = gray;
        }
    }
    uint8_t pixel(int x, int y) const
    {
        return (static_cast<unsigned>(x) < kW && static_cast<unsigned>(y) < kH)
                   ? buf_[static_cast<size_t>(y) * kW + x]
                   : static_cast<uint8_t>(kWhite);
    }
    void fillRect(Rect r, uint8_t gray);
    void frameRect(Rect r, int t, uint8_t gray);

    // Blit a 1-bpp MSB-first bitmap (glyphs). Set bits -> fg, clear bits -> bg
    // unless bg == kTransparent.
    static constexpr uint8_t kTransparent = 0xFF;
    void blit1bpp(int x, int y, const uint8_t* bits, int w, int h, int stride_bytes,
                  uint8_t fg, uint8_t bg = kTransparent);

    // True if every pixel in `r` is white or black (partial refresh is legal).
    bool rectIsMono(Rect r) const;

private:
    uint8_t* buf_ = nullptr;
};

}  // namespace core
