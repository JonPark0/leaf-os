#include "screen.h"

#include "display_manager.h"

namespace ui {

void Widget::setVisible(bool v)
{
    if (v == visible_) return;
    visible_ = v;
    invalidate();
}

void Widget::invalidate()
{
    if (screen_) screen_->invalidateRect(bounds_);
}

void Screen::add(Widget* w)
{
    w->setScreen(this);
    widgets_.push_back(w);
    invalidateRect(w->bounds());
}

void Screen::clearWidgets()
{
    widgets_.clear();
    captured_ = nullptr;
    invalidateAll();
}

void Screen::setOverlay(Widget* w)
{
    overlay_ = w;
    if (w) {
        w->setScreen(this);
        invalidateRect(w->bounds());
    }
}

void Screen::invalidateRect(core::Rect r)
{
    dirty_ = dirty_.unite(r.clampedTo(core::Framebuffer::kW, core::Framebuffer::kH));
}

void Screen::invalidateAll()
{
    full_repaint_ = true;
    dirty_ = {0, 0, core::Framebuffer::kW, core::Framebuffer::kH};
}

void Screen::handleTouch(int x, int y, TouchPhase phase)
{
    if (phase == TouchPhase::Down) {
        captured_ = nullptr;
        if (overlay_ && overlay_->visible() && overlay_->bounds().contains(x, y)) {
            if (overlay_->onTouch(x, y, phase)) captured_ = overlay_;
            return;
        }
        // Top-most first. A widget that declines the touch (onTouch -> false)
        // does not block widgets beneath it, so decorative full-area widgets
        // are transparent to input.
        for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
            Widget* w = *it;
            if (w->visible() && w->bounds().contains(x, y) && w->onTouch(x, y, phase)) {
                captured_ = w;
                break;
            }
        }
        return;
    }
    if (captured_) {
        captured_->onTouch(x, y, phase);
        if (phase == TouchPhase::Up) captured_ = nullptr;
    }
}

void Screen::paint(core::Framebuffer& fb)
{
    auto& dm = core::DisplayManager::instance();

    if (full_repaint_) {
        fb.clear(bg_);
        for (Widget* w : widgets_) {
            if (w->visible()) w->draw(fb);
        }
        if (overlay_ && overlay_->visible()) overlay_->draw(fb);
        dm.invalidateAll(core::RefreshHint::Page);
        full_repaint_ = false;
        dirty_ = {};
        return;
    }

    if (dirty_.empty()) return;

    const core::Rect d = dirty_;
    fb.fillRect(d, bg_);
    for (Widget* w : widgets_) {
        if (w->visible() && !w->bounds().intersect(d).empty()) {
            w->draw(fb);
        }
    }
    if (overlay_ && overlay_->visible() && !overlay_->bounds().intersect(d).empty()) {
        overlay_->draw(fb);
    }
    dm.invalidate(d);
    dirty_ = {};
}

}  // namespace ui
