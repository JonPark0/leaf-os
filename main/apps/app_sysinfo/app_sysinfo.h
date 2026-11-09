#pragma once

#include <string>
#include <vector>

#include "page_app.h"
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
class SysInfoApp : public PageApp {
public:
    SysInfoApp() : PageApp("sysinfo") {}
    void onCreate(const AppContext& ctx) override;
    void onResume() override;

private:
    void rebuild();

    ui::TextView body_;
    ui::Label    batt_cb_name_;
    ui::Checkbox batt_cb_;
    ui::Label    bri_cb_name_;
    ui::Checkbox bri_cb_;
};

}  // namespace apps
