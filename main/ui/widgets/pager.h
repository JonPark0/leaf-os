/*
 * The page-turn column shared by ListView and TextView: a 1 px divider on its
 * left, an up arrow in the top half, a down arrow in the bottom half and "n/m"
 * on the split line. Tapping the top half goes back a page, the bottom half
 * forward. Paging instead of smooth scrolling keeps e-ink to one partial
 * refresh per turn.
 */
#pragma once

#include "font.h"
#include "framebuffer.h"
#include "geometry.h"

namespace ui::pager {

constexpr int kWidth = 44;

// The pager column at the right edge of `area`.
inline core::Rect column(core::Rect area) { return {area.right() - kWidth, area.y, kWidth, area.h}; }

// `page` is 0-based.
void draw(core::Framebuffer& fb, core::Rect col, const gfx::Font& font, int page, int count);

// -1 for a tap in the top half of `col`, +1 for the bottom half.
inline int step(core::Rect col, int y) { return y < col.y + col.h / 2 ? -1 : +1; }

}  // namespace ui::pager
