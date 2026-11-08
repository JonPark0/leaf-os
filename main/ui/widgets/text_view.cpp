#include "text_view.h"

#include <algorithm>

#include "pager.h"
#include "text_draw.h"

namespace ui {

void TextView::setLines(std::vector<std::string> paragraphs)
{
    paragraphs_ = std::move(paragraphs);
    page_ = 0;
    relayout();
}

int TextView::pageCount() const
{
    layout();
    return static_cast<int>(starts_.size());
}

void TextView::setPage(int p)
{
    p = std::clamp(p, 0, pageCount() - 1);
    if (p == page_) return;
    page_ = p;
    invalidate();
}

core::Rect TextView::textRect() const
{
    core::Rect r = bounds_;
    if (paged_) r.w -= pager::kWidth;
    return r;
}

void TextView::layout() const
{
    if (laid_out_ && laid_bounds_ == bounds_) return;
    laid_out_ = true;
    laid_bounds_ = bounds_;
    lines_.clear();
    starts_.assign(1, 0);
    paged_ = false;
    if (!font_ || !font_->valid() || bounds_.w <= 2 * kInset) return;

    const text::Measure m = text::measureWith(*font_);
    const int lh = font_->lineHeight();
    const int gap = std::max(1, lh * gap_pct_ / 100);

    // Try the full width first; only make room for the pager when it is needed.
    lines_ = text::wrap(paragraphs_, bounds_.w - 2 * kInset, m);
    starts_ = text::paginate(lines_, lh, gap, bounds_.h);
    if (starts_.size() > 1) {
        paged_ = true;
        lines_ = text::wrap(paragraphs_, bounds_.w - pager::kWidth - 2 * kInset, m);
        starts_ = text::paginate(lines_, lh, gap, bounds_.h);
    }
}

void TextView::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);
    layout();
    if (!font_ || !font_->valid()) return;
    page_ = std::clamp(page_, 0, static_cast<int>(starts_.size()) - 1);

    const core::Rect tr = textRect();
    const int lh = font_->lineHeight();
    const int gap = std::max(1, lh * gap_pct_ / 100);
    const size_t first = starts_[static_cast<size_t>(page_)];
    const size_t last = static_cast<size_t>(page_) + 1 < starts_.size()
                            ? starts_[static_cast<size_t>(page_) + 1] : lines_.size();
    int y = tr.y;
    for (size_t i = first; i < last; ++i) {
        const text::Line& ln = lines_[i];
        if (ln.gap) { y += gap; continue; }
        if (y + lh > tr.y + tr.h) break;
        int x = tr.x + kInset;
        if (align_ == Align::Center) x = tr.x + (tr.w - font_->measure(ln.text.c_str())) / 2;
        font_->draw(fb, x, y + font_->ascent(), ln.text.c_str(), core::kBlack);
        y += lh;
    }
    if (paged_) pager::draw(fb, pager::column(bounds_), *font_, page_, pageCount());
}

bool TextView::onTouch(int x, int y, TouchPhase phase)
{
    if (pageCount() <= 1) return false;   // nothing to turn: let taps through
    switch (phase) {
        case TouchPhase::Down:
            down_y_ = y;
            return true;
        case TouchPhase::Move:
            return true;
        case TouchPhase::Up: {
            const int dy = down_y_ < 0 ? 0 : y - down_y_;
            down_y_ = -1;
            if (dy <= -kSwipePx)      setPage(page_ + 1);     // swipe up: next
            else if (dy >= kSwipePx)  setPage(page_ - 1);
            else if (x >= pager::column(bounds_).x)
                setPage(page_ + pager::step(pager::column(bounds_), y));
            return true;
        }
    }
    return false;
}

}  // namespace ui
