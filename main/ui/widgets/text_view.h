/*
 * Read-only text: paragraphs word-wrapped to the width, split into pages when
 * they do not fit. With more than one page a pager column (see pager.h) appears
 * at the right; a vertical swipe also turns the page (up = next).
 *
 * An empty paragraph is a blank gap, half a line tall by default. Text is
 * inset 12 px from both sides. Only this widget's rect repaints on a page turn.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "font.h"
#include "text_wrap.h"
#include "widget.h"

namespace ui {

class TextView : public Widget {
public:
    enum class Align { Left, Center };

    void setFont(const gfx::Font* f) { font_ = f; relayout(); }
    void setAlign(Align a) { align_ = a; invalidate(); }
    // Height of a blank paragraph as a share of the line height, in percent.
    void setGapPercent(int pct) { gap_pct_ = pct; relayout(); }
    // Replaces the text and goes back to the first page.
    void setLines(std::vector<std::string> paragraphs);

    int  page() const { return page_; }
    int  pageCount() const;
    void setPage(int p);

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    static constexpr int kInset = 12;
    static constexpr int kSwipePx = 48;

    void relayout() { laid_out_ = false; invalidate(); }
    void layout() const;           // fills lines_/starts_ for the current bounds and font
    core::Rect textRect() const;   // bounds minus the pager column when it shows

    const gfx::Font* font_ = nullptr;
    Align align_ = Align::Left;
    int gap_pct_ = 50;
    std::vector<std::string> paragraphs_;
    int page_ = 0;
    int down_y_ = -1;

    // Layout cache, rebuilt when the text, font or bounds change.
    mutable bool laid_out_ = false;
    mutable core::Rect laid_bounds_{};
    mutable bool paged_ = false;
    mutable std::vector<text::Line> lines_;
    mutable std::vector<size_t> starts_;
};

}  // namespace ui
