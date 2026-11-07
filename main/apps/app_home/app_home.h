#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "app.h"
#include "iconset.h"
#include "widgets/app_tile.h"
#include "widgets/label.h"

namespace apps {

// The root app: a grid of launchers built from the builtin registry.
class HomeApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override { rebuild(); }         // returning from a sub-app
    void onSystemChanged() override { rebuild(); }  // SD insert/removal

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }
    bool onBack() override { return true; }  // nothing to go back to

private:
    void rebuild();  // (re)build the launcher grid from the visible registry

    AppMeta meta_{"home", "홈"};
    ui::Screen screen_;
    std::vector<ui::AppTile> tiles_;   // builtin tiles, then SD-app tiles
    ui::Label divider_;                // rule above the SD-app section

    // Per-SD-app icon sheets (icon.m5i), parallel to the SD tiles; own the blob.
    struct SdIcon { std::vector<uint8_t> blob; gfx::IconSet set; };
    std::vector<std::unique_ptr<SdIcon>> sd_icons_;
};

}  // namespace apps
