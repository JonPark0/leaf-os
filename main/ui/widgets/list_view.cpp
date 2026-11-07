#include "list_view.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "iconset.h"

namespace ui {
namespace {

// Draw `s` at (x, baseline), clipped to `avail` px with a trailing "..." when it
// does not fit. Truncation lands on UTF-8 scalar boundaries.
void drawClipped(core::Framebuffer& fb, const gfx::Font& f, int x, int baseline,
                 const std::string& s, int avail)
{
    if (avail <= 0 || s.empty()) return;
    if (f.measure(s.c_str()) <= avail) {
        f.draw(fb, x, baseline, s.c_str(), core::kBlack);
        return;
    }
    const char* ell = "...";           // embedded font's U+2026 glyph is blank
    const int ew = f.measure(ell);
    int n = static_cast<int>(s.size());
    while (n > 0) {
        --n;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
        if (f.measure(s.c_str(), n) + ew <= avail) break;
    }
    f.draw(fb, x, baseline, s.c_str(), core::kBlack, n);
    f.draw(fb, x + f.measure(s.c_str(), n), baseline, ell, core::kBlack);
}

}  // namespace

void ListView::setItems(std::vector<std::string> items)
{
    items_ = std::move(items);
    page_ = 0;
    invalidate();
}

void ListView::setItem(int i, const std::string& s)
{
    if (i < 0 || i >= count()) return;
    if (items_[i] == s) return;
    items_[i] = s;
    invalidate();
}

namespace { int s_row_pad = 0; }
void ListView::setGlobalRowPadding(int px) { s_row_pad = px < 0 ? 0 : px; }
int  ListView::globalRowPadding() { return s_row_pad; }

int ListView::rowHeight() const
{
    return font_ && font_->valid() ? font_->lineHeight() + 14 + s_row_pad : 30;
}

int ListView::rowsPerPage() const
{
    return std::max(1, bounds_.h / rowHeight());
}

int ListView::pageCount() const
{
    const int rpp = rowsPerPage();
    return std::max(1, (count() + rpp - 1) / rpp);
}

void ListView::setPage(int p)
{
    p = std::clamp(p, 0, pageCount() - 1);
    if (p == page_) return;
    page_ = p;
    invalidate();
}

void ListView::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);
    if (!font_ || !font_->valid()) return;

    const int rh = rowHeight();
    const int rpp = rowsPerPage();
    const int start = page_ * rpp;
    const int list_w = bounds_.w - kPagerW;

    for (int r = 0; r < rpp && start + r < count(); ++r) {
        const int row = start + r;
        const int y = bounds_.y + r * rh;
        if (r > 0) fb.fillRect({bounds_.x, y, list_w, 1}, core::kBlack);  // separator (1-bit only)

        const gfx::Font* rowf = font_for_ ? font_for_(row) : nullptr;
        const gfx::Font& f = (rowf && rowf->valid()) ? *rowf : *font_;
        const int by = y + (rh - f.lineHeight()) / 2 + f.ascent();

        // Right-to-left: chevron, then optional badge, then optional detail
        // column; whatever is left of that is the primary text.
        int text_right = bounds_.x + list_w - 30;   // just left of the chevron

        if (icon_for_) {
            if (const char* nm = icon_for_(row)) {
                if (const gfx::IconSet* is = gfx::uiIcons()) {
                    const gfx::Icon ic = is->find(nm);
                    if (ic) {
                        is->draw(fb, bounds_.x + list_w - 30 - ic.w,
                                 y + (rh - ic.h) / 2, ic, core::kBlack);
                        text_right -= ic.w + 8;
                    }
                }
            }
        }

        if (detail_for_) {
            const std::string d = detail_for_(row);
            if (!d.empty()) {
                const int dw = f.measure(d.c_str());
                f.draw(fb, text_right - dw, by, d.c_str(), core::kBlack);
                text_right -= dw + 12;
            }
        }

        const int text_left = bounds_.x + 12;
        drawClipped(fb, f, text_left, by, items_[row], text_right - text_left);

        // chevron
        const int cx = bounds_.x + list_w - 18, cy = y + rh / 2;
        for (int k = 0; k < 6; ++k) {
            fb.setPixel(cx + k, cy - 6 + k, core::kBlack);
            fb.setPixel(cx + k, cy + 6 - k, core::kBlack);
        }
    }

    // pager column: prev (top half) / next (bottom half), split at midY.
    const int px   = bounds_.right() - kPagerW;
    const int cx   = px + kPagerW / 2;
    const int midY = bounds_.y + bounds_.h / 2;
    fb.fillRect({px, bounds_.y, 1, bounds_.h}, core::kBlack);  // divider from the list

    // prev triangle (points up), centred in the top half
    const int upY = bounds_.y + bounds_.h / 4 - 5;
    for (int i = 0; i < 10; ++i)
        fb.fillRect({cx - i, upY + i, 2 * i + 1, 1}, core::kBlack);

    // next triangle (points down), centred in the bottom half
    const int dnY = bounds_.y + bounds_.h * 3 / 4 - 5;
    for (int i = 0; i < 10; ++i)
        fb.fillRect({cx - (9 - i), dnY + i, 2 * (9 - i) + 1, 1}, core::kBlack);

    // page indicator, vertically centred on the split line — no divider line
    // here, so it never collides with the "n/m" text or the arrows.
    char pg[16];
    std::snprintf(pg, sizeof(pg), "%d/%d", page_ + 1, pageCount());
    const int pw = font_->measure(pg);
    font_->draw(fb, cx - pw / 2,
                midY - font_->lineHeight() / 2 + font_->ascent(), pg, core::kBlack);
}

bool ListView::onTouch(int x, int y, TouchPhase phase)
{
    const int px = bounds_.right() - kPagerW;
    const int rh = rowHeight();
    const int rpp = rowsPerPage();

    if (phase == TouchPhase::Down) {
        if (x >= px) { down_row_ = -1; return true; }        // pager press
        const int r = (y - bounds_.y) / rh;
        const int idx = page_ * rpp + r;
        down_row_ = (r >= 0 && r < rpp && idx < count()) ? idx : -1;
        return true;
    }
    if (phase == TouchPhase::Move) return true;

    // Up
    if (x >= px) {
        setPage(y < bounds_.y + bounds_.h / 2 ? page_ - 1 : page_ + 1);
        return true;
    }
    const int r = (y - bounds_.y) / rh;
    const int idx = page_ * rpp + r;
    if (down_row_ >= 0 && idx == down_row_ && on_select_) {
        on_select_(idx);
    }
    down_row_ = -1;
    return true;
}

}  // namespace ui
