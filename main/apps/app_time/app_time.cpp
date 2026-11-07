#include "app_time.h"

#include <cstdio>
#include <sys/time.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "hal_wifi.h"
#include "i18n.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr char TAG[] = "app_time";

const char* fieldName(int f)
{
    switch (f) {
        case 0:  return tr(TIME_YEAR);
        case 1:  return tr(TIME_MONTH);
        case 2:  return tr(TIME_DAY);
        case 3:  return tr(TIME_HOUR);
        default: return tr(TIME_MIN);
    }
}

int daysInMonth(int year, int month)
{
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return d[(month - 1) % 12];
}

int wrap(int v, int lo, int hi)  // inclusive range, wrap-around
{
    const int span = hi - lo + 1;
    return lo + ((v - lo) % span + span) % span;
}

int weekdayOf(const hal::rtc::DateTime& d)
{
    // 1970-01-01 was a Thursday (tm_wday 4).
    const long long days = hal::rtc::toUnix(d) / 86400;
    return static_cast<int>(((days % 7) + 7 + 4) % 7);
}
}  // namespace

void TimeApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    title_.setFont(ctx.ui_font);
    title_.setText(tr(SET_TIME));
    title_.setBounds({130, ui::StatusBar::kHeight + 10, 220, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const int top = bodyTop();
    const int rowH = 64, gap = 10;
    for (int f = 0; f < kFieldCount; ++f) {
        const int y = top + f * (rowH + gap);

        name_[f].setFont(ctx.ui_font);
        name_[f].setText(fieldName(f));
        name_[f].setBounds({24, y, 60, rowH});
        name_[f].setAlign(ui::Label::Align::Left);
        screen_.add(&name_[f]);

        minus_[f].setFont(ctx.ui_font);
        minus_[f].setLabel("-");   // ASCII: the font has no U+2212 minus sign
        minus_[f].setBounds({96, y, 72, rowH});
        minus_[f].onClick([this, f] { bump(f, -1); });
        screen_.add(&minus_[f]);

        value_[f].setFont(ctx.ui_font);
        value_[f].setBounds({176, y, 128, rowH});
        value_[f].setAlign(ui::Label::Align::Center);
        screen_.add(&value_[f]);

        plus_[f].setFont(ctx.ui_font);
        plus_[f].setLabel("+");
        plus_[f].setBounds({312, y, 72, rowH});
        plus_[f].onClick([this, f] { bump(f, +1); });
        screen_.add(&plus_[f]);
    }

    const int by = top + kFieldCount * (rowH + gap) + 8;
    save_.setFont(ctx.ui_font);
    save_.setLabel(tr(SAVE));
    save_.setBounds({24, by, 160, 52});
    save_.onClick([this] { save(); });
    screen_.add(&save_);

    ntp_.setFont(ctx.ui_font);
    ntp_.setLabel(tr(TIME_NTP));
    ntp_.setBounds({200, by, 184, 52});
    ntp_.onClick([this] {
        if (!hal::wifi::connected()) {
            status_.setText("");
            status2_.setText(tr(TIME_WIFI_DOWN));
            return;
        }
        hal::wifi::syncTimeSoon();
        ntp_baseline_ = hal::wifi::ntpSyncCount();
        ntp_deadline_us_ = esp_timer_get_time() + 15'000'000;  // 15 s
        status_.setText(tr(TIME_NTP_WAIT));
        status2_.setText("");
    });

    status_.setFont(ctx.ui_font);
    status_.setBounds({24, by + 64, 432, 30});
    status_.setAlign(ui::Label::Align::Left);
    screen_.add(&status_);

    status2_.setFont(ctx.ui_font);
    status2_.setBounds({24, by + 96, 432, 30});
    status2_.setAlign(ui::Label::Align::Left);
    screen_.add(&status2_);

    loadFromRtc();
}

void TimeApp::onTick()
{
    if (ntp_deadline_us_ == 0) return;

    if (hal::wifi::ntpSyncCount() != ntp_baseline_) {
        ntp_deadline_us_ = 0;
        hal::rtc::get(dt_);                  // pull the freshly-set time in
        refresh();
        char b[64];
        std::snprintf(b, sizeof(b), tr(TIME_SYNC_OK_FMT),
                      dt_.year, dt_.month, dt_.day, dt_.hour, dt_.minute);
        status2_.setText(b);
    } else if (esp_timer_get_time() > ntp_deadline_us_) {
        ntp_deadline_us_ = 0;
        status2_.setText(tr(TIME_SYNC_TIMEOUT));
    }
}

void TimeApp::onResume()
{
    // Add / remove the NTP button to match the current link state.
    const bool want = hal::wifi::connected();
    if (want && !have_ntp_row_) { screen_.add(&ntp_); have_ntp_row_ = true; }
    loadFromRtc();
}

void TimeApp::loadFromRtc()
{
    if (!hal::rtc::get(dt_) || hal::rtc::timeInvalid()) {
        dt_ = hal::rtc::DateTime{2025, 1, 1, 12, 0, 0, 3};
        status_.setText(tr(TIME_CLOCK_UNSET));
    } else {
        status_.setText("");
    }
    refresh();
}

void TimeApp::bump(int f, int delta)
{
    switch (f) {
        case kYear:   dt_.year   = wrap(dt_.year + delta, 2020, 2099); break;
        case kMonth:  dt_.month  = wrap(dt_.month + delta, 1, 12);     break;
        case kDay:    dt_.day    = wrap(dt_.day + delta, 1, daysInMonth(dt_.year, dt_.month)); break;
        case kHour:   dt_.hour   = wrap(dt_.hour + delta, 0, 23);      break;
        case kMinute: dt_.minute = wrap(dt_.minute + delta, 0, 59);    break;
    }
    // Keep the day valid if a month/year change shortened it.
    const int dim = daysInMonth(dt_.year, dt_.month);
    if (dt_.day > dim) dt_.day = dim;
    refresh();
}

void TimeApp::refresh()
{
    char b[16];
    std::snprintf(b, sizeof(b), "%04d", dt_.year);   value_[kYear].setText(b);
    std::snprintf(b, sizeof(b), "%02d", dt_.month);  value_[kMonth].setText(b);
    std::snprintf(b, sizeof(b), "%02d", dt_.day);    value_[kDay].setText(b);
    std::snprintf(b, sizeof(b), "%02d", dt_.hour);   value_[kHour].setText(b);
    std::snprintf(b, sizeof(b), "%02d", dt_.minute); value_[kMinute].setText(b);
}

void TimeApp::save()
{
    dt_.second = 0;
    dt_.weekday = weekdayOf(dt_);
    if (hal::rtc::set(dt_)) {
        ESP_LOGI(TAG, "RTC set to %04d-%02d-%02d %02d:%02d",
                 dt_.year, dt_.month, dt_.day, dt_.hour, dt_.minute);
        struct timeval tv{};
        tv.tv_sec = hal::rtc::toUnix(dt_);
        settimeofday(&tv, nullptr);   // keep the C clock in sync without a reboot
        status_.setText(tr(TIME_SAVED));
    } else {
        status_.setText(tr(TIME_SAVE_FAIL));
    }
}

}  // namespace apps
