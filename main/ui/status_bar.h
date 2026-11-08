/*
 * The always-on-top status strip: clock, front-light, Wi-Fi mark, battery.
 *
 * Owned by the kernel and installed as the overlay on whatever Screen is
 * active. tick() recomputes the content and repaints the affected slot only
 * when something actually changed, so it never spams partial refreshes.
 *
 * kHeight is not a compile-time constant: setFont() sizes the bar to the UI
 * font's *ink box* (ascent + descent + 12), so the vertically-centred text ends
 * up with ~6 px above and below on any script. Sizing to lineHeight() instead
 * leaks the font's line gap into the padding — Nanum's gap is large, CJK faces'
 * is ~0, so KO/EN then rode high while JA/ZH looked right. Every screen offsets
 * its layout from ui::StatusBar::kHeight, so setFont() must run (it does, from
 * Kernel::setUiFont) before the first app's onCreate().
 */
#pragma once

#include <algorithm>
#include <string>

#include "font.h"
#include "widget.h"

namespace ui {

class StatusBar : public Widget {
public:
    static inline int kHeight = 34;

    void setFont(const gfx::Font* f)
    {
        font_ = f;
        if (f && f->valid())
            kHeight = std::max(28, f->ascent() + f->descent() + 12);
    }
    void setWifiConnected(bool c);

    // Call once per kernel tick. Cheap; repaints only on change.
    void tick();

    void draw(core::Framebuffer& fb) override;
    // Takes no touches itself, but Screen still keeps its strip from the app
    // (see Screen::handleTouch).
    bool onTouch(int, int, TouchPhase) override { return false; }

private:
    void rebuildClock();
    void rebuildBrightness();
    void rebuildBattery();

    const gfx::Font* font_ = nullptr;
    std::string clock_ = "--:--";
    std::string bright_;
    std::string batt_;
    bool wifi_ = false;
    int  batt_bucket_ = -1;
    int  bright_shown_ = -1;
    int  last_minute_ = -1;
    int64_t next_poll_us_ = 0;
};

}  // namespace ui
