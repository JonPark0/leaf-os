#pragma once

#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "widget.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"
#include "widgets/text_view.h"

namespace apps {

// "시스템 상태": a snapshot of battery, SD card, memory and uptime, plus the two
// status-bar readout toggles (battery gauge / brightness gauge). Values are
// rebuilt on entry — re-open to refresh.
class SysInfoApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    void rebuild();

    AppMeta meta_{"sysinfo", "시스템 상태"};
    ui::Screen  screen_;
    ui::Button  back_;
    ui::Label   title_;
    ui::TextView body_;
    ui::Label    batt_cb_name_;
    ui::Checkbox batt_cb_;
    ui::Label    bri_cb_name_;
    ui::Checkbox bri_cb_;
};

}  // namespace apps
