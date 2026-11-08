#pragma once

#include <string>
#include <vector>

#include "font.h"
#include "widget.h"

namespace ui {

// Lines of text, top-aligned, each word-wrapped to the width (12 px inset on
// both sides). An empty line is a blank line. Lines past the bottom are cut.
class TextBlock : public Widget {
public:
    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }
    void setLines(std::vector<std::string> lines) { lines_ = std::move(lines); invalidate(); }
    void draw(core::Framebuffer& fb) override;

private:
    const gfx::Font* font_ = nullptr;
    std::vector<std::string> lines_;
};

}  // namespace ui
