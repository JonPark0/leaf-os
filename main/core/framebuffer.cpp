#include "framebuffer.h"

#include <cstring>

#include <esp_log.h>

#include "hal_sys.h"

namespace core {

bool Framebuffer::init()
{
    if (buf_ != nullptr) {
        return true;
    }
    buf_ = static_cast<uint8_t*>(hal::sys::allocLarge(kPixels));
    if (buf_ == nullptr) {
        ESP_LOGE("fb", "OOM (%u B PSRAM)", static_cast<unsigned>(kPixels));
        return false;
    }
    std::memset(buf_, kWhite, kPixels);
    return true;
}

void Framebuffer::clear(uint8_t gray)
{
    std::memset(buf_, gray, kPixels);
}

void Framebuffer::fillRect(Rect r, uint8_t gray)
{
    r = r.clampedTo(kW, kH);
    for (int y = r.y; y < r.bottom(); ++y) {
        std::memset(buf_ + static_cast<size_t>(y) * kW + r.x, gray, r.w);
    }
}

void Framebuffer::frameRect(Rect r, int t, uint8_t gray)
{
    if (r.empty() || t <= 0) return;
    fillRect({r.x, r.y, r.w, t}, gray);
    fillRect({r.x, r.bottom() - t, r.w, t}, gray);
    fillRect({r.x, r.y, t, r.h}, gray);
    fillRect({r.right() - t, r.y, t, r.h}, gray);
}

void Framebuffer::blit1bpp(int x, int y, const uint8_t* bits, int w, int h, int stride_bytes,
                           uint8_t fg, uint8_t bg)
{
    for (int gy = 0; gy < h; ++gy) {
        const int py = y + gy;
        if (static_cast<unsigned>(py) >= kH) continue;
        const uint8_t* srow = bits + static_cast<size_t>(gy) * stride_bytes;
        for (int gx = 0; gx < w; ++gx) {
            const bool on = (srow[gx >> 3] >> (7 - (gx & 7))) & 1;
            if (on) {
                setPixel(x + gx, py, fg);
            } else if (bg != kTransparent) {
                setPixel(x + gx, py, bg);
            }
        }
    }
}

bool Framebuffer::rectIsMono(Rect r) const
{
    r = r.clampedTo(kW, kH);
    for (int y = r.y; y < r.bottom(); ++y) {
        const uint8_t* p = buf_ + static_cast<size_t>(y) * kW + r.x;
        for (int x = 0; x < r.w; ++x) {
            if (p[x] != kWhite && p[x] != kBlack) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace core
