#pragma once

#include <functional>

#include "font.h"
#include "widget.h"

namespace ui {

// A square box with a check mark, drawn at the right edge of its bounds so it
// lines up with a left-aligned Label acting as the row title. The whole bounds
// rect is the touch target (e-ink taps are imprecise).
class Checkbox : public Widget {
public:
    explicit Checkbox(const gfx::Font* font = nullptr) : font_(font) {}

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }
    void setChecked(bool v);
    bool checked() const { return checked_; }
    void onToggle(std::function<void(bool)> cb) { on_toggle_ = std::move(cb); }

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    const gfx::Font* font_;
    bool checked_ = false;
    bool pressed_ = false;
    std::function<void(bool)> on_toggle_;
};

}  // namespace ui
