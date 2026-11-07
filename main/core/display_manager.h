/*
 * DisplayManager — the single gate for every panel refresh.
 *
 * Widgets draw into fb() and call invalidate(); the kernel calls flush() once
 * per tick. The manager owns the refresh policy that keeps the SSD1677 within
 * its DC-balance limits (docs.md):
 *   - fast full-frame mono differential (~0.7 s) for 1-bit UI updates AND for
 *     app-to-app screen changes (RefreshHint::Page);
 *   - a clean OTP full mono refresh (~4 s) after kPartialLimit fast refreshes
 *     to clear ghosting, on requestFullClear(), and to rebuild the baseline
 *     after boot or any gray frame;
 *   - a full 4-gray refresh whenever the dirty area contains a gray value.
 *
 * UI coordinates are portrait 480 x 800; flush() transposes + inverts to the
 * controller's 800 x 480 RAM via uiToRam(), verified on hardware.
 */
#pragma once

#include <freertos/FreeRTOS.h>

#include "framebuffer.h"
#include "geometry.h"
#include "ssd1677.h"

namespace core {

enum class RefreshHint {
    Auto,      // let the policy decide
    Feedback,  // small, immediate (button press) — still a partial
    Page,      // full-screen change (app switch) — fast full-frame mono is fine;
               // the DC-balance counter still forces a periodic clean full
    Quality,   // caller demands a clean OTP full refresh now (ghost clear)
};

class DisplayManager {
public:
    static DisplayManager& instance();

    bool init(const ssd1677::HwHooks& hooks);

    Framebuffer& fb() { return fb_; }

    void invalidate(Rect r, RefreshHint hint = RefreshHint::Auto);
    void invalidateAll(RefreshHint hint = RefreshHint::Quality);

    // Force the next flush to be a full refresh (ghost clear), even if small.
    void requestFullClear();

    // Suspend the DC-balance forced full refresh (the one that fires after
    // kPartialLimit fast refreshes). Use around a burst of small mono updates
    // that must never be interrupted by a ~4 s flash — e.g. typing on the
    // on-screen keyboard. Releasing the hold while the partial count is already
    // over the limit schedules the deferred full clear for the next flush.
    void holdFullRefresh(bool hold);
    bool fullRefreshHeld() const { return fullHold_; }

    // Called once per kernel tick. No-op if nothing is pending.
    void flush();

    // True if flush() would actually refresh the panel.
    bool hasPending() const;

    int partialsSinceFull() const { return partialCount_; }

    // Hard DC-balance ceiling: after this many fast refreshes the next flush is
    // forced full. docs.md says "approximately 10"; we sit a little above it and
    // instead try to spend the full at a screen transition (see kPartialFoldIn)
    // so a 4 s flash rarely lands mid-interaction.
    static constexpr int kPartialLimit  = 13;
    // When a full-screen change (RefreshHint::Page) arrives and we are already
    // this many fast refreshes in, do it as a full refresh — the transition
    // repaints everything anyway, so the ghost-clear is free of an extra flash.
    static constexpr int kPartialFoldIn = 9;

private:
    DisplayManager() = default;

    bool packMonoFull();               // returns true if any byte changed
    bool packMonoRegion(Rect ramRect); // returns true if any byte changed
    void packGrayFull();
    static Rect uiToRam(Rect ui);        // transpose
    static Rect align8x(Rect ram);       // snap RAM-x span to a byte boundary

    Framebuffer fb_;
    uint8_t* plane1_ = nullptr;  // SSD1677 RAM1 (mono, or gray plane 1)
    uint8_t* plane2_ = nullptr;  // SSD1677 RAM2 (gray plane 2)

    // dirty_ / hint_ / forceFull_ are written by the input task (invalidate)
    // and snapshotted by the render task (flush); lock_ guards them.
    mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    Rect dirty_{};
    RefreshHint hint_        = RefreshHint::Auto;
    int partialCount_        = 0;
    int failStreak_          = 0;  // consecutive failed refreshes
    static constexpr int kMaxRetries = 2;
    bool monoBaselineValid_  = false;
    bool forceFull_          = false;
    bool fullHold_           = false;
    bool ready_              = false;
};

}  // namespace core
