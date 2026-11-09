/*
 * System gesture arbiter (docs/design/gestures.md). It sits between the
 * kernel's touch phases and Screen::handleTouch and decides, per touch,
 * whether the touch is the app's or one of the two system swipes:
 *
 *   top strip    y < strip_h                   never delivered; a pull down
 *                                              fires PullDown on release
 *   bottom band  y >= screen_h - kBottomBandPx held back until it is clearly
 *                                              a swipe up (fires SwipeUp on
 *                                              release) or clearly not (given
 *                                              back to the app, in order)
 *   elsewhere                                  delivered unchanged
 *
 * E-paper cannot follow the finger, so a gesture fires once, on release.
 * Host-clean, so tests/ builds this file.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "framebuffer.h"
#include "widget.h"   // ui::TouchPhase

namespace ui {

struct TouchEvent { int x; int y; TouchPhase phase; };

enum class Gesture { None, PullDown, SwipeUp };

class GestureArbiter {
public:
    static constexpr int     kBottomBandPx = 40;   // touches starting this close to the bottom edge are held
    static constexpr int     kTriggerPx    = 60;   // travel along the gesture's direction that fires it
    static constexpr int     kSidewaysPx   = 60;   // sideways travel at release must stay below this
    static constexpr int     kCancelPx     = 24;   // travel the other way that gives a held touch back
    static constexpr int     kStartPx      = 8;    // upward travel that marks a swipe in progress
    static constexpr int64_t kHoldMs       = 400;  // a held touch with no upward start by then is given back
    static constexpr int     kMaxHeldMoves = 32;

    // Both are read when a touch starts (Down); a touch in progress keeps the
    // rules it started with.
    void setEnabled(bool on);
    // strip_h: height of the status bar strip, 0 while the bar is hidden.
    // screen_h: panel height (800). The bottom band is y >= screen_h - kBottomBandPx.
    void setZones(int strip_h, int screen_h);

    // Feed one touch sample. Appends the events the screen should receive to
    // `out` (does not clear it) and returns the gesture that fired on this
    // sample, if any.
    Gesture feed(int x, int y, TouchPhase phase, int64_t now_ms, std::vector<TouchEvent>& out);

private:
    enum class Mode {
        Idle,    // no touch in progress
        Pass,    // the app's touch: every sample goes straight through
        Strip,   // started in the top strip: swallowed, may pull down
        Held,    // started in the bottom band: held back, may swipe up
    };

    Gesture feedStrip(int x, int y, TouchPhase phase);
    Gesture feedHeld(int x, int y, TouchPhase phase, int64_t now_ms, std::vector<TouchEvent>& out);
    void remember(int x, int y);
    void giveBack(std::vector<TouchEvent>& out);

    // Settings. Only Down reads them (into mode_), so a change lands on the
    // next touch.
    bool enabled_  = true;                    // the setting is on by default
    int  strip_h_  = 0;
    int  screen_h_ = core::Framebuffer::kH;

    // The touch in progress.
    Mode    mode_    = Mode::Idle;
    int     x0_      = 0;                     // where it went down, and when
    int     y0_      = 0;
    int64_t t0_      = 0;
    bool    started_ = false;                 // has moved up kStartPx: no time limit any more

    // Moves seen while held, replayed after the held Down if the touch is given
    // back. Fixed size: the input tick must not allocate per sample.
    TouchEvent held_[kMaxHeldMoves]{};
    int        held_n_ = 0;
};

}  // namespace ui
