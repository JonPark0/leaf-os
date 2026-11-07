#pragma once

#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "widget.h"
#include "widgets/button.h"
#include "widgets/label.h"

namespace apps {

// "정보" page: version, hardware, and third-party / open-source attributions.
// Plain centred text — no list separators or chevrons.
class AboutApp : public App {
public:
    void onCreate(const AppContext& ctx) override;

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

    AppMeta meta_{"about", "정보"};
    ui::Screen screen_;
    ui::Button back_;
    ui::Label  title_;
    Body       body_;
};

}  // namespace apps
