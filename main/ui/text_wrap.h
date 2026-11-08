/*
 * Word wrap, pagination and ellipsis for UI text. Host-clean: the font is
 * reached only through a measure callback, so tests/ builds this file.
 *
 * Breaks prefer the position after a space; a word wider than the line is
 * split between UTF-8 scalars (CJK text has no spaces and wraps per character).
 */
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ui::text {

// Width in px of the first `len` bytes of `s`.
using Measure = std::function<int(const char* s, int len)>;

// One display line: a slice of a paragraph, or the gap a blank paragraph leaves.
struct Line {
    std::string text;
    bool gap = false;
};

// Wraps each paragraph to `width` px. An empty paragraph becomes one gap line.
std::vector<Line> wrap(const std::vector<std::string>& paragraphs, int width,
                       const Measure& measure);

// First line index of each page, for pages `page_h` px tall where a text line
// is `line_h` and a gap line `gap_h`. A gap that would open a page is dropped
// (the page starts after it). Always at least one page.
std::vector<size_t> paginate(const std::vector<Line>& lines, int line_h, int gap_h,
                             int page_h);

// Bytes of `s` to draw so it fits in `avail` px: all of it if it fits,
// otherwise the longest prefix (on a UTF-8 boundary) that leaves room for a
// suffix `suffix_w` px wide.
size_t fitPrefix(const std::string& s, int avail, int suffix_w, const Measure& measure);

// Byte length of the UTF-8 sequence that starts with `lead`.
inline int u8len(unsigned char lead) { return lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4; }

}  // namespace ui::text
