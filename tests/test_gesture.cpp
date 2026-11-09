#include <string>
#include <vector>

#include "check.h"
#include "gesture.h"

using ui::Gesture;
using ui::GestureArbiter;
using ui::TouchEvent;
using ui::TouchPhase;

namespace {

constexpr int kScreenH = 800;
constexpr int kStripH  = 36;
constexpr int kBandTop = kScreenH - GestureArbiter::kBottomBandPx;   // first row of the band

// An arbiter with the status bar shown, fed with a clock that advances one
// input tick (16 ms) per sample unless told otherwise.
struct Rig {
    GestureArbiter g;
    std::vector<TouchEvent> out;
    int64_t now = 1000;

    Rig() { g.setZones(kStripH, kScreenH); }

    Gesture down(int x, int y) { return g.feed(x, y, TouchPhase::Down, now, out); }
    Gesture move(int x, int y, int64_t dt = 16)
    {
        now += dt;
        return g.feed(x, y, TouchPhase::Move, now, out);
    }
    Gesture up(int x, int y, int64_t dt = 16)
    {
        now += dt;
        return g.feed(x, y, TouchPhase::Up, now, out);
    }
};

// "D240,780 M240,770 U240,770": a whole delivery compared in one line.
std::string trace(const std::vector<TouchEvent>& evs)
{
    std::string s;
    for (const TouchEvent& e : evs) {
        if (!s.empty()) s += ' ';
        s += e.phase == TouchPhase::Down ? 'D' : e.phase == TouchPhase::Move ? 'M' : 'U';
        s += std::to_string(e.x) + ',' + std::to_string(e.y);
    }
    return s;
}

}  // namespace

// --- ordinary touches -------------------------------------------------------

TEST(gesture_tap_in_middle_passes_through)
{
    Rig r;
    EXPECT_EQ(r.down(240, 400), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,400");
    EXPECT_EQ(r.move(240, 400), Gesture::None);
    EXPECT_EQ(r.move(250, 380), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,400 M240,400 M250,380");
    EXPECT_EQ(r.up(250, 380), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,400 M240,400 M250,380 U250,380");
}

TEST(gesture_row_above_band_passes_through)
{
    Rig r;
    r.down(240, kBandTop - 1);
    EXPECT_STREQ(trace(r.out), "D240,759");
    r.up(240, kBandTop - 1);
    EXPECT_STREQ(trace(r.out), "D240,759 U240,759");
}

TEST(gesture_move_or_up_without_down_passes_through)
{
    Rig r;
    EXPECT_EQ(r.move(10, 790), Gesture::None);
    EXPECT_EQ(r.up(10, 790), Gesture::None);
    EXPECT_STREQ(trace(r.out), "M10,790 U10,790");
}

TEST(gesture_out_is_appended_not_cleared)
{
    Rig r;
    r.out.push_back({1, 2, TouchPhase::Move});
    r.down(240, 400);
    EXPECT_STREQ(trace(r.out), "M1,2 D240,400");
}

// --- bottom band: taps ------------------------------------------------------

TEST(gesture_band_tap_delivered_on_release)
{
    Rig r;
    EXPECT_EQ(r.down(240, kBandTop), Gesture::None);
    EXPECT_STREQ(trace(r.out), "");
    // The kernel sends a Move every tick, moved or not.
    for (int i = 0; i < 5; ++i) EXPECT_EQ(r.move(240, kBandTop), Gesture::None);
    EXPECT_STREQ(trace(r.out), "");
    EXPECT_EQ(r.up(240, kBandTop), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,760 U240,760");
}

TEST(gesture_band_tap_keeps_its_jitter)
{
    Rig r;
    r.down(240, 780);
    r.move(242, 778);
    r.move(242, 778);
    r.move(243, 779);
    EXPECT_STREQ(trace(r.out), "");
    r.up(243, 779);
    EXPECT_STREQ(trace(r.out), "D240,780 M242,778 M243,779 U243,779");
}

// --- bottom band: swipe up --------------------------------------------------

TEST(gesture_swipe_up_60_fires)
{
    Rig r;
    r.down(240, 790);
    EXPECT_EQ(r.move(240, 770), Gesture::None);
    EXPECT_EQ(r.move(240, 740), Gesture::None);
    EXPECT_EQ(r.up(240, 730), Gesture::SwipeUp);
    EXPECT_STREQ(trace(r.out), "");
}

TEST(gesture_swipe_up_59_is_a_tap)
{
    Rig r;
    r.down(240, 790);
    r.move(240, 760);
    r.move(240, 731);
    EXPECT_EQ(r.up(240, 731), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,790 M240,760 M240,731 U240,731");
}

TEST(gesture_swipe_up_too_sideways_does_not_fire)
{
    {
        Rig r;
        r.down(240, 790);
        r.move(260, 760);
        r.move(290, 720);   // 50 sideways but 70 up: still a swipe
        EXPECT_STREQ(trace(r.out), "");
        EXPECT_EQ(r.up(300, 720), Gesture::None);
        EXPECT_STREQ(trace(r.out), "D240,790 M260,760 M290,720 U300,720");
    }
    {
        Rig r;
        r.down(240, 790);
        r.move(260, 760);
        r.move(290, 720);
        EXPECT_EQ(r.up(299, 720), Gesture::SwipeUp);
        EXPECT_STREQ(trace(r.out), "");
    }
    {
        Rig r;   // to the left as well
        r.down(240, 790);
        r.move(220, 760);
        r.move(190, 720);
        EXPECT_EQ(r.up(180, 720), Gesture::None);
        EXPECT_STREQ(trace(r.out), "D240,790 M220,760 M190,720 U180,720");
    }
}

// --- bottom band: giving the touch back -------------------------------------

TEST(gesture_sideways_move_gives_back)
{
    Rig r;
    r.down(240, 790);
    r.move(250, 788);
    r.move(263, 786);   // 23 sideways: not yet
    EXPECT_STREQ(trace(r.out), "");
    EXPECT_EQ(r.move(264, 786), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,790 M250,788 M263,786 M264,786");
    // From here on the touch is the app's.
    r.out.clear();
    r.move(264, 786);
    r.move(300, 786);
    EXPECT_STREQ(trace(r.out), "M264,786 M300,786");
    EXPECT_EQ(r.up(300, 700), Gesture::None);
    EXPECT_STREQ(trace(r.out), "M264,786 M300,786 U300,700");
}

TEST(gesture_sideways_move_to_the_left_gives_back)
{
    Rig r;
    r.down(240, 790);
    r.move(216, 780);   // 24 left, 10 up
    EXPECT_STREQ(trace(r.out), "D240,790 M216,780");
}

TEST(gesture_downward_move_gives_back)
{
    Rig r;
    r.down(240, 770);
    r.move(240, 793);   // 23 down: not yet
    EXPECT_STREQ(trace(r.out), "");
    r.move(240, 794);
    EXPECT_STREQ(trace(r.out), "D240,770 M240,793 M240,794");
    r.out.clear();
    EXPECT_EQ(r.up(240, 794), Gesture::None);
    EXPECT_STREQ(trace(r.out), "U240,794");
}

TEST(gesture_rest_gives_back_after_hold)
{
    Rig r;
    r.down(240, 780);
    r.move(240, 780, GestureArbiter::kHoldMs - 1);
    EXPECT_STREQ(trace(r.out), "");
    r.move(240, 780, 1);
    EXPECT_STREQ(trace(r.out), "D240,780");
    r.move(240, 780);
    r.up(240, 780);
    EXPECT_STREQ(trace(r.out), "D240,780 M240,780 U240,780");
}

TEST(gesture_small_upward_move_still_times_out)
{
    Rig r;
    r.down(240, 780);
    r.move(240, 773, 100);   // 7 up: no swipe started
    r.move(240, 773, 299);
    EXPECT_STREQ(trace(r.out), "");
    r.move(240, 773, 1);
    EXPECT_STREQ(trace(r.out), "D240,780 M240,773");
}

TEST(gesture_slow_swipe_has_no_time_limit)
{
    Rig r;
    r.down(240, 790);
    r.move(240, 782, 300);   // 8 up before kHoldMs: a swipe has started
    r.move(240, 770, 300);
    r.move(240, 740, 400);
    EXPECT_STREQ(trace(r.out), "");
    EXPECT_EQ(r.up(240, 730, 0), Gesture::SwipeUp);
    EXPECT_STREQ(trace(r.out), "");
}

TEST(gesture_started_swipe_that_settles_stays_held)
{
    // Once started, the swipe keeps no time limit even if the finger drifts
    // back below kStartPx; release decides.
    Rig r;
    r.down(240, 790);
    r.move(240, 780, 100);
    r.move(240, 786, 900);
    EXPECT_STREQ(trace(r.out), "");
    EXPECT_EQ(r.up(240, 786), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,790 M240,780 M240,786 U240,786");
}

TEST(gesture_held_move_cap_keeps_newest)
{
    constexpr int kCap = GestureArbiter::kMaxHeldMoves;
    constexpr int kMoves = kCap + 8;
    {
        // Given back by a sideways move.
        Rig r;
        r.down(240, 790);
        for (int i = 1; i <= kMoves; ++i) r.move(240, 790 - i);
        EXPECT_STREQ(trace(r.out), "");
        r.move(290, 790 - kMoves);   // 50 sideways, 40 up
        EXPECT_EQ(r.out.size(), size_t(1 + kCap));
        EXPECT_STREQ(trace({r.out[0]}), "D240,790");
        EXPECT_STREQ(trace({r.out[1]}), "M240,789");
        EXPECT_STREQ(trace({r.out[kCap - 1]}), "M240," + std::to_string(790 - (kCap - 1)));
        EXPECT_STREQ(trace({r.out[kCap]}), "M290,750");
    }
    {
        // Given back on release.
        Rig r;
        r.down(240, 790);
        for (int i = 1; i <= kMoves; ++i) r.move(240, 790 - i);
        r.up(240, 790 - kMoves);
        EXPECT_EQ(r.out.size(), size_t(1 + kCap + 1));
        EXPECT_STREQ(trace({r.out[kCap]}), "M240,750");
        EXPECT_STREQ(trace({r.out[kCap + 1]}), "U240,750");
    }
}

// --- top strip: pull down ---------------------------------------------------

TEST(gesture_pull_down_60_fires)
{
    Rig r;
    EXPECT_EQ(r.down(240, 10), Gesture::None);
    EXPECT_EQ(r.move(240, 40), Gesture::None);
    EXPECT_EQ(r.move(240, 70), Gesture::None);
    EXPECT_EQ(r.up(240, 70), Gesture::PullDown);
    EXPECT_STREQ(trace(r.out), "");
}

TEST(gesture_pull_down_short_or_sideways_does_nothing)
{
    {
        Rig r;
        r.down(240, 10);
        r.move(240, 69);
        EXPECT_EQ(r.up(240, 69), Gesture::None);
        EXPECT_STREQ(trace(r.out), "");
    }
    {
        Rig r;
        r.down(240, 10);
        r.move(300, 90);
        EXPECT_EQ(r.up(300, 90), Gesture::None);
        EXPECT_STREQ(trace(r.out), "");
    }
    {
        Rig r;
        r.down(240, 10);
        r.move(299, 90);
        EXPECT_EQ(r.up(299, 90), Gesture::PullDown);
        EXPECT_STREQ(trace(r.out), "");
    }
    {
        Rig r;   // a tap or a sideways drag in the strip never reaches the app
        r.down(240, 10);
        r.move(400, 12, 600);
        EXPECT_EQ(r.up(400, 12), Gesture::None);
        EXPECT_STREQ(trace(r.out), "");
    }
}

TEST(gesture_hidden_status_bar_has_no_strip)
{
    Rig r;
    r.g.setZones(0, kScreenH);
    r.down(240, 5);
    r.move(240, 80);
    EXPECT_EQ(r.up(240, 80), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,5 M240,80 U240,80");
}

// --- settings ---------------------------------------------------------------

TEST(gesture_disabled_passes_everything_through)
{
    Rig r;
    r.g.setEnabled(false);
    EXPECT_EQ(r.down(240, 790), Gesture::None);
    EXPECT_EQ(r.move(240, 700), Gesture::None);
    EXPECT_EQ(r.up(240, 700), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,790 M240,700 U240,700");

    r.out.clear();
    EXPECT_EQ(r.down(240, 10), Gesture::None);
    EXPECT_EQ(r.move(240, 100), Gesture::None);
    EXPECT_EQ(r.up(240, 100), Gesture::None);
    EXPECT_STREQ(trace(r.out), "D240,10 M240,100 U240,100");
}

TEST(gesture_settings_change_waits_for_next_touch)
{
    {
        // Held when it started: still a swipe after gestures are turned off.
        Rig r;
        r.down(240, 790);
        r.g.setEnabled(false);
        r.g.setZones(0, 2000);
        r.move(240, 750);
        EXPECT_EQ(r.up(240, 730), Gesture::SwipeUp);
        EXPECT_STREQ(trace(r.out), "");
        // The next touch follows the new settings.
        r.down(240, 790);
        EXPECT_STREQ(trace(r.out), "D240,790");
    }
    {
        // A strip touch keeps its pull down after the bar hides.
        Rig r;
        r.down(240, 10);
        r.g.setZones(0, kScreenH);
        r.g.setEnabled(false);
        EXPECT_EQ(r.up(240, 80), Gesture::PullDown);
        EXPECT_STREQ(trace(r.out), "");
    }
    {
        // Passing through when it started: still the app's after gestures
        // are turned on and the strip grows over it.
        Rig r;
        r.g.setEnabled(false);
        r.down(240, 790);
        r.g.setEnabled(true);
        r.g.setZones(900, kScreenH);
        r.move(240, 700);
        EXPECT_EQ(r.up(240, 700), Gesture::None);
        EXPECT_STREQ(trace(r.out), "D240,790 M240,700 U240,700");
    }
}

// --- missed Up --------------------------------------------------------------

TEST(gesture_down_while_held_starts_over)
{
    Rig r;
    r.down(240, 790);
    r.move(240, 770);
    // The Up was missed; the held touch is dropped, not delivered.
    r.down(100, 400);
    EXPECT_STREQ(trace(r.out), "D100,400");
    r.up(100, 400);
    EXPECT_STREQ(trace(r.out), "D100,400 U100,400");

    // Nor does it surface with the next held touch.
    r.out.clear();
    r.down(240, 790);
    r.move(240, 770);
    r.down(200, 780);
    r.up(200, 780);
    EXPECT_STREQ(trace(r.out), "D200,780 U200,780");
}

TEST(gesture_band_touches_in_a_row_start_clean)
{
    Rig r;
    r.down(240, 790);
    r.move(240, 750);
    EXPECT_EQ(r.up(240, 720), Gesture::SwipeUp);
    r.down(100, 780);
    r.move(130, 780);   // given back
    r.up(130, 780);
    r.down(300, 790);
    r.move(302, 789);
    r.up(302, 789);
    r.down(400, 790);
    r.up(400, 790);
    EXPECT_STREQ(trace(r.out),
                 "D100,780 M130,780 U130,780 D300,790 M302,789 U302,789 D400,790 U400,790");
}

TEST(gesture_down_while_in_strip_starts_over)
{
    Rig r;
    r.down(240, 10);
    r.down(240, 790);
    r.move(240, 740);
    EXPECT_EQ(r.up(240, 720), Gesture::SwipeUp);
    EXPECT_STREQ(trace(r.out), "");
}

// --- reset (the screen changed under the finger) ----------------------------

TEST(gesture_reset_drops_a_held_tap)
{
    Rig r;
    r.down(240, kBandTop);
    r.move(250, kBandTop);
    r.g.reset();
    // The held Down and Move are gone; what follows arrives as strays, which
    // Screen ignores, instead of a whole tap on the new screen.
    EXPECT_EQ(r.move(250, kBandTop), Gesture::None);
    EXPECT_EQ(r.up(250, kBandTop), Gesture::None);
    EXPECT_STREQ(trace(r.out), "M250,760 U250,760");
}

TEST(gesture_reset_cancels_a_pull_down)
{
    Rig r;
    r.down(240, 10);
    r.move(240, 50);
    r.g.reset();
    EXPECT_EQ(r.up(240, 100), Gesture::None);
    EXPECT_STREQ(trace(r.out), "U240,100");
}

TEST(gesture_reset_leaves_the_next_touch_alone)
{
    Rig r;
    r.down(240, kBandTop);
    r.g.reset();
    r.up(240, kBandTop);
    r.out.clear();
    r.down(240, kBandTop);
    EXPECT_EQ(r.up(240, kBandTop - GestureArbiter::kTriggerPx), Gesture::SwipeUp);
    EXPECT_STREQ(trace(r.out), "");
}
