#pragma once

#include <cstdint>

#include "page_app.h"
#include "app_common.h"
#include "hal_wifi.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"

namespace apps {

// "빠른 설정" (quick settings): the page a pull-down from the status bar opens.
// Front-light -/+, Wi-Fi on/off, a ghost-clearing full refresh, battery and
// clock, and a shortcut to 설정. It is an ordinary page on the app stack, not an
// overlay. Brightness and Wi-Fi go through the same calls as the side keys and
// 설정 -> Wi-Fi, so each setting still lives in one place. See
// docs/design/gestures.md §4.
class QuickSettingsApp : public PageApp {
public:
    // The kernel compares the top app's id with this to ignore a second
    // pull-down while the page is already open.
    static constexpr const char* kId = "quick";

    QuickSettingsApp() : PageApp(kId) {}
    void onCreate(const AppContext& ctx) override;
    void onTick() override;

private:
    void stepBrightness(int delta);
    void refreshBrightness();
    void refreshWifi(bool force);
    void refreshStatus();

    ui::Label    bright_name_;
    ui::Button   bright_minus_;
    ui::Label    bright_value_;
    ui::Button   bright_plus_;

    ui::Label    wifi_name_;
    ui::Label    wifi_detail_;
    ui::Checkbox wifi_on_;

    ui::Button   clear_;
    ui::Label    status_;
    ui::Button   settings_;

    // What the labels show now, so onTick only touches them on a change.
    int  bright_shown_ = -1;
    bool wifi_on_shown_ = false;
    hal::wifi::Status wifi_status_shown_ = hal::wifi::Status::Off;
    int64_t next_status_us_ = 0;
};

}  // namespace apps
