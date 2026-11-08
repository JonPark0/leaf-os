#pragma once

#include <memory>
#include <string>
#include <vector>

#include "page_app.h"
#include "app_common.h"
#include "font.h"
#include "lang_pack.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/list_view.h"

namespace apps {

// "언어 / Language" sub-page: 한국어 / English plus any /sd/lang/*.lang packs.
// Saved to core::Settings; the running UI keeps its language until the next boot
// (like the system font size) — the note says so.
class LanguageApp : public PageApp {
public:
    LanguageApp() : PageApp("language") {}
    void onCreate(const AppContext& ctx) override;

private:
    void rebuild();
    void choose(int row);
    void loadPackFonts();   // SD font per pack whose endonym the UI font can't draw

    ui::ListView list_{nullptr};
    ui::Label    note_;

    std::vector<core::LangPackInfo> packs_;   // rows 2..
    std::vector<std::unique_ptr<gfx::Font>> pack_fonts_;  // parallel to packs_, may hold nullptr
};

}  // namespace apps
