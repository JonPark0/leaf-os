/*
 * 앱 / Apps: browse the app catalog, install, update and delete SD apps.
 * See docs/design/app-catalog.md.
 *
 * StoreApp lists catalog apps plus installed apps the catalog does not have.
 * StoreDetailApp shows one app and runs its install / update / delete.
 * Network work runs from onTick so the screen paints its "loading" text first.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "catalog.h"
#include "installer.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/list_view.h"
#include "widgets/text_block.h"

namespace apps {

class StoreApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onTick() override;
    void onSystemChanged() override { rebuildRows(); }

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    struct Row {
        int catalog = -1;                 // index into catalog_, or -1
        bool installed = false;
        appstore::AppManifest local;      // valid when installed
    };

    void requestLoad();
    void load();
    void rebuildRows();
    void onRow(int i);

    AppMeta meta_{"store", "앱"};
    ui::Screen   screen_;
    ui::Button   back_;
    ui::Label    title_;
    ui::Label    status_;
    ui::ListView list_;

    std::string base_;
    std::vector<appstore::CatalogEntry> catalog_;
    std::vector<Row> rows_;   // parallel to the list, minus the refresh row
    int load_in_ticks_ = -1;  // >0: load after this many ticks (lets the text paint)
};

class StoreDetailApp : public App {
public:
    // `entry` null: an installed app the catalog does not list (delete only).
    StoreDetailApp(std::string base, const appstore::CatalogEntry* entry,
                   const appstore::AppManifest* installed);

    void onCreate(const AppContext& ctx) override;
    void onTick() override;
    void onDestroy() override { installer_.cancel(); }
    void onSystemChanged() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    void refresh();         // texts and buttons from the current state
    void tapInstall();
    void tapDelete();

    AppMeta meta_{"store_detail", ""};
    ui::Screen screen_;
    ui::Button back_;
    ui::Label  title_;
    ui::TextBlock body_;
    ui::Button install_;
    ui::Button delete_;
    ui::Label  hint_;

    std::string base_;
    bool has_entry_ = false;
    appstore::CatalogEntry entry_;
    bool installed_ = false;
    appstore::AppManifest local_;
    appstore::Installer installer_;
    int start_in_ticks_ = -1;
    int64_t delete_armed_us_ = 0;
};

}  // namespace apps
