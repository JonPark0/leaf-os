#include "app_home.h"

#include <cstdio>

#include <esp_log.h>

#include "app_registry.h"
#include "i18n.h"
#include "lua/lua_app.h"
#include "lua/lua_registry.h"

namespace apps {
namespace {
constexpr char TAG[] = "app_home";
constexpr int kCols = 3;
constexpr int kTop  = 34 + 20;   // below the status bar
constexpr int kMargin = 20, kGapX = 12, kGapY = 24, kCellH = 108;
constexpr int kMaxSdApps = 24;
}  // namespace

void HomeApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);
    rebuild();
}

void HomeApp::rebuild()
{
    const AppContext& ctx = ctx_;
    screen_.clearWidgets();

    int n = 0;
    const BuiltinApp* builtins = builtinApps(n);
    std::vector<const BuiltinApp*> shown;
    for (int i = 0; i < n; ++i)
        if (builtins[i].visible == nullptr || builtins[i].visible()) shown.push_back(&builtins[i]);

    SdAppInfo sd[kMaxSdApps];
    const int sdn = scanSdApps(sd, kMaxSdApps);

    tiles_.clear();
    tiles_.resize(shown.size() + static_cast<size_t>(sdn));   // one realloc
    sd_icons_.clear();
    sd_icons_.resize(static_cast<size_t>(sdn));

    const int cw = (480 - 2 * kMargin - (kCols - 1) * kGapX) / kCols;
    auto place = [&](int idx, int slot) {
        const int r = slot / kCols, c = slot % kCols;
        ui::AppTile& t = tiles_[idx];
        t.setBounds({kMargin + c * (cw + kGapX), kTop + r * (kCellH + kGapY), cw, kCellH});
        screen_.add(&t);
    };

    for (size_t i = 0; i < shown.size(); ++i) {
        ui::AppTile& t = tiles_[i];
        t.setFont(ctx.ui_font);
        t.setLabel(core::i18n::tr(shown[i]->name));
        t.setIcon(shown[i]->icon);
        App* (*mk)() = shown[i]->make;
        const char* id = shown[i]->id;
        AppContext c2 = ctx;
        t.onClick([mk, id, c2] {
            ESP_LOGI(TAG, "launch %s", id);
            if (c2.push && mk) c2.push(mk());
        });
        place(static_cast<int>(i), static_cast<int>(i));
    }

    if (sdn <= 0) return;

    // Divider + SD-app section on a fresh row below the builtins.
    const int builtin_rows = (static_cast<int>(shown.size()) + kCols - 1) / kCols;
    const int sep_y = kTop + builtin_rows * (kCellH + kGapY) - kGapY / 2 - 1;
    divider_.setBackground(core::kBlack);
    divider_.setText("");
    divider_.setBounds({kMargin, sep_y, 480 - 2 * kMargin, 2});
    screen_.add(&divider_);

    for (int j = 0; j < sdn; ++j) {
        const size_t idx = shown.size() + static_cast<size_t>(j);
        ui::AppTile& t = tiles_[idx];
        t.setFont(ctx.ui_font);
        const bool en = core::i18n::lang() == core::i18n::En && !sd[j].name_en.empty();
        t.setLabel((en ? sd[j].name_en : sd[j].name).c_str());
        t.setIcon(sd[j].icon.empty() ? "note" : sd[j].icon.c_str());

        // Per-app icon.m5i (so the tile can show a custom glyph).
        if (FILE* f = std::fopen((sd[j].dir + "/icon.m5i").c_str(), "rb")) {
            std::fseek(f, 0, SEEK_END);
            long sz = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 64 * 1024) {
                auto ico = std::make_unique<SdIcon>();
                ico->blob.resize(static_cast<size_t>(sz));
                if (std::fread(ico->blob.data(), 1, ico->blob.size(), f) == ico->blob.size() &&
                    ico->set.load(ico->blob.data(), ico->blob.size())) {
                    t.setIconSet(&ico->set);
                    sd_icons_[j] = std::move(ico);
                }
            }
            std::fclose(f);
        }
        std::string dir = sd[j].dir, entry = sd[j].entry, title = sd[j].name;
        AppContext c2 = ctx;
        t.onClick([c2, dir, entry, title] {
            ESP_LOGI(TAG, "launch SD app %s", dir.c_str());
            if (c2.push) c2.push(new LuaApp(dir, entry, title));
        });
        place(static_cast<int>(idx), builtin_rows * kCols + j);
    }
}

}  // namespace apps
