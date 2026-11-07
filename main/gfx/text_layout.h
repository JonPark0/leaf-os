/*
 * Line breaking for the reader and for multi-line labels.
 *
 * Latin: greedy word wrap at spaces. CJK / Hangul / Kana: break between any two
 * characters. An explicit '\n' forces a break. Long unbreakable runs are hard
 * cut so a line never exceeds max_width.
 */
#pragma once

#include <cstdint>

#include "font.h"

namespace gfx {

struct Line {
    const char* begin = nullptr;  // into the source string
    const char* end   = nullptr;
    int width         = 0;        // pixels
};

// Fill `out` with up to `max_lines` wrapped lines covering `utf8`. Returns the
// line count. `text` must outlive the returned Line pointers.
int wrap_text(const Font& font, const char* utf8, int max_width, Line* out, int max_lines);

// True if a line break is allowed immediately before/after this codepoint
// (CJK/Hangul/Kana/fullwidth).
bool is_cjk_breakable(uint32_t cp);

}  // namespace gfx
