#include "app_sysinfo.h"

#include <cstdio>
#include <cstdint>


#include "hal_power.h"
#include "hal_storage.h"
#include "hal_sys.h"
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

void SysInfoApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, tr(SET_SYSINFO));

    const int top = bodyTop();
    body_.setFont(ctx.ui_font);
    body_.setAlign(ui::TextView::Align::Center);
    body_.setBounds({0, top + 8, ui::metrics::kScreenW, 352});
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
                  humanBytes(hal::sys::freeHeapBytes()).c_str(),
                  humanBytes(hal::sys::freePsramBytes()).c_str(),
                  tr(SYS_FREE));
    L.push_back(b);
    L.push_back("");

    L.push_back(tr(SYS_UPTIME));
    const int64_t sec = hal::sys::nowUs() / 1000000;
    std::snprintf(b, sizeof(b), tr(SYS_UPTIME_FMT),
                  static_cast<int>(sec / 3600), static_cast<int>((sec % 3600) / 60));
    L.push_back(b);

    body_.setLines(std::move(L));
}

}  // namespace apps
