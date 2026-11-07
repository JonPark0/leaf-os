/*
 * A stand-in app for grid entries whose real implementation lands in a later
 * phase. Shows a back button and its own name, nothing else.
 */
#pragma once

#include "app.h"
#include "app_common.h"
#include "widgets/button.h"
#include "widgets/label.h"

namespace apps {

class PlaceholderApp : public App {
public:
    explicit PlaceholderApp(const char* id, const char* name) : meta_{id, name} {}

    void onCreate(const AppContext& ctx) override
    {
        App::onCreate(ctx);
        screen_.setBackground(core::kWhite);

        makeBackButton(back_, ctx);
        screen_.add(&back_);

        title_.setFont(ctx.ui_font);
        title_.setText(meta_.name);
        title_.setBounds(contentRect());
        title_.setAlign(ui::Label::Align::Center);
        screen_.add(&title_);
    }

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }
    bool onBack() override { return false; }

private:
    AppMeta meta_;
    ui::Screen screen_;
    ui::Button back_{nullptr};
    ui::Label  title_{nullptr};
};

}  // namespace apps
