/*
 * Splits a book::Book into page boundaries for a fixed content box and font.
 *
 * Only the page-start positions are stored; the reader re-wraps the runs on the
 * current page when it draws, so memory stays O(pages) not O(lines). A page
 * start is a (run, line-within-run) pair plus the document scalar offset, so a
 * reading position survives a re-pagination at a different size.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "book.h"
#include "font.h"

namespace gfx {

struct PageMetrics {
    int width   = 432;   // content box, px
    int height  = 740;
    int para_gap = 10;   // extra space between blocks
    int rule_gap = 12;   // space above+below a horizontal rule
    int line_extra = 0;  // added to font.lineHeight() for every body line
};

struct PagePos {
    uint32_t run   = 0;   // index into Book
    uint32_t line  = 0;   // wrapped-line index within that run (0 = run start)
    uint32_t offset = 0;  // document scalar offset of this position
};

class Paginator {
public:
    // Builds the page table. `font` and `book` must outlive further queries
    // only if you call reflow(); the table itself is self-contained.
    void paginate(const book::Book& book, const Font& font, const PageMetrics& m);

    int pageCount() const { return static_cast<int>(pages_.size()); }
    const PagePos& page(int i) const { return pages_[i]; }

    // Page whose [start,next) scalar-offset range contains `offset`
    // (clamped to a valid index).
    int pageForOffset(uint32_t offset) const;

    const PageMetrics& metrics() const { return m_; }

private:
    std::vector<PagePos> pages_;
    PageMetrics m_;
};

}  // namespace gfx
