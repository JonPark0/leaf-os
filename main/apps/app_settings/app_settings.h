#pragma once

#include <vector>

#include "page_app.h"
#include "app_common.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/list_view.h"

namespace apps {

class SettingsApp : public PageApp {
public:
    SettingsApp() : PageApp("settings") {}
    void onCreate(const AppContext& ctx) override;
    void onResume() override;

private:
    enum RowKind { kRestart, kDisplay, kWifi, kTime, kLanguage, kSysInfo, kPower, kUpdate, kAbout };

    void rebuildRows();
    void onRow(int idx);

    ui::ListView list_{nullptr};
    std::vector<RowKind> row_kind_;
};

}  // namespace apps
