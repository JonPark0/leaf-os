#include "button.h"

namespace ui {

void Button::setLabel(const char* s)
{
    const char* v = s ? s : "";
    if (label_ == v) return;
    label_ = v;
    invalidate();
}

void Button::draw(core::Framebuffer& fb)
{
    const uint8_t face = pressed_ ? core::kBlack : core::kWhite;
    const uint8_t ink  = pressed_ ? core::kWhite : core::kBlack;
    fb.fillRect(bounds_, face);
    fb.frameRect(bounds_, 2, core::kBlack);

    if (font_ && font_->valid() && !label_.empty()) {
        const int tw = font_->measure(label_.c_str());
        const int x  = bounds_.x + (bounds_.w - tw) / 2;
        const int by = bounds_.y + (bounds_.h - font_->lineHeight()) / 2 + font_->ascent();
        font_->draw(fb, x, by, label_.c_str(), ink);
    }
}

bool Button::onTouch(int x, int y, TouchPhase phase)
{
    // No press-flash: an e-ink partial refresh is far too slow to show a
    // transient pressed state, and forcing one on every tap wastes a ~0.7 s
    // refresh with no visible change. Just act on release.
    switch (phase) {
        case TouchPhase::Down:
        case TouchPhase::Move:
            return true;  // capture the gesture
        case TouchPhase::Up:
            if (bounds_.contains(x, y) && on_click_) on_click_();
            return true;
    }
    return false;
}

}  // namespace ui
