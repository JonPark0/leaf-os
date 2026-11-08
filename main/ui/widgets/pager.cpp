#include "pager.h"

#include <cstdio>

namespace ui::pager {

void draw(core::Framebuffer& fb, core::Rect col, const gfx::Font& font, int page, int count)
{
    const int cx   = col.x + col.w / 2;
    const int midY = col.y + col.h / 2;
    fb.fillRect({col.x, col.y, 1, col.h}, core::kBlack);   // divider from the content

    // previous (points up), centred in the top half
    const int upY = col.y + col.h / 4 - 5;
    for (int i = 0; i < 10; ++i)
        fb.fillRect({cx - i, upY + i, 2 * i + 1, 1}, core::kBlack);

    // next (points down), centred in the bottom half
    const int dnY = col.y + col.h * 3 / 4 - 5;
    for (int i = 0; i < 10; ++i)
        fb.fillRect({cx - (9 - i), dnY + i, 2 * (9 - i) + 1, 1}, core::kBlack);

    // "n/m" on the split line; there is no divider there, so nothing collides.
    char pg[16];
    std::snprintf(pg, sizeof(pg), "%d/%d", page + 1, count);
    const int pw = font.measure(pg);
    font.draw(fb, cx - pw / 2, midY - font.lineHeight() / 2 + font.ascent(), pg, core::kBlack);
}

}  // namespace ui::pager
