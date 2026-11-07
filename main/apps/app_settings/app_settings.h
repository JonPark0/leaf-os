#pragma once

#include <vector>

#include "app.h"
#include "app_common.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/list_view.h"

namespace apps {

class SettingsApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    enum RowKind { kRestart, kDisplay, kWifi, kTime, kLanguage, kSysInfo, kPower, kAbout };

    void rebuildRows();
    void onRow(int idx);

    AppMeta meta_{"settings", "설정"};
    ui::Screen   screen_;
    ui::Button   back_;
    ui::Label    title_;
    ui::ListView list_{nullptr};
    std::vector<RowKind> row_kind_;
};

}  // namespace apps
