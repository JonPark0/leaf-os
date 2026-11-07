/*
 * Small shared helpers for app screens: a standard back button and the content
 * rectangle below the status bar.
 */
#pragma once

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

}  // namespace apps
