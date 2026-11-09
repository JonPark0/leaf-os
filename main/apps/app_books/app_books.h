#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "page_app.h"
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
class BooksApp : public PageApp {
public:
    BooksApp() : PageApp("reader") {}
    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onSystemChanged() override;

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

    ui::Label    empty_;
    ui::ListView list_{nullptr};

    std::vector<Book> books_;
};

}  // namespace apps
