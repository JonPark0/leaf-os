#include "app_quick.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "app_settings/app_settings.h"
#include "app_wifi/app_wifi.h"
#include "display_manager.h"
#include "hal_power.h"
#include "hal_rtc.h"
#include "hal_sys.h"
#include "i18n.h"
#include "settings.h"

namespace apps {
namespace {
using namespace core::i18n;

constexpr int kRows    = 5;
constexpr int kMinRowH = 56;   // a comfortable tap target at the default font
constexpr int kRowPad  = 20;   // row height above the font's line height
constexpr int kGap     = 12;   // between rows
constexpr int kStepW   = 60;   // the - and + buttons
constexpr int kCheckW  = 80;   // checkbox touch area; it draws its box at the right edge
constexpr int64_t kStatusPeriodUs = 1'000'000;

// Wi-Fi row detail: the network when connected, otherwise what the radio is
// doing.
const char* wifiDetail(bool on, hal::wifi::Status s)
{
    if (!on) return tr(SET_WIFI_OFF);
    switch (s) {
        case hal::wifi::Status::Connected: {
            const std::string& ssid = core::Settings::instance().wifiSsid();
            return ssid.empty() ? tr(WIFI_CONNECTED) : ssid.c_str();
        }
        case hal::wifi::Status::Connecting: return tr(WIFI_CONNECTING);
        default:                            return tr(WIFI_OFF);
    }
}
}  // namespace

void QuickSettingsApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, tr(QS_TITLE));

    const gfx::Font* font = ctx.ui_font;
    const bool have_font = font && font->valid();
    auto textW = [&](const char* s) { return have_font ? font->measure(s) : 0; };

    // Rows grow with the system font so a larger size still fits its text, but
    // never past an equal share of the body, so all five stay on screen.
    const int top  = ui::metrics::bodyTop();
    const int lh   = have_font ? font->lineHeight() : 0;
    const int fit  = (ui::metrics::kScreenH - ui::metrics::kMargin - top - (kRows - 1) * kGap) / kRows;
    const int rowH = std::min(std::max(kMinRowH, lh + kRowPad), fit);
    auto rowY = [&](int i) { return top + i * (rowH + kGap); };

    const int left  = ui::metrics::kMargin;
    const int right = ui::metrics::kScreenW - ui::metrics::kMargin;   // control columns end here
    const int fullW = right - left;

    // 1. Brightness: name | - | NN% | +, the controls right-aligned. The value
    // column is wide enough for "100%" in the current font.
    int y = rowY(0);
    const int plusX  = right - kStepW;
    const int valueW = std::max(96, textW("100%") + 16);
    const int valueX = plusX - 4 - valueW;
    const int minusX = valueX - 4 - kStepW;
    const int nameW  = minusX - 8 - left;

    bright_name_.setFont(font);
    bright_name_.setText(tr(DISP_BRIGHTNESS));
    bright_name_.setAlign(ui::Label::Align::Left);
    bright_name_.setBounds({left, y, nameW, rowH});
    screen_.add(&bright_name_);

    bright_minus_.setFont(font);
    bright_minus_.setLabel("-");
    bright_minus_.setBounds({minusX, y, kStepW, rowH});
    bright_minus_.onClick([this] { stepBrightness(-1); });
    screen_.add(&bright_minus_);

    bright_value_.setFont(font);
    bright_value_.setAlign(ui::Label::Align::Center);
    bright_value_.setBounds({valueX, y, valueW, rowH});
    screen_.add(&bright_value_);

    bright_plus_.setFont(font);
    bright_plus_.setLabel("+");
    bright_plus_.setBounds({plusX, y, kStepW, rowH});
    bright_plus_.onClick([this] { stepBrightness(+1); });
    screen_.add(&bright_plus_);

    // 2. Wi-Fi: name, then the state from just after it up to the checkbox, so
    // a network name gets all the room there is. The checkbox fills its whole
    // rect white when it paints, so no label may reach under it.
    y = rowY(1);
    const int checkX    = right - kCheckW;
    const int wifiNameW = std::min(textW(tr(QS_WIFI)), nameW);
    const int detailX   = left + wifiNameW + 16;

    wifi_name_.setFont(font);
    wifi_name_.setText(tr(QS_WIFI));
    wifi_name_.setAlign(ui::Label::Align::Left);
    wifi_name_.setBounds({left, y, wifiNameW, rowH});
    screen_.add(&wifi_name_);

    wifi_detail_.setFont(font);
    wifi_detail_.setAlign(ui::Label::Align::Left);
    wifi_detail_.setBounds({detailX, y, checkX - 8 - detailX, rowH});
    screen_.add(&wifi_detail_);

    wifi_on_.setFont(font);
    wifi_on_.setBounds({checkX, y, kCheckW, rowH});
    wifi_on_.onToggle([this](bool on) {
        setWifiEnabled(on);
        refreshWifi(true);
    });
    screen_.add(&wifi_on_);

    // 3. Clear screen: the ghosting the user means is on the screen underneath,
    // so close this page and let that repaint be the clean one. The pop queues
    // a fast full-frame refresh (RefreshHint::Page); the pending full clear
    // turns that same flush into a clean full refresh, so it is still one.
    y = rowY(2);
    clear_.setFont(font);
    clear_.setLabel(tr(QS_CLEAR_SCREEN));
    clear_.setBounds({left, y, fullW, rowH});
    clear_.onClick([this] {
        core::DisplayManager::instance().requestFullClear();
        if (ctx_.pop) ctx_.pop();
    });
    screen_.add(&clear_);

    // 4. Battery and clock, kept current by onTick.
    y = rowY(3);
    status_.setFont(font);
    status_.setAlign(ui::Label::Align::Center);
    status_.setBounds({left, y, fullW, rowH});
    screen_.add(&status_);

    // 5. 설정: close this page first, so Back from 설정 returns to the screen
    // the user pulled down from. Both are queued; on the next tick the kernel
    // replaces this page with 설정, leaving the screen beneath paused.
    y = rowY(4);
    settings_.setFont(font);
    settings_.setLabel(tr(APP_SETTINGS));
    settings_.setBounds({left, y, fullW, rowH});
    settings_.onClick([this] {
        if (!ctx_.pop || !ctx_.push) return;
        ctx_.pop();
        ctx_.push(new SettingsApp());
    });
    screen_.add(&settings_);

    refreshBrightness();
    refreshWifi(true);
    refreshStatus();
    next_status_us_ = hal::sys::nowUs() + kStatusPeriodUs;
}

void QuickSettingsApp::onTick()
{
    // The side keys step the front-light while this page is up, and Wi-Fi
    // connects in the background. Both are plain reads, cheap every tick.
    refreshBrightness();
    refreshWifi(false);

    // Battery and clock go over I2C, so once a second, as the status bar does.
    const int64_t t = hal::sys::nowUs();
    if (t < next_status_us_) return;
    next_status_us_ = t + kStatusPeriodUs;
    refreshStatus();
}

// The same 10 % step and the same saved value as 설정 -> 화면 (DisplayApp).
void QuickSettingsApp::stepBrightness(int delta)
{
    const int p = std::clamp(hal::power::brightnessPercent() + delta * 10, 0, 100);
    hal::power::setBrightnessPercent(p);
    core::Settings::instance().setBrightnessPerMille(hal::power::brightness());
    refreshBrightness();
}

void QuickSettingsApp::refreshBrightness()
{
    const int p = hal::power::brightnessPercent();
    if (p == bright_shown_) return;
    bright_shown_ = p;
    char b[16];
    std::snprintf(b, sizeof(b), "%d%%", p);
    bright_value_.setText(b);
}

// `force` repaints even when nothing changed (first fill, or right after the
// user toggled, when the radio has not reported a new status yet).
void QuickSettingsApp::refreshWifi(bool force)
{
    const bool on = core::Settings::instance().wifiAutoConnect();
    const hal::wifi::Status s = hal::wifi::status();
    if (!force && on == wifi_on_shown_ && s == wifi_status_shown_) return;
    wifi_on_shown_     = on;
    wifi_status_shown_ = s;
    wifi_on_.setChecked(on);
    wifi_detail_.setText(wifiDetail(on, s));
}

void QuickSettingsApp::refreshStatus()
{
    char batt[16];
    const int pct = hal::power::batteryPercent();
    if (pct < 0) std::snprintf(batt, sizeof(batt), "--");
    else         std::snprintf(batt, sizeof(batt), "%d%%", pct);

    // The status bar's clock source, so the two always agree, including
    // "--:--" while the RTC has not been set.
    char hhmm[16];
    hal::rtc::DateTime dt;
    if (hal::rtc::get(dt) && !hal::rtc::timeInvalid())
        std::snprintf(hhmm, sizeof(hhmm), "%02d:%02d", dt.hour % 100, dt.minute % 100);
    else
        std::snprintf(hhmm, sizeof(hhmm), "--:--");

    char b[96];
    std::snprintf(b, sizeof(b), tr(QS_STATUS_FMT), batt, hhmm);
    status_.setText(b);   // repaints only when the text changed
}

}  // namespace apps
