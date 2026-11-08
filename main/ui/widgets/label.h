#pragma once

#include <string>

#include "font.h"
#include "widget.h"

namespace ui {

// One line of text in its rect, vertically centred. Text wider than the rect
// is cut with "..." instead of drawing outside it.
class Label : public Widget {
public:
    enum class Align { Left, Center, Right };

    Label(const gfx::Font* font = nullptr, const char* text = "") : font_(font), text_(text) {}

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }
    void setText(const char* t);
    void setText(const std::string& t) { setText(t.c_str()); }
    const std::string& text() const { return text_; }

    void setAlign(Align a) { align_ = a; invalidate(); }
    void setInk(uint8_t g) { ink_ = g; invalidate(); }
    void setBackground(uint8_t g) { bg_ = g; invalidate(); }  // kTransparent to skip

    static constexpr uint8_t kTransparent = 0xFF;

    void draw(core::Framebuffer& fb) override;

private:
    const gfx::Font* font_;
    std::string text_;
    Align align_   = Align::Left;
    uint8_t ink_   = core::kBlack;
    uint8_t bg_    = kTransparent;
};

}  // namespace ui
