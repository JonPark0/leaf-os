#include "text_block.h"

namespace ui {
namespace {

// Byte length of the UTF-8 sequence starting with `c`.
int u8len(unsigned char c) { return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; }

}  // namespace

void TextBlock::draw(core::Framebuffer& fb)
{
    if (!font_ || !font_->valid()) return;
    const int lh = font_->lineHeight();
    const int max_w = bounds_.w - 24;
    int y = bounds_.y;
    for (const std::string& line : lines_) {
        size_t start = 0;
        do {
            // Longest prefix that fits; prefer to break after a space.
            size_t end = start, last_space = std::string::npos;
            while (end < line.size()) {
                const size_t next = end + u8len(static_cast<unsigned char>(line[end]));
                if (font_->measure(line.c_str() + start, static_cast<int>(next - start)) > max_w) break;
                if (line[end] == ' ') last_space = next;
                end = next;
            }
            if (end < line.size() && last_space != std::string::npos && last_space > start) end = last_space;
            if (end == start && start < line.size()) end = start + u8len(static_cast<unsigned char>(line[start]));
            if (y + lh > bounds_.y + bounds_.h) return;
            font_->draw(fb, bounds_.x + 12, y + font_->ascent(), line.c_str() + start,
                        core::kBlack, static_cast<int>(end - start));
            y += lh;
            start = end;
        } while (start < line.size());
        if (line.empty()) y += lh;
    }
}

}  // namespace ui
