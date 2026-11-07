/*
 * Launcher tile: a 48 px icon centred in the upper area with the app name
 * centred below it — the iOS / Android home-screen idiom. Borderless; the whole
 * rect is the touch target. Acts on release (no e-ink press flash).
 */
#pragma once

#include <functional>
#include <string>

#include "font.h"
#include "widget.h"

namespace gfx { class IconSet; }

namespace ui {

class AppTile : public Widget {
public:
    AppTile() = default;

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }
    void setLabel(const char* s) { label_ = s ? s : ""; invalidate(); }
    void setIcon(const char* iconName) { icon_ = iconName ? iconName : ""; invalidate(); }
    // Optional per-tile icon sheet (an SD app's icon.m5i); nullptr => uiIcons().
    void setIconSet(const gfx::IconSet* s) { icon_set_ = s; invalidate(); }
    void onClick(std::function<void()> cb) { on_click_ = std::move(cb); }

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    const gfx::Font* font_ = nullptr;
    std::string label_;
    std::string icon_;              // name in the icon sheet; "" => text only
    const gfx::IconSet* icon_set_ = nullptr;  // per-tile override
    std::function<void()> on_click_;
};

}  // namespace ui
