#include "text_field.h"

#include <algorithm>
#include <string>

namespace ui {
namespace {

void appendUtf8(std::string& s, uint32_t cp)
{
    if (cp < 0x80) {
        s.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Drop the last UTF-8 scalar (a trailing continuation run + its lead byte).
void popUtf8(std::string& s)
{
    if (s.empty()) return;
    size_t i = s.size();
    do { --i; } while (i > 0 && (static_cast<uint8_t>(s[i]) & 0xC0) == 0x80);
    s.erase(i);
}

// Number of UTF-8 scalars (for the password mask).
size_t scalarCount(const std::string& s)
{
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

}  // namespace

void TextField::setText(const std::string& s)
{
    if (s == text_) return;
    text_ = s;
    invalidate();
    notify();
}

void TextField::clear()
{
    if (text_.empty()) return;
    text_.clear();
    invalidate();
    notify();
}

void TextField::insertUtf8(uint32_t cp)
{
    appendUtf8(text_, cp);
    invalidate();
    notify();
}

void TextField::backspace()
{
    if (text_.empty()) return;
    popUtf8(text_);
    invalidate();
    notify();
}

void TextField::setFocused(bool f)
{
    if (f == focused_) return;
    focused_ = f;
    invalidate();
}

void TextField::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);
    fb.frameRect(bounds_, focused_ ? 3 : 1, core::kBlack);
    if (!font_ || !font_->valid()) return;

    const int pad = 10;
    const int by = bounds_.y + (bounds_.h - font_->lineHeight()) / 2 + font_->ascent();

    std::string shown;
    if (text_.empty() && !focused_) {
        shown = placeholder_;
    } else if (password_) {
        shown.assign(scalarCount(text_), '*');
    } else {
        shown = text_;
    }

    const int x = bounds_.x + pad;
    int pen = x;
    if (!shown.empty()) pen = font_->draw(fb, x, by, shown.c_str(), core::kBlack);

    // Caret: a 2 px bar at the pen, when focused.
    if (focused_) {
        const int cx = std::min(pen + 1, bounds_.right() - pad);
        fb.fillRect({cx, bounds_.y + 6, 2, bounds_.h - 12}, core::kBlack);
    }
}

bool TextField::onTouch(int x, int y, TouchPhase phase)
{
    if (phase == TouchPhase::Up && bounds_.contains(x, y)) {
        if (!focused_ && on_focus_) on_focus_();
    }
    return true;  // capture the gesture regardless
}

}  // namespace ui
