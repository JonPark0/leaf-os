#include "gesture.h"

#include <cstdlib>

namespace ui {

void GestureArbiter::setEnabled(bool on)
{
    enabled_ = on;
}

void GestureArbiter::setZones(int strip_h, int screen_h)
{
    strip_h_ = strip_h;
    screen_h_ = screen_h;
}

Gesture GestureArbiter::feed(int x, int y, TouchPhase phase, int64_t now_ms,
                             std::vector<TouchEvent>& out)
{
    if (phase == TouchPhase::Down) {
        // A Down while a touch is still open means its Up was missed. What was
        // held for it is stale by now, so it is dropped rather than delivered
        // ahead of the new touch.
        x0_ = x;
        y0_ = y;
        t0_ = now_ms;
        started_ = false;
        held_n_ = 0;
        // The zones are decided here, once: the finger's start says whose
        // touch it is, wherever it travels afterwards.
        if (!enabled_)                            mode_ = Mode::Pass;
        else if (y < strip_h_)                    mode_ = Mode::Strip;
        else if (y >= screen_h_ - kBottomBandPx)  mode_ = Mode::Held;
        else                                      mode_ = Mode::Pass;
        if (mode_ == Mode::Pass) out.push_back({x, y, phase});
        return Gesture::None;
    }

    Gesture fired = Gesture::None;
    switch (mode_) {
    case Mode::Strip:
        fired = feedStrip(x, y, phase);
        break;
    case Mode::Held:
        fired = feedHeld(x, y, phase, now_ms, out);
        break;
    case Mode::Idle:   // a Move/Up with no Down seen: not ours to judge
    case Mode::Pass:
        out.push_back({x, y, phase});
        break;
    }
    if (phase == TouchPhase::Up) mode_ = Mode::Idle;
    return fired;
}

Gesture GestureArbiter::feedStrip(int x, int y, TouchPhase phase)
{
    // Screen already keeps strip touches from the app, so nothing is lost by
    // swallowing them; only the release is judged.
    if (phase != TouchPhase::Up) return Gesture::None;
    const int down = y - y0_;
    const int dx = x - x0_;
    if (down >= kTriggerPx && std::abs(dx) < kSidewaysPx) return Gesture::PullDown;
    return Gesture::None;
}

Gesture GestureArbiter::feedHeld(int x, int y, TouchPhase phase, int64_t now_ms,
                                 std::vector<TouchEvent>& out)
{
    const int up = y0_ - y;
    const int dx = x - x0_;

    if (phase == TouchPhase::Up) {
        if (up >= kTriggerPx && std::abs(dx) < kSidewaysPx) {
            held_n_ = 0;
            return Gesture::SwipeUp;          // the app never sees this touch
        }
        // Every widget acts on release, so a tap delivered late, but whole and
        // in order, behaves exactly like one delivered as it happened.
        giveBack(out);
        out.push_back({x, y, phase});
        return Gesture::None;
    }

    remember(x, y);
    if (up >= kStartPx) started_ = true;

    // Give the touch back as soon as it is clearly not a swipe up, so a drag
    // that starts near the bottom edge (a scroll, a slider) only lags a little.
    const bool went_down = -up >= kCancelPx;
    const bool went_sideways = std::abs(dx) >= kCancelPx && std::abs(dx) > up;
    // A finger resting in the band is a press, not a swipe: the app gets it
    // after kHoldMs. A swipe that has started may be slow, so it has no limit.
    const bool rested = !started_ && now_ms - t0_ >= kHoldMs;
    if (went_down || went_sideways || rested) {
        giveBack(out);
        mode_ = Mode::Pass;
    }
    return Gesture::None;
}

void GestureArbiter::remember(int x, int y)
{
    // The kernel sends a Move every tick while the finger is down, moved or
    // not; only the positions matter to the app, so repeats are not kept.
    const int last_x = held_n_ > 0 ? held_[held_n_ - 1].x : x0_;
    const int last_y = held_n_ > 0 ? held_[held_n_ - 1].y : y0_;
    if (x == last_x && y == last_y) return;
    // When full, the newest sample replaces the last one: the path in between
    // is coarser, but the app still ends up where the finger is.
    if (held_n_ < kMaxHeldMoves) ++held_n_;
    held_[held_n_ - 1] = {x, y, TouchPhase::Move};
}

void GestureArbiter::giveBack(std::vector<TouchEvent>& out)
{
    out.push_back({x0_, y0_, TouchPhase::Down});
    out.insert(out.end(), held_, held_ + held_n_);
    held_n_ = 0;
}

}  // namespace ui
