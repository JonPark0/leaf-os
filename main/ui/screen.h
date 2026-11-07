/*
 * Screen — a container of widgets plus touch routing and dirty-region tracking.
 *
 * The kernel calls paint() once per tick (cheap when nothing changed) and
 * routes touch through handleTouch(). paint() clears the background under the
 * dirty region, repaints the widgets that intersect it, and pushes the region
 * to core::DisplayManager. The kernel's render task performs the (blocking)
 * panel refresh separately, so input is never stalled.
 */
#pragma once

#include <vector>

#include "framebuffer.h"
#include "geometry.h"
#include "widget.h"

namespace ui {

class Screen {
public:
    virtual ~Screen() = default;

    void add(Widget* w);                       // not owned; must outlive the Screen
    void clearWidgets();                        // drop all widgets (keeps overlay)
    void setBackground(uint8_t gray) { bg_ = gray; }

    // A single always-on-top widget (the status bar). Drawn after everything
    // else, hit-tested before everything else. May be shared between screens;
    // the kernel re-points it on app switch.
    void setOverlay(Widget* w);
    Widget* overlay() const { return overlay_; }

    void invalidateRect(core::Rect r);
    void invalidateAll();                      // full repaint + quality refresh

    void handleTouch(int x, int y, TouchPhase phase);

    // Optional key handling; return true to stop the kernel's default action
    // (side keys -> front-light brightness).
    virtual bool onKey(int /*key*/, int /*event*/) { return false; }

    // Called by the kernel each tick. No-op if nothing is dirty.
    void paint(core::Framebuffer& fb);

protected:
    virtual void onShow() {}                   // first paint hook for subclasses

private:
    std::vector<Widget*> widgets_;
    Widget* overlay_ = nullptr;
    Widget* captured_ = nullptr;
    core::Rect dirty_{};
    uint8_t bg_ = core::kWhite;
    bool full_repaint_ = true;
};

}  // namespace ui
