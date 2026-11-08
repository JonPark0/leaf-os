/*
 * Small shared helpers for app screens: a standard back button and the content
 * rectangle below the status bar.
 */
#pragma once

#include <cstdio>
#include <string>

#include "app.h"
#include "geometry.h"
#include "i18n.h"
#include "status_bar.h"
#include "widgets/button.h"

namespace apps {

inline core::Rect contentRect()
{
    const int top = ui::StatusBar::kHeight;
    return {0, top, 480, 800 - top};
}

// Places `btn` as a back control just under the status bar and wires it to pop.
// A null label uses the translated default ("← 뒤로" / "← Back").
inline void makeBackButton(ui::Button& btn, const AppContext& ctx, const char* label = nullptr)
{
    btn.setFont(ctx.ui_font);
    btn.setLabel(label ? label : core::i18n::tr(core::i18n::BACK));
    btn.setBounds({8, ui::StatusBar::kHeight + 8, 104, 42});
    btn.onClick([ctx] { if (ctx.pop) ctx.pop(); });
}

inline int bodyTop() { return ui::StatusBar::kHeight + 60; }

// `err` from hal_http (or code built on it) as one line that fits the screen:
// connection-level failures (no route, timeout, captive portal) read as "can't
// reach the internet"; anything else is put into `fmt` ("...: %s").
inline std::string netFailText(const char* fmt, const std::string& err)
{
    if (err.rfind("ESP_ERR_HTTP_", 0) == 0 || err.rfind("ESP_ERR_ESP_TLS_", 0) == 0 ||
        err == "ESP_FAIL" || err == "connection closed" || err == "read failed")
        return core::i18n::tr(core::i18n::STORE_NET_FAIL);
    char buf[160];
    std::snprintf(buf, sizeof(buf), fmt, err.c_str());
    return buf;
}

}  // namespace apps
