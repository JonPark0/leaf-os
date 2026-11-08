#pragma once

#include <cstdint>

#include "page_app.h"
#include "app_common.h"
#include "widgets/button.h"
#include "widgets/label.h"

namespace apps {

// Settings → 전원 sub-page: 재시작 / 전원 끄기. Each action needs two taps —
// the first arms the button (label switches to "한 번 더 눌러 확인"); a second
// tap within 5 s runs it. Any other tap, or the timeout, disarms it. E-ink tap
// feedback is slow, so a single tap must never power the device down.
class PowerApp : public PageApp {
public:
    PowerApp() : PageApp("power") {}
    void onCreate(const AppContext& ctx) override;
    void onTick() override;

private:
    enum Action { kNone, kRestart, kShutdown };

    void tap(Action a);
    void disarm();

    ui::Button restart_;
    ui::Button shutdown_;
    ui::Label  hint_;

    Action  armed_ = kNone;
    int64_t armed_us_ = 0;
};

}  // namespace apps
