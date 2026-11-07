/*
 * Modal reader-settings panel, shown via Screen::setOverlay(). The widget's
 * bounds cover the whole screen so every tap is captured while it is up (a tap
 * outside the panel just closes it); the visible panel is drawn centred inside.
 * Strictly 1-bit — the reader holds full refresh while this is open.
 */
#pragma once

#include <functional>
#include <vector>

#include "font.h"
#include "widget.h"

namespace ui {

class ReaderSettingsPopup : public Widget {
public:
    void setFont(const gfx::Font* f) { font_ = f; }

    // Available font sizes (px), from /sd/fonts; empty => embedded font only.
    void configure(std::vector<int> sizes, int font_px, int margin_px, int line_extra);

    int fontPx() const;                 // -1 when no SD sizes are available
    int marginPx() const { return margin_; }
    int lineExtra() const { return line_extra_; }

    void onChange(std::function<void()> cb) { on_change_ = std::move(cb); }
    void onClose(std::function<void()> cb) { on_close_ = std::move(cb); }

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    struct Hit { core::Rect r; int row; int delta; };  // delta 0 => close

    core::Rect panel() const;
    void buildHits(std::vector<Hit>& out) const;
    void bump(int row, int delta);

    const gfx::Font* font_ = nullptr;
    std::vector<int> sizes_;
    int size_idx_ = -1;
    int margin_ = 24;
    int line_extra_ = 0;

    int down_row_ = -2;   // -2 none, -1 close, >=0 stepper row
    int down_delta_ = 0;

    std::function<void()> on_change_;
    std::function<void()> on_close_;
};

}  // namespace ui
