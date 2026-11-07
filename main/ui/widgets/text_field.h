/*
 * Single-line text input. Not self-managing focus — the owning screen decides
 * when it is focused (typically: tap the field -> show a Keyboard -> feed its
 * events here). Strictly 1-bit (white/black) like every shell widget.
 */
#pragma once

#include <functional>
#include <string>

#include "font.h"
#include "widget.h"

namespace ui {

class TextField : public Widget {
public:
    explicit TextField(const gfx::Font* font = nullptr) : font_(font) {}

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }
    void setPlaceholder(const char* s) { placeholder_ = s ? s : ""; invalidate(); }
    void setPassword(bool on) { password_ = on; invalidate(); }

    const std::string& text() const { return text_; }
    void setText(const std::string& s);
    void clear();

    void insertUtf8(uint32_t cp);   // append one Unicode scalar
    void backspace();               // remove the last scalar

    void setFocused(bool f);
    bool focused() const { return focused_; }

    // Fires after any content change (insert / backspace / clear / setText).
    void onChanged(std::function<void(const std::string&)> cb) { on_changed_ = std::move(cb); }
    // Fires on a tap when not already focused — the app shows its keyboard here.
    void onFocusRequested(std::function<void()> cb) { on_focus_ = std::move(cb); }

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    void notify() { if (on_changed_) on_changed_(text_); }

    const gfx::Font* font_;
    std::string text_;
    std::string placeholder_;
    bool password_ = false;
    bool focused_  = false;
    std::function<void(const std::string&)> on_changed_;
    std::function<void()> on_focus_;
};

}  // namespace ui
