/*
 * PageApp — the base for an ordinary app screen: white background, a back
 * button and a title in the header (ui/metrics.h), and the body below for the
 * subclass. It owns the Screen and the AppMeta, so a subclass only declares its
 * own widgets.
 *
 *   class FooApp : public PageApp {
 *   public:
 *       FooApp() : PageApp("foo") {}
 *       void onCreate(const AppContext& ctx) override {
 *           setupPage(ctx, tr(FOO_TITLE));   // first: also runs App::onCreate
 *           list_.setBounds(ui::metrics::body());
 *           screen_.add(&list_);
 *       }
 *   };
 *
 * Full-screen apps that draw their own chrome (home, the reader, Lua apps)
 * derive from App directly.
 */
#pragma once

#include <string>

#include "app.h"
#include "app_common.h"
#include "metrics.h"
#include "widgets/button.h"
#include "widgets/label.h"

namespace apps {

class PageApp : public App {
public:
    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

protected:
    enum class TitleStyle {
        Centered,     // 220 px, centred (most pages)
        CenteredWide, // 240 px, centred, when the header's right side is free
        BesideBack,   // left-aligned from the back button to the right edge
        None,         // no title label (the subclass draws its own)
    };

    explicit PageApp(const char* id) : meta_{id} {}

    // Runs App::onCreate, sets the background and adds the back button (pops
    // the app) and the title. Call first in onCreate. Rewire back_ afterwards
    // for a different back action.
    void setupPage(const AppContext& ctx, const std::string& title,
                   TitleStyle style = TitleStyle::Centered);

    // Adds the back button and title to the screen again, after clearWidgets().
    void addHeader();

    ui::Screen screen_;
    ui::Button back_;
    ui::Label  title_;

private:
    AppMeta meta_;
    bool has_title_ = true;
};

}  // namespace apps
