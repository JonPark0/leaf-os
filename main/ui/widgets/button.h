#pragma once

#include <functional>
#include <string>

#include "font.h"
#include "widget.h"

namespace ui {

class Button : public Widget {
public:
    Button(const gfx::Font* font = nullptr, const char* label = "") : font_(font), label_(label) {}

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }
    void setLabel(const char* s);
    void onClick(std::function<void()> cb) { on_click_ = std::move(cb); }

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    const gfx::Font* font_;
    std::string label_;
    std::function<void()> on_click_;
    bool pressed_ = false;
};

}  // namespace ui
