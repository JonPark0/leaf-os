#include "display_manager.h"

#include <algorithm>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace core {
namespace {
constexpr char TAG[] = "dispmgr";
}

DisplayManager& DisplayManager::instance()
{
    static DisplayManager s;
    return s;
}

bool DisplayManager::init(const ssd1677::HwHooks& hooks)
{
    if (ready_) return true;
    if (!fb_.init()) return false;

    plane1_ = static_cast<uint8_t*>(
        heap_caps_malloc(ssd1677::PLANE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    plane2_ = static_cast<uint8_t*>(
        heap_caps_malloc(ssd1677::PLANE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (plane1_ == nullptr || plane2_ == nullptr) {
        ESP_LOGE(TAG, "plane OOM");
        return false;
    }

    if (!ssd1677::init(hooks)) {
        ESP_LOGE(TAG, "ssd1677::init failed");
        return false;
    }

    fb_.clear(kWhite);
    dirty_          = {0, 0, Framebuffer::kW, Framebuffer::kH};
    hint_           = RefreshHint::Quality;
    monoBaselineValid_ = false;
    ready_          = true;
    return true;
}

void DisplayManager::invalidate(Rect r, RefreshHint hint)
{
    r = r.clampedTo(Framebuffer::kW, Framebuffer::kH);
    if (r.empty()) return;
    portENTER_CRITICAL(&lock_);
    dirty_ = dirty_.unite(r);
    if (hint == RefreshHint::Quality) hint_ = RefreshHint::Quality;
    portEXIT_CRITICAL(&lock_);
}

void DisplayManager::invalidateAll(RefreshHint hint)
{
    portENTER_CRITICAL(&lock_);
    dirty_ = {0, 0, Framebuffer::kW, Framebuffer::kH};
    hint_  = hint;
    portEXIT_CRITICAL(&lock_);
}

void DisplayManager::requestFullClear()
{
    portENTER_CRITICAL(&lock_);
    forceFull_ = true;
    portEXIT_CRITICAL(&lock_);
}

bool DisplayManager::hasPending() const
{
    portENTER_CRITICAL(&lock_);
    const bool pending = !dirty_.empty() || forceFull_;
    portEXIT_CRITICAL(&lock_);
    return pending;
}

void DisplayManager::holdFullRefresh(bool hold)
{
    if (hold == fullHold_) return;
    fullHold_ = hold;
    if (!hold && partialCount_ >= kPartialLimit) {
        // We suppressed the DC-balance full while held; do it now.
        requestFullClear();
    }
}

Rect DisplayManager::uiToRam(Rect ui)
{
    // The panel maps controller RAM (rx,ry) to the physical portrait pixel
    // (x = ry, y = RAM_W-1-rx)  — verified on hardware with a corner probe.
    // To make UI(x,y) land at physical(x,y) we transpose AND invert the RAM-x
    // (vertical) axis:  RAM.x = RAM_W - ui.y - ui.h,  RAM.y = ui.x.
    return {ssd1677::RAM_W - ui.y - ui.h, ui.x, ui.h, ui.w};
}

Rect DisplayManager::align8x(Rect ram)
{
    const int x0 = ram.x & ~7;
    const int x1 = (ram.x + ram.w + 7) & ~7;
    return {x0, ram.y, x1 - x0, ram.h};
}

// Repack `ram` from the framebuffer into plane1_. Returns true if any byte
// changed vs. what was already there (i.e. vs. what the panel currently shows).
bool DisplayManager::packMonoRegion(Rect ram)
{
    bool changed = false;
    for (int rx = ram.x; rx < ram.x + ram.w; ++rx) {
        const uint8_t mask = static_cast<uint8_t>(0x80u >> (rx & 7));
        const int byte_col = rx >> 3;
        const int uy = (ssd1677::RAM_W - 1) - rx;   // vertical-axis inversion
        for (int ry = ram.y; ry < ram.y + ram.h; ++ry) {
            const uint8_t g = fb_.pixel(/*ux=*/ry, uy);
            uint8_t& b = plane1_[static_cast<size_t>(ry) * ssd1677::RAM_STRIDE + byte_col];
            const uint8_t nb = (g == kWhite) ? (b | mask)
                                             : static_cast<uint8_t>(b & ~mask);
            if (nb != b) { b = nb; changed = true; }
        }
    }
    return changed;
}

bool DisplayManager::packMonoFull()
{
    return packMonoRegion({0, 0, ssd1677::RAM_W, ssd1677::RAM_H});
}

void DisplayManager::packGrayFull()
{
    // gray planes: p1 = g & 1, p2 = (g >> 1) & 1  (white=00 light=10 dark=01 black=11)
    for (int rx = 0; rx < ssd1677::RAM_W; ++rx) {
        const uint8_t mask = static_cast<uint8_t>(0x80u >> (rx & 7));
        const int byte_col = rx >> 3;
        const int uy = (ssd1677::RAM_W - 1) - rx;   // match packMonoRegion
        for (int ry = 0; ry < ssd1677::RAM_H; ++ry) {
            const uint8_t g = fb_.pixel(/*ux=*/ry, uy);
            const size_t idx = static_cast<size_t>(ry) * ssd1677::RAM_STRIDE + byte_col;
            if (g & 1) plane1_[idx] |= mask; else plane1_[idx] &= static_cast<uint8_t>(~mask);
            if (g & 2) plane2_[idx] |= mask; else plane2_[idx] &= static_cast<uint8_t>(~mask);
        }
    }
}

void DisplayManager::flush()
{
    if (!ready_) return;

    // Snapshot and clear the request state up front, atomically. A concurrent
    // input task may call invalidate() while the (long, blocking) refresh below
    // runs; that new dirty region must survive for the next flush rather than
    // being wiped when this one finishes.
    portENTER_CRITICAL(&lock_);
    if (dirty_.empty() && hint_ == RefreshHint::Auto && !forceFull_) {
        portEXIT_CRITICAL(&lock_);
        return;
    }
    const Rect d = dirty_.clampedTo(Framebuffer::kW, Framebuffer::kH);
    const RefreshHint hint = hint_;
    const bool forceFull = forceFull_;
    dirty_     = {};
    hint_      = RefreshHint::Auto;
    forceFull_ = false;
    portEXIT_CRITICAL(&lock_);

    const int64_t t0 = esp_timer_get_time();

    const bool wantQuality = (hint == RefreshHint::Quality) || forceFull;
    // While a hold is active the DC-balance forced full is deferred (see
    // holdFullRefresh) — but an explicit clear / a gray frame / a missing
    // baseline still must run.
    const bool overLimit   = partialCount_ >= kPartialLimit && !fullHold_;
    // Fold a due DC-balance clear into a screen transition: it repaints the
    // whole frame regardless, so no extra flash is spent.
    const bool foldIntoPage = hint == RefreshHint::Page &&
                              partialCount_ >= kPartialFoldIn && !fullHold_;
    const bool notMono     = !d.empty() && !fb_.rectIsMono(d);
    const bool doFull      = wantQuality || overLimit || foldIntoPage ||
                             notMono || !monoBaselineValid_;

    const char* mode;
    bool ok = true;
    if (doFull) {
        const bool anyGray = !fb_.rectIsMono({0, 0, Framebuffer::kW, Framebuffer::kH});
        if (anyGray) {
            packGrayFull();
            ok = ssd1677::refreshFullGray(plane1_, plane2_);
            monoBaselineValid_ = false;
            mode = "full-gray";
        } else {
            const bool changed = packMonoFull();
            // Skip a full refresh that would redraw identical pixels. An
            // explicit requestFullClear() (forceFull) always runs — that is the
            // deliberate "clear ghosting now" path; so does a DC-balance forced
            // full (overLimit) and the first full after boot (no baseline yet).
            if (!changed && !forceFull && !overLimit && monoBaselineValid_) {
                ESP_LOGD(TAG, "flush skipped (no change)");
                return;
            }
            ok = ssd1677::refreshFullMono(plane1_);
            monoBaselineValid_ = ok;
            mode = "full-mono";
        }
        partialCount_ = 0;
    } else {
        const Rect ram = align8x(uiToRam(d)).clampedTo(ssd1677::RAM_W, ssd1677::RAM_H);
        if (!packMonoRegion(ram)) {
            ESP_LOGD(TAG, "flush skipped (no change) dirty=(%d,%d %dx%d)", d.x, d.y, d.w, d.h);
            return;  // nothing on screen would change — don't spend a refresh
        }
        ok = ssd1677::refreshPartialMono(plane1_, {static_cast<uint16_t>(ram.x),
                                              static_cast<uint16_t>(ram.y),
                                              static_cast<uint16_t>(ram.w),
                                              static_cast<uint16_t>(ram.h)});
        ++partialCount_;
        mode = "partial";
    }

    if (!ok) {
        // The panel may not show what plane1_ now holds, so the no-change skip
        // must not trust it: drop the baseline (next flush goes full) and
        // re-queue the whole frame.
        // Retries are capped so a dead panel doesn't loop on BUSY timeouts.
        ESP_LOGE(TAG, "flush %s failed (streak %d)", mode, failStreak_ + 1);
        monoBaselineValid_ = false;
        if (++failStreak_ <= kMaxRetries) invalidateAll(RefreshHint::Quality);
        return;
    }
    failStreak_ = 0;

    const int ms = static_cast<int>((esp_timer_get_time() - t0) / 1000);
    ESP_LOGI(TAG, "flush %-9s dirty=(%d,%d %dx%d) partials=%d %dms",
             mode, d.x, d.y, d.w, d.h, partialCount_, ms);
}

}  // namespace core
