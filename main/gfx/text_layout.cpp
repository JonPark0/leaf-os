#include "text_layout.h"

#include <cstring>

namespace gfx {

bool is_cjk_breakable(uint32_t cp)
{
    return (cp >= 0x3000 && cp <= 0x30FF) ||   // CJK symbols, kana
           (cp >= 0x3130 && cp <= 0x318F) ||   // Hangul compatibility jamo
           (cp >= 0x3400 && cp <= 0x4DBF) ||   // CJK ext-A
           (cp >= 0x4E00 && cp <= 0x9FFF) ||   // CJK unified
           (cp >= 0xAC00 && cp <= 0xD7A3) ||   // Hangul syllables
           (cp >= 0xF900 && cp <= 0xFAFF) ||   // CJK compat ideographs
           (cp >= 0xFF00 && cp <= 0xFFEF);     // fullwidth forms
}

int wrap_text(const Font& font, const char* text, int max_width, Line* out, int max_lines)
{
    if (text == nullptr || out == nullptr || max_lines <= 0) return 0;
    const char* end = text + std::strlen(text);

    int nlines = 0;
    const char* line_begin = text;
    const char* p = text;

    int line_w = 0;
    const char* brk_pos = nullptr;   // byte pos to break at
    int brk_w = 0;                   // line width up to brk_pos

    auto emit = [&](const char* stop, int w) {
        if (nlines < max_lines) out[nlines++] = {line_begin, stop, w};
    };

    while (p < end) {
        const char* cp_start = p;
        const uint32_t cp = utf8_next(p, end);
        if (cp == 0) break;

        if (cp == '\n') {
            emit(cp_start, line_w);
            line_begin = p;
            line_w = 0;
            brk_pos = nullptr;
            if (nlines >= max_lines) return nlines;
            continue;
        }

        const int adv = font.advanceOf(cp);

        // Record a break opportunity: after a space, or between CJK chars.
        if (cp == ' ') {
            brk_pos = p;             // break after the space
            brk_w = line_w;          // trailing space not counted on this line
        } else if (is_cjk_breakable(cp)) {
            brk_pos = cp_start;      // break before this char
            brk_w = line_w;
        }

        if (line_w + adv > max_width && line_w > 0) {
            if (brk_pos != nullptr && brk_pos > line_begin) {
                emit(brk_pos, brk_w);
                line_begin = brk_pos;
                // re-measure the remainder that was already consumed past brk
                line_w = font.measure(line_begin, static_cast<int>(p - line_begin));
            } else {
                emit(cp_start, line_w);         // hard cut before this glyph
                line_begin = cp_start;
                line_w = adv;
            }
            brk_pos = nullptr;
            if (nlines >= max_lines) return nlines;
        } else {
            line_w += adv;
        }
    }

    if (line_begin < end || nlines == 0) {
        emit(end, line_w);
    }
    return nlines;
}

}  // namespace gfx
