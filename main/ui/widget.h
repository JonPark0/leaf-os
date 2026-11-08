/*
 * Minimal retained-mode widget base. A widget owns a rectangle in portrait UI
 * coordinates, paints itself into the framebuffer, and may consume touch.
 * Repaint is driven by invalidate(), which bubbles a dirty rect to the Screen.
 */
#pragma once

#include "framebuffer.h"
#include "geometry.h"

namespace ui {

enum class TouchPhase { Down, Move, Up };

class Screen;

class Widget {
public:
    virtual ~Widget() = default;

    // Moving or resizing repaints both the old and the new area.
    void setBounds(core::Rect b)
    {
        if (b == bounds_) return;
        invalidate();
        bounds_ = b;
        invalidate();
    }
    core::Rect bounds() const { return bounds_; }

    void setScreen(Screen* s) { screen_ = s; }

    bool visible() const { return visible_; }
    void setVisible(bool v);

    // Paint into `fb`; only called for visible widgets that intersect a dirty
    // region (the Screen has already cleared the background underneath).
    virtual void draw(core::Framebuffer& fb) = 0;

    // Return true if the event was consumed. `x`,`y` are screen-absolute.
    virtual bool onTouch(int /*x*/, int /*y*/, TouchPhase /*phase*/) { return false; }

protected:
    void invalidate();  // request a repaint of bounds_

    core::Rect bounds_{};
    Screen* screen_ = nullptr;
    bool visible_ = true;
};

}  // namespace ui
