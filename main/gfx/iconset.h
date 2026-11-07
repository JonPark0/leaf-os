/*
 * .m5i 1-bit icon sheet reader.  See tools/icongen/icongen.py for the format.
 *
 * A sheet is a flat blob (embedded in flash) holding N square-ish 1-bpp icons,
 * MSB-first. Lookup is by short name (linear scan — N is small). Icons are
 * drawn through core::Framebuffer::blit1bpp, so they honour the same
 * transparent-background rule as glyphs.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "framebuffer.h"

namespace gfx {

struct Icon {
    uint8_t w = 0, h = 0, stride = 0;
    const uint8_t* bits = nullptr;   // stride * h bytes, MSB-first
    explicit operator bool() const { return bits != nullptr; }
};

class IconSet {
public:
    bool load(const uint8_t* data, size_t len);
    bool valid() const { return data_ != nullptr; }
    int count() const { return count_; }
    int cellPx() const { return cell_; }

    // Returns an empty Icon (operator bool == false) if `name` is unknown.
    Icon find(const char* name) const;

    // Blit `ic` with its top-left at (x, y). No-op for an empty icon.
    void draw(core::Framebuffer& fb, int x, int y, const Icon& ic,
              uint8_t ink = core::kBlack) const;

    // Convenience: find + draw in one call.
    void draw(core::Framebuffer& fb, int x, int y, const char* name,
              uint8_t ink = core::kBlack) const
    {
        draw(fb, x, y, find(name), ink);
    }

private:
    const uint8_t* data_  = nullptr;
    const uint8_t* table_ = nullptr;   // count_ * 24-byte entries
    size_t len_     = 0;
    uint16_t count_ = 0;
    uint16_t cell_  = 0;
};

// Process-wide UI icon sheet, set once at boot from main. Widgets that are not
// apps (status bar, list rows) read it here; may be null if loading failed.
void setUiIcons(const IconSet* s);
const IconSet* uiIcons();

}  // namespace gfx
