#include "app_books.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

#include <esp_log.h>

#include "app_reader/app_reader.h"
#include "hal_storage.h"
#include "i18n.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr char TAG[] = "app_books";
constexpr size_t kMaxBooks = 256;
constexpr uint64_t kMaxReadable = 12ull * 1024 * 1024;
}  // namespace

void BooksApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, tr(APP_BOOKS), TitleStyle::BesideBack);

    list_.setFont(ctx.ui_font);
    list_.setBounds(ui::metrics::body());
    list_.onSelect([this](int i) { openBook(i); });
    list_.iconFor([this](int i) -> const char* {
        return (i >= 0 && i < static_cast<int>(books_.size())) ? "book" : nullptr;
    });
    list_.detailFor([this](int i) -> std::string {
        if (i < 0 || i >= static_cast<int>(books_.size())) return "";
        const Book& b = books_[i];
        if (b.recent)    return tr(BOOK_RECENT);
        if (b.resumable) return tr(BOOK_CONTINUE);
        return b.is_epub ? "EPUB" : "TXT";
    });
    screen_.add(&list_);

    // Added after list_ so its white row-fill never paints over the message.
    empty_.setFont(ctx.ui_font);
    empty_.setBounds({0, bodyTop() + 60, ui::metrics::kScreenW, 40});
    empty_.setAlign(ui::Label::Align::Center);
    empty_.setVisible(false);
    screen_.add(&empty_);

    scan();
}

void BooksApp::onResume()
{
    scan();   // a book may have been added; refresh "이어읽기" markers
}

void BooksApp::onSystemChanged()
{
    scan();
}

void BooksApp::scan()
{
    books_.clear();

    if (hal::storage::state() != hal::storage::State::Mounted) {
        list_.setItems({});
        list_.setVisible(false);
        empty_.setText(tr(BOOK_NO_SD));
        empty_.setVisible(true);
        return;
    }

    const std::string root = hal::storage::mountPoint();     // "/sd"
    const std::string dirs[] = {root, root + "/Books"};

    for (const std::string& dir : dirs) {
        DIR* d = ::opendir(dir.c_str());
        if (!d) continue;
        struct dirent* ent;
        while ((ent = ::readdir(d)) != nullptr && books_.size() < kMaxBooks) {
            if (ent->d_name[0] == '.') continue;
            const char* dot = std::strrchr(ent->d_name, '.');
            if (!dot) continue;
            const bool epub = !strcasecmp(dot, ".epub");
            const bool txt  = !strcasecmp(dot, ".txt");
            if (!epub && !txt) continue;

            Book b;
            b.path = dir + "/" + ent->d_name;
            struct stat st;
            if (::stat(b.path.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) continue;
                b.size = static_cast<uint64_t>(st.st_size);
            }
            if (b.size == 0) continue;

            b.is_epub = epub;
            b.title.assign(ent->d_name, static_cast<size_t>(dot - ent->d_name));

            // Reader's resume sidecar is "/<dir>/.<filename>.pos".
            const std::string side = dir + "/." + ent->d_name + ".pos";
            struct stat ss;
            if (::stat(side.c_str(), &ss) == 0) {
                b.resumable = true;
                b.read_at = static_cast<long>(ss.st_mtime);
            }

            books_.push_back(std::move(b));
        }
        ::closedir(d);
    }

    std::sort(books_.begin(), books_.end(), [](const Book& a, const Book& b) {
        return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
    });

    // Pin the single most-recently-read book to the top.
    int recent = -1;
    long best = 0;
    for (size_t i = 0; i < books_.size(); ++i) {
        if (books_[i].read_at > best) { best = books_[i].read_at; recent = static_cast<int>(i); }
    }
    if (recent > 0) {
        Book r = std::move(books_[recent]);
        books_.erase(books_.begin() + recent);
        books_.insert(books_.begin(), std::move(r));
    }
    if (recent >= 0) books_.front().recent = true;

    ESP_LOGI(TAG, "%d book(s) on card%s", static_cast<int>(books_.size()),
             recent >= 0 ? ", most-recent pinned" : "");

    std::vector<std::string> rows;
    rows.reserve(books_.size());
    for (const Book& b : books_) rows.push_back(b.title);
    list_.setItems(std::move(rows));

    list_.setVisible(!books_.empty());
    empty_.setText(tr(BOOK_NONE));
    empty_.setVisible(books_.empty());
}

void BooksApp::openBook(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(books_.size())) return;
    const Book& b = books_[idx];
    ESP_LOGI(TAG, "open %s (%llu B)", b.path.c_str(), b.size);
    if (b.size > kMaxReadable) {
        ESP_LOGW(TAG, "too large to open");
        return;
    }
    if (ctx_.push) ctx_.push(new ReaderApp(b.path));
}

}  // namespace apps
