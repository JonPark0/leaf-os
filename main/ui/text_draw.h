/*
 * Drawing helpers on top of gfx::Font for text that must stay inside a box.
 */
#pragma once

#include <string>

#include "font.h"
#include "framebuffer.h"
#include "text_wrap.h"

namespace ui::text {

// Adapts a font to the Measure callback in text_wrap.h.
inline Measure measureWith(const gfx::Font& f)
{
    return [&f](const char* s, int len) { return f.measure(s, len); };
}

// Draws `s` at (x, baseline), cut to `avail` px with a trailing "..." when it
// does not fit. ASCII dots, because the embedded font's U+2026 glyph is blank.
void drawEllipsized(core::Framebuffer& fb, const gfx::Font& f, int x, int baseline,
                    const std::string& s, int avail, uint8_t ink = core::kBlack);

}  // namespace ui::text
