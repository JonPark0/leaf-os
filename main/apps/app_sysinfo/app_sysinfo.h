#pragma once

#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "widget.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"

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
    class Body : public ui::Widget {
    public:
        void setContent(const gfx::Font* f, std::vector<std::string> lines)
        {
            font_ = f;
            lines_ = std::move(lines);
            invalidate();
        }
        void draw(core::Framebuffer& fb) override;
    private:
        const gfx::Font* font_ = nullptr;
        std::vector<std::string> lines_;
    };

    void rebuild();

    AppMeta meta_{"sysinfo", "시스템 상태"};
    ui::Screen  screen_;
    ui::Button  back_;
    ui::Label   title_;
    Body        body_;
    ui::Label    batt_cb_name_;
    ui::Checkbox batt_cb_;
    ui::Label    bri_cb_name_;
    ui::Checkbox bri_cb_;
};

}  // namespace apps
