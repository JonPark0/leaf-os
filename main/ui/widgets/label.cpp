#include "label.h"

namespace ui {

void Label::setText(const char* t)
{
    const char* s = t ? t : "";
    if (text_ == s) return;
    text_ = s;
    invalidate();
}

void Label::draw(core::Framebuffer& fb)
{
    if (bg_ != kTransparent) fb.fillRect(bounds_, bg_);
    if (!font_ || !font_->valid() || text_.empty()) return;

    const int tw = font_->measure(text_.c_str());
    int x = bounds_.x;
    if (align_ == Align::Center)      x = bounds_.x + (bounds_.w - tw) / 2;
    else if (align_ == Align::Right)  x = bounds_.right() - tw;

    const int baseline =
        bounds_.y + (bounds_.h - font_->lineHeight()) / 2 + font_->ascent();
    font_->draw(fb, x, baseline, text_.c_str(), ink_);
}

}  // namespace ui
