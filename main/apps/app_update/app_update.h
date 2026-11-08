/*
 * 설정 → 펌웨어 업데이트: check the firmware feed, download the new image into
 * the other slot and restart into it. See docs/design/ota-update.md.
 *
 * Network calls run from onTick a few ticks after the status text changes, so
 * "checking…" paints first. The image download runs on ota::Updater's task;
 * leaving the screen cancels it.
 */
#pragma once

#include <cstdint>
#include <string>

#include "app.h"
#include "app_common.h"
#include "ota_client.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_view.h"

namespace apps {

class UpdateApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onTick() override;
    void onDestroy() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    enum class Phase { Idle, Checking, Checked, Updating, Restarting };

    void refresh();     // body lines and button from the phase
    void tapButton();
    void check();
    void startUpdate();

    AppMeta meta_{"update", "펌웨어 업데이트"};
    ui::Screen    screen_;
    ui::Button    back_;
    ui::Label     title_;
    ui::TextView  body_;
    ui::Button    button_;
    ui::Label     status_;

    Phase phase_ = Phase::Idle;
    bool has_feed_ = false;
    bool newer_ = false;
    ota::Feed feed_;
    std::string base_;
    int check_in_ticks_ = -1;
    int last_pct_ = -1;
    int64_t restart_at_us_ = 0;
};

}  // namespace apps
