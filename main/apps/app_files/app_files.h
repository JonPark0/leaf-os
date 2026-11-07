#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/list_view.h"

namespace apps {

// SD-card file browser. Folders first, then files; tap a folder to descend,
// ".." to go up (never above the mount point). Files are inert for now — the
// reader will hook in here later. Registered only while a card is mounted
// (app_registry `sdInserted`); if the card is pulled while browsing, the app
// pops itself.
class FilesApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onSystemChanged() override;   // SD insert / removal

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }
    bool onBack() override;             // ".." if not at root, else let kernel pop

private:
    struct Entry {
        std::string name;
        bool        is_dir = false;
        uint64_t    size   = 0;
    };

    void loadDir(const std::string& path);
    void openEntry(int idx);
    void goUp();

    AppMeta meta_{"files", "파일 관리자"};
    ui::Screen   screen_;
    ui::Button   back_;
    ui::Label    path_;
    ui::Label    empty_;      // shown when the folder has no entries
    ui::ListView list_{nullptr};

    std::string          cur_;
    std::vector<Entry>   entries_;
};

}  // namespace apps
