#pragma once

#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "widget.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_view.h"

namespace apps {

// "정보" page: version, hardware, and third-party / open-source attributions.
// Centred, wrapped text, paged when it runs past the screen.
class AboutApp : public App {
public:
    void onCreate(const AppContext& ctx) override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    AppMeta meta_{"about", "정보"};
    ui::Screen screen_;
    ui::Button back_;
    ui::Label  title_;
    ui::TextView body_;
};

}  // namespace apps
