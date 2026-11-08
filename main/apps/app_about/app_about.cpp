#include "app_about.h"

#include <string>

#include "hal_sys.h"
#include "i18n.h"
#include "lua/lua_bindings.h"

namespace apps {
namespace { using namespace core::i18n; }

void AboutApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    title_.setFont(ctx.ui_font);
    title_.setText(tr(SET_ABOUT));
    title_.setBounds({130, ui::StatusBar::kHeight + 10, 220, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const std::string version =
        std::string(tr(ABOUT_VERSION)) + " " + hal::sys::firmwareVersion();

    body_.setFont(ctx.ui_font);
    body_.setAlign(ui::TextView::Align::Center);
    body_.setBounds({0, bodyTop() + 8, 480, 800 - bodyTop() - 8});
    body_.setLines({
        "LEAF OS",
        "Lightweight/Lua EPD Application Framework",
        version,
        "Lua 5.4 · API " + std::to_string(kLuaApiLevel),
        "",
        tr(ABOUT_HW),
        "M5Stack PaperMono-Lite (C153-Lite)",
        "ESP32-S3R8 · SSD1677 · FT6336G",
        "RX8130CE · M5PM1 · M5IOE1",
        "",
        tr(ABOUT_FW),
        "ESP-IDF 5.5.5  (Apache-2.0)",
        "arduino-esp32 3.3.x  (LGPL-2.1)",
        "M5Unified / M5GFX  (MIT)",
        "",
        tr(ABOUT_FONT_HDR),
        tr(ABOUT_FONT_LINE),
        tr(ABOUT_FONT_LICENSE),
        "",
        tr(ABOUT_ICON_HDR),
        "Dazzle Line Icons (SVGRepo)",
        "",
        tr(ABOUT_MINIZ),
        "",
        tr(ABOUT_OSS_NOTE),
    });
    screen_.add(&body_);
}

}  // namespace apps
