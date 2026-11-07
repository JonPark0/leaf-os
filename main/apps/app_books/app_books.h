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

// Bookshelf — the home "책" tile. Lists .txt / .epub files found directly in
// /sd and /sd/Books, newest-looking first is not attempted; plain case-
// insensitive name order. Tapping one opens ReaderApp, which restores the
// reading position from its dot-prefixed ".<name>.pos" sidecar; a book that has
// such a sidecar is marked "이어읽기" here.
//
// Cover-thumbnail grid (plan step 20) needs an image decoder and is deferred;
// this is the list-mode v1.
class BooksApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onSystemChanged() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    struct Book {
        std::string path;      // absolute, e.g. /sd/Books/foo.epub
        std::string title;     // file name without extension
        bool        is_epub = false;
        bool        resumable = false;
        bool        recent = false;   // most-recently-read -> pinned to the top
        long        read_at = 0;      // .pos sidecar mtime, 0 if never opened
        uint64_t    size = 0;
    };

    void scan();
    void openBook(int idx);

    AppMeta meta_{"reader", "책"};
    ui::Screen   screen_;
    ui::Button   back_;
    ui::Label    title_;
    ui::Label    empty_;
    ui::ListView list_{nullptr};

    std::vector<Book> books_;
};

}  // namespace apps
