/*
 * On-screen keyboard: 4 rows, three layers (lowercase / uppercase / symbols).
 * ASCII only — enough for Wi-Fi passwords and file names; Hangul IME is a later
 * task. No per-key press flash (e-ink); each tap fires on release. Strictly
 * 1-bit. The owning screen positions it (typically pinned to the bottom).
 */
#pragma once

#include <functional>

#include "font.h"
#include "widget.h"

namespace ui {

class Keyboard : public Widget {
public:
    explicit Keyboard(const gfx::Font* font = nullptr) : font_(font) {}

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }

    void onChar(std::function<void(uint32_t)> cb) { on_char_ = std::move(cb); }
    void onBackspace(std::function<void()> cb) { on_bs_ = std::move(cb); }
    void onEnter(std::function<void()> cb) { on_enter_ = std::move(cb); }

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    enum KeyType : uint8_t { KChar, KShift, KBack, KLayer, KSpace, KEnter };
    struct Key {
        const char* cap;   // label; for KChar, also the emitted byte
        KeyType     type;
    };

    // Fill `rects`/`keys` with the current layer's layout; returns the count.
    int layout(core::Rect* rects, const Key** keys, int max) const;
    void tap(const Key& k);

    const gfx::Font* font_;
    int  layer_ = 0;   // 0 lower, 1 upper, 2 symbols
    int  down_key_ = -1;
    std::function<void(uint32_t)> on_char_;
    std::function<void()> on_bs_;
    std::function<void()> on_enter_;
};

}  // namespace ui
