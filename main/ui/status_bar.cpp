#include "status_bar.h"

#include <algorithm>
#include <cstdio>

#include <esp_timer.h>

#include "hal_power.h"
#include "hal_rtc.h"
#include "i18n.h"
#include "iconset.h"
#include "settings.h"

namespace ui {
namespace {

// batt_bucket_ is percent/10 (0..10). Map it to a gauge icon in the UI sheet.
const char* batteryIconName(int bucket10)
{
    if (bucket10 >= 8) return "batt-full";
    if (bucket10 >= 4) return "batt-mid";
    if (bucket10 >= 1) return "batt-low";
    return "batt-empty";
}

}  // namespace

void StatusBar::setWifiConnected(bool c)
{
    if (c == wifi_) return;
    wifi_ = c;
    invalidate();
}

void StatusBar::rebuildClock()
{
    hal::rtc::DateTime dt;
    char buf[16];
    if (hal::rtc::get(dt) && !hal::rtc::timeInvalid()) {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour % 100, dt.minute % 100);
        last_minute_ = dt.hour * 60 + dt.minute;
    } else {
        std::snprintf(buf, sizeof(buf), "--:--");
        last_minute_ = -1;
    }
    if (clock_ != buf) { clock_ = buf; invalidate(); }
}

void StatusBar::rebuildBrightness()
{
    const int p = hal::power::brightnessPercent();
    if (p == bright_shown_) return;
    bright_shown_ = p;
    char buf[24];
    std::snprintf(buf, sizeof(buf), core::i18n::tr(core::i18n::BRIGHTNESS_FMT), p);
    bright_ = buf;
    invalidate();
}

void StatusBar::rebuildBattery()
{
    int b = (hal::power::batteryPercent() + 5) / 10;
    if (b < 0) b = 0;
    if (b > 10) b = 10;
    if (b == batt_bucket_) return;
    batt_bucket_ = b;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", b * 10);
    batt_ = buf;
    invalidate();
}

void StatusBar::tick()
{
    rebuildBrightness();  // cheap, reflects key presses immediately

    const int64_t t = esp_timer_get_time();
    if (t < next_poll_us_) return;
    next_poll_us_ = t + 1'000'000;
    rebuildClock();
    rebuildBattery();
}

void StatusBar::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kBlack);
    if (!font_ || !font_->valid()) return;

    const core::Settings& st = core::Settings::instance();
    const gfx::IconSet* icons = gfx::uiIcons();

    // Centre the glyph ink box (ascent+descent), not the line box — lineHeight
    // carries the font's line gap, which Nanum sizes large and CJK ~0, so
    // centring by it made KO/EN ride high. The bar height (setFont) matches.
    const int lh   = font_->lineHeight();
    const int inkH = font_->ascent() + font_->descent();
    const int by = bounds_.y + (bounds_.h - inkH) / 2 + font_->ascent();
    auto centreY = [&](int h) { return bounds_.y + (bounds_.h - h) / 2; };

    // clock (left)
    font_->draw(fb, bounds_.x + 10, by, clock_.c_str(), core::kWhite);

    // front-light: a short gauge, or "NN%"
    const int brx = bounds_.x + 100;
    if (st.brightnessGauge()) {
        constexpr int gw = 44, gh = 12;
        const int gy = centreY(gh);
        fb.frameRect({brx, gy, gw, gh}, 1, core::kWhite);
        const int fw = (gw - 4) * std::clamp(bright_shown_, 0, 100) / 100;
        if (fw > 0) fb.fillRect({brx + 2, gy + 2, fw, gh - 4}, core::kWhite);
    } else {
        font_->draw(fb, brx, by, bright_.c_str(), core::kWhite);
    }

    // battery (right): a gauge icon (default), or "NN%"
    const int slotRight = bounds_.right() - 10;
    int battLeft = slotRight;
    gfx::Icon bic = (st.batteryGauge() && icons)
                        ? icons->find(batteryIconName(batt_bucket_)) : gfx::Icon{};
    if (bic) {
        battLeft = slotRight - bic.w;
        icons->draw(fb, battLeft, centreY(bic.h), bic, core::kWhite);
    } else {
        const int w = font_->measure(batt_.c_str());
        battLeft = slotRight - w;
        font_->draw(fb, battLeft, by, batt_.c_str(), core::kWhite);
    }

    // Wi-Fi mark, just left of the battery slot.
    gfx::Icon wic = icons ? icons->find(wifi_ ? "wifi-good" : "wifi-off") : gfx::Icon{};
    if (wic) {
        icons->draw(fb, battLeft - 8 - wic.w, centreY(wic.h), wic, core::kWhite);
    } else {
        const int wx = battLeft - 8 - 15;
        const int wy = bounds_.y + bounds_.h - (bounds_.h - lh) / 2 - 2;
        for (int i = 0; i < 3; ++i) {
            const int h = 4 + i * 4;
            if (wifi_) fb.fillRect({wx + i * 5, wy - h, 3, h}, core::kWhite);
        }
    }
}

}  // namespace ui
