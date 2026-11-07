#include "app_sysinfo.h"

#include <cstdio>
#include <cstdint>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "hal_power.h"
#include "hal_storage.h"
#include "i18n.h"
#include "settings.h"

namespace apps {
namespace {
using namespace core::i18n;

std::string humanBytes(uint64_t b)
{
    char s[24];
    if (b >= (1ull << 30))      std::snprintf(s, sizeof(s), "%.1f GB", b / double(1ull << 30));
    else if (b >= (1ull << 20)) std::snprintf(s, sizeof(s), "%.0f MB", b / double(1ull << 20));
    else                        std::snprintf(s, sizeof(s), "%.0f KB", b / 1024.0);
    return s;
}
}  // namespace

void SysInfoApp::Body::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);
    if (!font_ || !font_->valid()) return;

    const int lh = font_->lineHeight();
    int y = bounds_.y + 8;
    for (const std::string& s : lines_) {
        if (s.empty()) { y += lh / 2; continue; }
        if (y + lh > bounds_.y + bounds_.h) break;
        const int w = font_->measure(s.c_str());
        font_->draw(fb, bounds_.x + (bounds_.w - w) / 2, y + font_->ascent(),
                    s.c_str(), core::kBlack);
        y += lh;
    }
}

void SysInfoApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    title_.setFont(ctx.ui_font);
    title_.setText(tr(SET_SYSINFO));
    title_.setBounds({130, ui::StatusBar::kHeight + 10, 220, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const int top = bodyTop();
    body_.setBounds({0, top, 480, 360});
    screen_.add(&body_);

    const int cy = top + 372;
    const int rowH = 56, rowGap = 12;
    auto addToggle = [&](ui::Label& name, ui::Checkbox& cb, int y, Msg label,
                         bool checked, void (core::Settings::*setter)(bool)) {
        name.setFont(ctx.ui_font);
        name.setText(tr(label));
        name.setAlign(ui::Label::Align::Left);
        name.setBounds({16, y, 300, rowH});
        screen_.add(&name);

        cb.setFont(ctx.ui_font);
        cb.setBounds({330, y, 130, rowH});
        cb.setChecked(checked);
        cb.onToggle([setter](bool on) {
            (core::Settings::instance().*setter)(on);
            // The status bar picks up the new style on the next screen change
            // (a Screen::invalidateAll here would force a ~4 s full flash).
        });
        screen_.add(&cb);
    };

    const core::Settings& st = core::Settings::instance();
    addToggle(batt_cb_name_, batt_cb_, cy, DISP_BATT_GAUGE,
              st.batteryGauge(), &core::Settings::setBatteryGauge);
    addToggle(bri_cb_name_, bri_cb_, cy + rowH + rowGap, DISP_BRIGHT_GAUGE,
              st.brightnessGauge(), &core::Settings::setBrightnessGauge);

    rebuild();
}

void SysInfoApp::onResume() { rebuild(); }

void SysInfoApp::rebuild()
{
    char b[80];
    std::vector<std::string> L;

    const int mv = hal::power::batteryMilliVolts();
    L.push_back(tr(SYS_BATTERY));
    std::snprintf(b, sizeof(b), "%d%%    %d.%02d V",
                  hal::power::batteryPercent(), mv / 1000, (mv % 1000) / 10);
    L.push_back(b);
    L.push_back("");

    L.push_back(tr(SYS_STORAGE));
    if (hal::storage::state() == hal::storage::State::Mounted) {
        uint64_t total = 0, freeb = 0;
        hal::storage::usage(&total, &freeb);
        std::snprintf(b, sizeof(b), "%s / %s %s",
                      humanBytes(total - freeb).c_str(), humanBytes(total).c_str(),
                      tr(SYS_USED));
        L.push_back(b);
    } else {
        L.push_back(tr(SYS_NONE));
    }
    L.push_back("");

    L.push_back(tr(SYS_MEMORY));
    std::snprintf(b, sizeof(b), "heap %s  ·  PSRAM %s  %s",
                  humanBytes(esp_get_free_heap_size()).c_str(),
                  humanBytes(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)).c_str(),
                  tr(SYS_FREE));
    L.push_back(b);
    L.push_back("");

    L.push_back(tr(SYS_UPTIME));
    const int64_t sec = esp_timer_get_time() / 1000000;
    std::snprintf(b, sizeof(b), tr(SYS_UPTIME_FMT),
                  static_cast<int>(sec / 3600), static_cast<int>((sec % 3600) / 60));
    L.push_back(b);

    body_.setContent(ctx_.ui_font, std::move(L));
}

}  // namespace apps
