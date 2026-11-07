#include "checkbox.h"

namespace ui {

void Checkbox::setChecked(bool v)
{
    if (v == checked_) return;
    checked_ = v;
    invalidate();
}

void Checkbox::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);

    const int box = 34;
    const int bx = bounds_.right() - box;
    const int by = bounds_.y + (bounds_.h - box) / 2;

    fb.frameRect({bx, by, box, box}, 2, core::kBlack);
    if (checked_) {
        const int pad = 8;
        fb.fillRect({bx + pad, by + pad, box - 2 * pad, box - 2 * pad}, core::kBlack);
    }
}

bool Checkbox::onTouch(int x, int y, TouchPhase phase)
{
    // Like Button: no press-flash (an e-ink partial refresh is too slow for a
    // transient state); act on release.
    if (phase == TouchPhase::Up) {
        if (bounds_.contains(x, y)) {
            checked_ = !checked_;
            invalidate();
            if (on_toggle_) on_toggle_(checked_);
        }
        return true;
    }
    return phase == TouchPhase::Down || phase == TouchPhase::Move;
}

}  // namespace ui
