#include "keyboard.h"

#include <cstring>

namespace ui {
namespace {

constexpr int kGap = 4;

// Row character sets, per layer. Rows 0..2 are plain character keys; row 2 is
// framed by Shift (front) and Backspace (back); row 3 is fixed specials.
const char* const kRows[3][3] = {
    // lowercase
    {"qwertyuiop", "asdfghjkl",  "zxcvbnm"},
    // uppercase
    {"QWERTYUIOP", "ASDFGHJKL",  "ZXCVBNM"},
    // symbols
    {"1234567890", "@#$_&-+()/", "*\"':;!?"},
};

}  // namespace

int Keyboard::layout(core::Rect* rects, const Key** keys, int max) const
{
    static Key store[40];
    int n = 0;

    const int rows = 4;
    const int rh = (bounds_.h - (rows + 1) * kGap) / rows;
    const int cell = (bounds_.w - 11 * kGap) / 10;  // 10-column base grid

    auto put = [&](core::Rect r, const char* cap, KeyType t) {
        if (n >= max || n >= 40) return;
        store[n] = {cap, t};
        keys[n] = &store[n];
        rects[n] = r;
        ++n;
    };

    for (int row = 0; row < 3; ++row) {
        const char* s = kRows[layer_][row];
        const int len = static_cast<int>(std::strlen(s));
        const bool framed = (row == 2);
        const int count = len + (framed ? 2 : 0);
        const int total_w = count * cell + (count - 1) * kGap;
        int x = bounds_.x + (bounds_.w - total_w) / 2;
        const int y = bounds_.y + kGap + row * (rh + kGap);

        if (framed) {
            // The UI font covers U+2190..U+21FF (arrows) — ⇧ (U+21E7) is in it,
            // but ⌫ (U+232B, misc-technical) is NOT, so backspace uses ← below.
            put({x, y, cell, rh}, layer_ == 2 ? "ABC" : "\xE2\x87\xA7" /* ⇧ */,
                layer_ == 2 ? KLayer : KShift);
            x += cell + kGap;
        }
        for (int i = 0; i < len; ++i) {
            static char one[40][2];
            one[n][0] = s[i];
            one[n][1] = '\0';
            put({x, y, cell, rh}, one[n], KChar);
            x += cell + kGap;
        }
        if (framed) {
            put({x, y, cell, rh}, "\xE2\x86\x90" /* ← */, KBack);
        }
    }

    // Row 3: [layer] [,] [ space ] [.] [enter]
    const int y = bounds_.y + kGap + 3 * (rh + kGap);
    const int wSpecial = cell * 3 / 2;
    const int wSpace = bounds_.w - 2 * kGap - 2 * wSpecial - 2 * cell - 4 * kGap;
    int x = bounds_.x + kGap;
    put({x, y, wSpecial, rh}, layer_ == 2 ? "ABC" : "?123", KLayer);
    x += wSpecial + kGap;
    put({x, y, cell, rh}, ",", KChar);
    x += cell + kGap;
    put({x, y, wSpace, rh}, "", KSpace);
    x += wSpace + kGap;
    put({x, y, cell, rh}, ".", KChar);
    x += cell + kGap;
    put({x, y, wSpecial, rh}, "\xEC\x99\x84\xEB\xA3\x8C" /* 완료 */, KEnter);

    return n;
}

void Keyboard::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);
    fb.fillRect({bounds_.x, bounds_.y, bounds_.w, 1}, core::kBlack);  // top edge

    core::Rect rects[40];
    const Key* keys[40];
    const int n = layout(rects, keys, 40);
    if (!font_ || !font_->valid()) return;

    for (int i = 0; i < n; ++i) {
        const core::Rect& r = rects[i];
        fb.fillRect(r, core::kWhite);
        fb.frameRect(r, 1, core::kBlack);
        const char* cap = keys[i]->cap;
        if (keys[i]->type == KSpace || !cap || !cap[0]) continue;
        const int tw = font_->measure(cap);
        const int tx = r.x + (r.w - tw) / 2;
        const int ty = r.y + (r.h - font_->lineHeight()) / 2 + font_->ascent();
        font_->draw(fb, tx, ty, cap, core::kBlack);
    }
}

void Keyboard::tap(const Key& k)
{
    switch (k.type) {
        case KChar:
            if (on_char_ && k.cap && k.cap[0]) on_char_(static_cast<uint8_t>(k.cap[0]));
            break;
        case KSpace:
            if (on_char_) on_char_(' ');
            break;
        case KBack:
            if (on_bs_) on_bs_();
            break;
        case KEnter:
            if (on_enter_) on_enter_();
            break;
        case KShift:
            layer_ = (layer_ == 0) ? 1 : 0;
            invalidate();
            break;
        case KLayer:
            layer_ = (layer_ == 2) ? 0 : 2;
            invalidate();
            break;
    }
}

bool Keyboard::onTouch(int x, int y, TouchPhase phase)
{
    core::Rect rects[40];
    const Key* keys[40];
    const int n = layout(rects, keys, 40);

    if (phase == TouchPhase::Down) {
        down_key_ = -1;
        for (int i = 0; i < n; ++i) {
            if (rects[i].contains(x, y)) { down_key_ = i; break; }
        }
        return true;
    }
    if (phase == TouchPhase::Move) return true;

    // Up: act only if the release lands on the same key as the press.
    if (down_key_ >= 0 && down_key_ < n && rects[down_key_].contains(x, y)) {
        tap(*keys[down_key_]);
    }
    down_key_ = -1;
    return true;
}

}  // namespace ui
