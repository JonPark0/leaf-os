#include "paginator.h"

#include <algorithm>

#include "text_layout.h"

namespace gfx {
namespace {
constexpr int kMaxLinesPerRun = 600;

uint32_t scalarsBetween(const char* a, const char* b)
{
    uint32_t n = 0;
    for (const char* p = a; p < b; ++p)
        if ((static_cast<uint8_t>(*p) & 0xC0) != 0x80) ++n;
    return n;
}
}  // namespace

void Paginator::paginate(const book::Book& bk, const Font& font, const PageMetrics& m)
{
    pages_.clear();
    m_ = m;
    if (!font.valid()) return;

    const int lh = font.lineHeight() + m.line_extra;
    const int rule_h = m.rule_gap * 2 + 2;

    static Line lines[kMaxLinesPerRun];

    int y = 0;                        // filled height on the current page
    bool page_open = false;
    auto startPage = [&](uint32_t run, uint32_t line, uint32_t off) {
        pages_.push_back({run, line, off});
        y = 0;
        page_open = true;
    };

    for (uint32_t ri = 0; ri < bk.runCount(); ++ri) {
        const book::TextRun& r = bk.run(ri);
        const uint32_t run_off = static_cast<uint32_t>(bk.charOffsetOf(ri));

        // Gap before this block (not on the very first line of a page).
        if (page_open && y > 0) y += m.para_gap;

        if (r.rule) {
            if (!page_open || y + rule_h > m.height) startPage(ri, 0, run_off);
            y += rule_h;
            continue;
        }

        const int n = wrap_text(font, r.text.c_str(), m.width, lines, kMaxLinesPerRun);
        for (int li = 0; li < n; ++li) {
            const uint32_t line_off =
                run_off + scalarsBetween(r.text.c_str(), lines[li].begin);
            if (!page_open || y + lh > m.height) {
                startPage(ri, static_cast<uint32_t>(li), line_off);
            }
            y += lh;
        }
        if (n == 0 && !page_open) startPage(ri, 0, run_off);  // empty run safety
    }

    if (pages_.empty()) pages_.push_back({0, 0, 0});
}

int Paginator::pageForOffset(uint32_t offset) const
{
    if (pages_.size() <= 1) return 0;
    // pages_ offsets are non-decreasing.
    int lo = 0, hi = static_cast<int>(pages_.size()) - 1, ans = 0;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (pages_[mid].offset <= offset) { ans = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return ans;
}

}  // namespace gfx
