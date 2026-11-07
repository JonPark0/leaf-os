#include "font.h"

#include <cstdio>
#include <cstring>

#include <esp_log.h>

#include "hal_sys.h"

namespace gfx {
namespace {
constexpr char TAG[] = "font";

// Unaligned little-endian reads (the blob may sit at any flash offset).
uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
int16_t  rds16(const uint8_t* p) { return static_cast<int16_t>(rd16(p)); }
uint32_t rd32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

uint32_t utf8_next(const char*& p, const char* end)
{
    if (p >= end) return 0;
    const uint8_t c = static_cast<uint8_t>(*p++);
    if (c < 0x80) return c;
    int extra;
    uint32_t cp;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else return 0xFFFD;
    for (int i = 0; i < extra; ++i) {
        if (p >= end || (static_cast<uint8_t>(*p) & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (static_cast<uint8_t>(*p++) & 0x3F);
    }
    return cp;
}

bool Font::load(const uint8_t* data, size_t len)
{
    data_ = nullptr;
    if (data == nullptr || len < 32) return false;
    if (std::memcmp(data, "M5F1", 4) != 0 || rd16(data + 4) != 1) {
        ESP_LOGE(TAG, "bad magic/version");
        return false;
    }
    bpp_     = data[6];
    flags_   = data[7];
    asc_     = rds16(data + 8);
    desc_    = rds16(data + 10);
    gap_     = rds16(data + 12);
    px_      = rd16(data + 14);
    nRanges_ = rd16(data + 16);
    nGlyphs_ = rd16(data + 18);
    const uint32_t roff = rd32(data + 20);
    const uint32_t goff = rd32(data + 24);
    const uint32_t boff = rd32(data + 28);
    // SD fonts are untrusted: every table must fit inside the blob.
    if (boff > len || goff > len || roff > len ||
        static_cast<size_t>(nRanges_) * 12 > len - roff ||
        static_cast<size_t>(nGlyphs_) * 10 > len - goff) {
        ESP_LOGE(TAG, "table offset out of range");
        return false;
    }
    data_   = data;
    ranges_ = data + roff;
    glyphs_ = data + goff;
    bitmap_ = data + boff;
    bitmapLen_ = len - boff;
    ESP_LOGI(TAG, "loaded %upx bpp%u, %u glyphs / %u ranges", px_, bpp_, nGlyphs_, nRanges_);
    return true;
}

Font::~Font() { unload(); }

void Font::unload()
{
    if (owned_) { hal::sys::freeBuffer(owned_); owned_ = nullptr; }
    data_ = nullptr;
}

bool Font::loadFromFile(const char* path)
{
    unload();
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) { ESP_LOGW(TAG, "open %s failed", path); return false; }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 32 || sz > 4 * 1024 * 1024) {
        std::fclose(f);
        ESP_LOGW(TAG, "%s: implausible size %ld", path, sz);
        return false;
    }
    uint8_t* buf = static_cast<uint8_t*>(hal::sys::allocLarge(static_cast<size_t>(sz)));
    if (buf == nullptr) {
        buf = static_cast<uint8_t*>(hal::sys::allocInternal(static_cast<size_t>(sz)));
    }
    if (buf == nullptr) { std::fclose(f); ESP_LOGE(TAG, "font alloc %ld failed", sz); return false; }

    const size_t got = std::fread(buf, 1, static_cast<size_t>(sz), f);
    std::fclose(f);
    if (got != static_cast<size_t>(sz) || !load(buf, got)) {
        hal::sys::freeBuffer(buf);
        ESP_LOGW(TAG, "%s: read/parse failed", path);
        return false;
    }
    owned_ = buf;
    ESP_LOGI(TAG, "loaded %s (%ld B) from file", path, sz);
    return true;
}

int Font::glyphIndex(uint32_t cp) const
{
    if (data_ == nullptr) return -1;
    int lo = 0, hi = nRanges_ - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) >> 1;
        const uint8_t* r = ranges_ + mid * 12;
        const uint32_t first = rd32(r);
        const uint32_t last  = rd32(r + 4);
        if (cp < first)      hi = mid - 1;
        else if (cp > last)  lo = mid + 1;
        else {
            const uint32_t idx = rd32(r + 8) + (cp - first);
            return idx < nGlyphs_ ? static_cast<int>(idx) : -1;
        }
    }
    return -1;
}

Font::Glyph Font::glyphAt(int idx) const
{
    const uint8_t* g = glyphs_ + idx * 10;
    return Glyph{
        rd32(g),
        g[4], g[5],
        static_cast<int8_t>(g[6]), static_cast<int8_t>(g[7]),
        rds16(g + 8),
    };
}

int Font::advanceOf(uint32_t cp) const
{
    const int idx = glyphIndex(cp);
    return idx < 0 ? px_ / 2 : glyphAt(idx).adv;
}

int Font::measure(const char* s, int len) const
{
    if (s == nullptr) return 0;
    const char* p = s;
    const char* end = (len < 0) ? s + std::strlen(s) : s + len;
    int w = 0;
    while (p < end) {
        const uint32_t cp = utf8_next(p, end);
        if (cp == 0) break;
        if (cp == '\n') continue;
        w += advanceOf(cp);
    }
    return w;
}

void Font::blit(core::Framebuffer& fb, int x, int y, const Glyph& g, uint8_t ink) const
{
    if (g.w == 0 || g.h == 0) return;
    const int row_bytes = (g.w * bpp_ + 7) / 8;
    if (g.bmp > bitmapLen_ ||
        static_cast<size_t>(row_bytes) * g.h > bitmapLen_ - g.bmp) return;
    const uint8_t* bmp = bitmap_ + g.bmp;
    for (int gy = 0; gy < g.h; ++gy) {
        const uint8_t* srow = bmp + gy * row_bytes;
        const int py = y + gy;
        for (int gx = 0; gx < g.w; ++gx) {
            if (bpp_ == 1) {
                if ((srow[gx >> 3] >> (7 - (gx & 7))) & 1) {
                    fb.setPixel(x + gx, py, ink);
                }
            } else {  // bpp == 2, coverage 0..3
                const uint8_t cov = (srow[gx >> 2] >> (6 - 2 * (gx & 3))) & 3;
                if (cov == 0) continue;
                const uint8_t v = (ink == core::kBlack) ? cov
                                  : (cov >= 2 ? ink : core::kLight);
                fb.setPixel(x + gx, py, v);
            }
        }
    }
}

int Font::draw(core::Framebuffer& fb, int x, int baseline_y, const char* s,
               uint8_t ink, int len) const
{
    if (data_ == nullptr || s == nullptr) return x;
    const char* p = s;
    const char* end = (len < 0) ? s + std::strlen(s) : s + len;
    int pen = x;
    while (p < end) {
        const uint32_t cp = utf8_next(p, end);
        if (cp == 0) break;
        if (cp == '\n') continue;
        const int idx = glyphIndex(cp);
        if (idx < 0) { pen += px_ / 2; continue; }
        const Glyph g = glyphAt(idx);
        blit(fb, pen + g.xo, baseline_y + g.yo, g, ink);
        pen += g.adv;
    }
    return pen;
}

}  // namespace gfx
