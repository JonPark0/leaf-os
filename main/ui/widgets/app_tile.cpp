#include "app_tile.h"

#include "iconset.h"

namespace ui {

void AppTile::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);

    // Icon: a per-tile sheet (SD app icon.m5i) wins, else the shared UI sheet;
    // prefer the 48 px variant, fall back to the base size.
    const gfx::IconSet* is = icon_set_ ? icon_set_ : gfx::uiIcons();
    gfx::Icon ic;
    if (is && !icon_.empty()) {
        ic = is->find((icon_ + "@48").c_str());
        if (!ic) ic = is->find(icon_.c_str());
    }

    const int labelH = (font_ && font_->valid()) ? font_->lineHeight() : 0;
    const int gap = 8;
    const int blockH = (ic ? ic.h : 0) + (ic && labelH ? gap : 0) + labelH;
    int y = bounds_.y + (bounds_.h - blockH) / 2;
    if (y < bounds_.y) y = bounds_.y;

    if (ic) {
        is->draw(fb, bounds_.x + (bounds_.w - ic.w) / 2, y, ic, core::kBlack);
        y += ic.h + gap;
    }
    if (font_ && font_->valid() && !label_.empty()) {
        const int tw = font_->measure(label_.c_str());
        const int tx = bounds_.x + (bounds_.w - tw) / 2;
        font_->draw(fb, tx, y + font_->ascent(), label_.c_str(), core::kBlack);
    }
}

bool AppTile::onTouch(int x, int y, TouchPhase phase)
{
    switch (phase) {
        case TouchPhase::Down:
        case TouchPhase::Move:
            return true;
        case TouchPhase::Up:
            if (bounds_.contains(x, y) && on_click_) on_click_();
            return true;
    }
    return false;
}

}  // namespace ui
