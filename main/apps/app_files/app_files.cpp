#include "app_files.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
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
constexpr char TAG[] = "app_files";
constexpr size_t kMaxEntries = 512;

void formatSize(uint64_t b, char* out, size_t n)
{
    if (b < 1024) std::snprintf(out, n, "%llu B", b);
    else if (b < 1024 * 1024) std::snprintf(out, n, "%.1f KB", b / 1024.0);
    else if (b < 1024ull * 1024 * 1024) std::snprintf(out, n, "%.1f MB", b / (1024.0 * 1024));
    else std::snprintf(out, n, "%.2f GB", b / (1024.0 * 1024 * 1024));
}

const char* iconForName(const char* name)
{
    const char* dot = std::strrchr(name, '.');
    if (!dot) return "note";
    char ext[8] = {};
    for (int i = 0; i < 7 && dot[i + 1]; ++i) ext[i] = static_cast<char>(std::tolower(dot[i + 1]));
    if (!std::strcmp(ext, "txt") || !std::strcmp(ext, "epub")) return "book";
    if (!std::strcmp(ext, "png") || !std::strcmp(ext, "jpg") ||
        !std::strcmp(ext, "jpeg") || !std::strcmp(ext, "bmp") || !std::strcmp(ext, "gif"))
        return "image";
    return "note";
}
}  // namespace

void FilesApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, "", TitleStyle::BesideBack);   // the title shows the folder path

    // Back goes up a folder first, and only leaves the app at the root.
    back_.onClick([this] {
        if (cur_ != hal::storage::mountPoint()) goUp();
        else if (ctx_.pop) ctx_.pop();
    });

    list_.setFont(ctx.ui_font);
    list_.setBounds(ui::metrics::body());
    list_.onSelect([this](int i) { openEntry(i); });
    list_.iconFor([this](int i) -> const char* {
        if (i < 0 || i >= static_cast<int>(entries_.size())) return nullptr;
        const Entry& e = entries_[i];
        if (e.name == "..") return nullptr;
        return e.is_dir ? "folder" : iconForName(e.name.c_str());
    });
    screen_.add(&list_);

    // Added after list_ so its white row-fill never paints over the message.
    empty_.setFont(ctx.ui_font);
    empty_.setText(tr(FILE_EMPTY));
    empty_.setBounds({0, bodyTop() + 60, ui::metrics::kScreenW, 40});
    empty_.setAlign(ui::Label::Align::Center);
    empty_.setVisible(false);
    screen_.add(&empty_);

    cur_ = hal::storage::mountPoint();   // "/sd"
    loadDir(cur_);
}

void FilesApp::onResume()
{
    if (hal::storage::state() != hal::storage::State::Mounted) {
        if (ctx_.pop) ctx_.pop();
        return;
    }
    loadDir(cur_);
}

void FilesApp::onSystemChanged()
{
    if (hal::storage::state() != hal::storage::State::Mounted) {
        ESP_LOGI(TAG, "card gone — leaving file manager");
        if (ctx_.pop) ctx_.pop();
    }
}

bool FilesApp::onBack()
{
    if (cur_ != hal::storage::mountPoint()) { goUp(); return true; }
    return false;   // at root — let the kernel pop us
}

void FilesApp::goUp()
{
    const std::string root = hal::storage::mountPoint();
    if (cur_ == root) return;
    const size_t slash = cur_.find_last_of('/');
    cur_ = (slash <= root.size()) ? root : cur_.substr(0, slash);
    loadDir(cur_);
}

void FilesApp::loadDir(const std::string& path)
{
    entries_.clear();

    DIR* d = ::opendir(path.c_str());
    if (!d) {
        ESP_LOGW(TAG, "opendir(%s) failed", path.c_str());
        title_.setText(path.c_str());
        list_.setItems({});
        list_.setVisible(false);
        empty_.setText(tr(FILE_UNREADABLE));
        empty_.setVisible(true);
        return;
    }

    struct dirent* ent;
    while ((ent = ::readdir(d)) != nullptr && entries_.size() < kMaxEntries) {
        if (!std::strcmp(ent->d_name, ".") || !std::strcmp(ent->d_name, "..")) continue;
        if (ent->d_name[0] == '.') continue;   // hidden / system

        Entry e;
        e.name = ent->d_name;
        std::string full = path + "/" + e.name;
        struct stat st;
        if (::stat(full.c_str(), &st) == 0) {
            e.is_dir = S_ISDIR(st.st_mode);
            e.size = static_cast<uint64_t>(st.st_size);
        } else {
            e.is_dir = (ent->d_type == DT_DIR);
        }
        entries_.push_back(std::move(e));
    }
    ::closedir(d);

    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir;          // folders first
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    if (path != hal::storage::mountPoint()) {
        entries_.insert(entries_.begin(), Entry{"..", true, 0});
    }

    std::vector<std::string> rows;
    rows.reserve(entries_.size());
    for (const Entry& e : entries_) {
        if (e.name == "..") { rows.emplace_back(tr(FILE_PARENT)); continue; }
        if (e.is_dir) { rows.emplace_back(e.name + "/"); continue; }
        char sz[24];
        formatSize(e.size, sz, sizeof(sz));
        rows.emplace_back(e.name + "    " + sz);
    }

    title_.setText(path.c_str());
    list_.setItems(std::move(rows));
    list_.setVisible(!entries_.empty());
    empty_.setText(tr(FILE_EMPTY));
    empty_.setVisible(entries_.empty());
}

void FilesApp::openEntry(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(entries_.size())) return;
    const Entry& e = entries_[idx];

    if (e.name == "..") { goUp(); return; }
    if (e.is_dir) {
        cur_ = (cur_ == "/") ? "/" + e.name : cur_ + "/" + e.name;
        loadDir(cur_);
        return;
    }

    const char* dot = std::strrchr(e.name.c_str(), '.');
    const bool readable = dot && (!strcasecmp(dot, ".txt") || !strcasecmp(dot, ".epub"));
    const std::string full = cur_ + "/" + e.name;
    ESP_LOGI(TAG, "file selected: %s (%llu B)", full.c_str(), e.size);

    if (readable && e.size > 0 && e.size <= 12 * 1024 * 1024) {
        if (ctx_.push) ctx_.push(new ReaderApp(full));
    }
    // Images / other formats get their handlers in later phases.
}

}  // namespace apps
