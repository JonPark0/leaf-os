#include "iconset.h"

#include <cstring>

namespace gfx {
namespace {
constexpr size_t kHeader = 12;
constexpr size_t kEntry  = 24;   // name[16] w h stride pad off(u32)
constexpr size_t kNameLen = 16;
}  // namespace

bool IconSet::load(const uint8_t* data, size_t len)
{
    data_ = nullptr;
    if (!data || len < kHeader) return false;
    if (std::memcmp(data, "M5I1", 4) != 0) return false;

    count_ = static_cast<uint16_t>(data[6] | (data[7] << 8));
    cell_  = static_cast<uint16_t>(data[8] | (data[9] << 8));
    if (len < kHeader + static_cast<size_t>(count_) * kEntry) return false;

    data_  = data;
    len_   = len;
    table_ = data + kHeader;
    return true;
}

Icon IconSet::find(const char* name) const
{
    if (!data_ || !name) return {};
    for (uint16_t i = 0; i < count_; ++i) {
        const uint8_t* e = table_ + static_cast<size_t>(i) * kEntry;
        if (std::strncmp(reinterpret_cast<const char*>(e), name, kNameLen) == 0) {
            Icon ic;
            ic.w      = e[16];
            ic.h      = e[17];
            ic.stride = e[18];
            const uint32_t off = static_cast<uint32_t>(e[20]) |
                                 (static_cast<uint32_t>(e[21]) << 8) |
                                 (static_cast<uint32_t>(e[22]) << 16) |
                                 (static_cast<uint32_t>(e[23]) << 24);
            // SD app icons are untrusted: the bitmap must fit in the sheet.
            if (ic.stride * 8 < ic.w || off > len_ ||
                static_cast<size_t>(ic.stride) * ic.h > len_ - off) return {};
            ic.bits = data_ + off;
            return ic;
        }
    }
    return {};
}

void IconSet::draw(core::Framebuffer& fb, int x, int y, const Icon& ic,
                   uint8_t ink) const
{
    if (!ic) return;
    fb.blit1bpp(x, y, ic.bits, ic.w, ic.h, ic.stride, ink);
}

namespace {
const IconSet* s_ui_icons = nullptr;
}
void setUiIcons(const IconSet* s) { s_ui_icons = s; }
const IconSet* uiIcons() { return s_ui_icons; }

}  // namespace gfx
