/*
 * .m5f bitmap-font reader + glyph blitter.  See docs/FONT_FORMAT.md.
 *
 * A Font wraps a pointer to an .m5f blob (embedded in flash, or loaded from SD
 * into PSRAM) and does not own it. Codepoint lookup is a binary search over the
 * range table.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "framebuffer.h"

namespace gfx {

// Decode one UTF-8 scalar starting at *p (< end); advances p. Returns U+FFFD on
// a malformed sequence (still advancing by one byte).
uint32_t utf8_next(const char*& p, const char* end);

class Font {
public:
    Font() = default;
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // `data` must remain valid for the lifetime of the Font. Returns false if
    // the blob is not a valid v1 .m5f.
    bool load(const uint8_t* data, size_t len);

    // Read a whole .m5f from the filesystem into an owned PSRAM buffer (freed on
    // reload / destruction). Use for per-size reader fonts on the SD card.
    bool loadFromFile(const char* path);

    // Drop any owned buffer and become invalid().
    void unload();

    bool valid() const { return data_ != nullptr; }

    int ascent() const { return asc_; }
    int descent() const { return desc_; }
    int lineGap() const { return gap_; }
    int lineHeight() const { return asc_ + desc_ + gap_; }
    int pxSize() const { return px_; }
    int bpp() const { return bpp_; }

    bool hasGlyph(uint32_t cp) const { return glyphIndex(cp) >= 0; }
    int advanceOf(uint32_t cp) const;

    // Pixel width of a UTF-8 run (len<0 => NUL-terminated).
    int measure(const char* utf8, int len = -1) const;

    // Draw a UTF-8 run with its baseline at (x, baseline_y). Returns the pen x
    // after the run. `ink` is the gray level for set pixels; for a 2-bpp font
    // with ink == kBlack the coverage is used directly (anti-aliased).
    int draw(core::Framebuffer& fb, int x, int baseline_y, const char* utf8,
             uint8_t ink = core::kBlack, int len = -1) const;

private:
    struct Glyph {
        uint32_t bmp;   // offset into bitmap_
        uint8_t w, h;
        int8_t xo, yo;
        int16_t adv;
    };
    int glyphIndex(uint32_t cp) const;
    Glyph glyphAt(int idx) const;
    void blit(core::Framebuffer& fb, int x, int y, const Glyph& g, uint8_t ink) const;

    uint8_t*       owned_  = nullptr;   // non-null when the blob is our PSRAM copy
    const uint8_t* data_   = nullptr;
    const uint8_t* ranges_ = nullptr;   // 12-byte entries
    const uint8_t* glyphs_ = nullptr;   // 10-byte entries
    const uint8_t* bitmap_ = nullptr;
    size_t bitmapLen_ = 0;              // bytes from bitmap_ to end of blob
    uint16_t nRanges_ = 0;
    uint16_t nGlyphs_ = 0;
    int16_t asc_ = 0, desc_ = 0, gap_ = 0;
    uint16_t px_ = 0;
    uint8_t bpp_ = 1;
    uint8_t flags_ = 0;
};

}  // namespace gfx
