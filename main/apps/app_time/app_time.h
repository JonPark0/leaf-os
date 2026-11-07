#pragma once

#include <array>

#include "app.h"
#include "app_common.h"
#include "hal_rtc.h"
#include "widgets/button.h"
#include "widgets/label.h"

namespace apps {

// Manual clock set: five −/+ steppers (year, month, day, hour, minute) plus
// Save, and (when Wi-Fi is up) an NTP-sync shortcut. Replaces the old stub that
// jammed the RTC to 2025-01-01 12:00.
class TimeApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onTick() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    enum Field { kYear, kMonth, kDay, kHour, kMinute, kFieldCount };

    void loadFromRtc();
    void bump(int field, int delta);
    void refresh();          // repaint value labels
    void save();

    AppMeta meta_{"time", "시간 설정"};
    ui::Screen screen_;
    ui::Button back_;
    ui::Label  title_;
    ui::Label  status_;    // "동기화 중…"
    ui::Label  status2_;   // completion line

    uint32_t   ntp_baseline_ = 0;
    int64_t    ntp_deadline_us_ = 0;   // 0 = not waiting

    std::array<ui::Label, kFieldCount>  name_;
    std::array<ui::Label, kFieldCount>  value_;
    std::array<ui::Button, kFieldCount> minus_;
    std::array<ui::Button, kFieldCount> plus_;
    ui::Button save_;
    ui::Button ntp_;

    hal::rtc::DateTime dt_{};
    bool have_ntp_row_ = false;
};

}  // namespace apps
