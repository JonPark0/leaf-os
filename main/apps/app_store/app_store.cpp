#include "app_store.h"

#include <cstdio>

#include <esp_log.h>

#include "catalog_client.h"
#include "hal_storage.h"
#include "hal_sys.h"
#include "hal_wifi.h"
#include "i18n.h"
#include "lua/lua_bindings.h"
#include "lua/lua_registry.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr char TAG[] = "app_store";
constexpr int kMaxSdApps = 24;
constexpr int kPaintTicks = 3;                  // ticks to let "loading" paint first
constexpr int64_t kArmWindowUs = 5'000'000;     // second tap on 삭제 within 5 s

bool sdMounted() { return hal::storage::state() == hal::storage::State::Mounted; }

std::string displayName(const appstore::AppManifest& m)
{
    if (lang() == En && !m.name_en.empty()) return m.name_en;
    return m.name.empty() ? m.id : m.name;
}

std::string summary(const appstore::AppManifest& m)
{
    return (lang() == En && !m.summary_en.empty()) ? m.summary_en : m.summary;
}


}  // namespace

// ---- StoreApp ---------------------------------------------------------------

void StoreApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    title_.setFont(ctx.ui_font);
    title_.setText(tr(APP_STORE));
    title_.setBounds({130, ui::StatusBar::kHeight + 10, 220, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const int lh = ctx.ui_font ? ctx.ui_font->lineHeight() : 30;
    status_.setFont(ctx.ui_font);
    status_.setBounds({16, bodyTop(), 448, lh + 8});
    screen_.add(&status_);

    list_.setFont(ctx.ui_font);
    list_.setBounds({0, bodyTop() + lh + 16, 480, 800 - bodyTop() - lh - 16});
    list_.onSelect([this](int i) { onRow(i); });
    list_.detailFor([this](int i) -> std::string {
        if (i < 0 || i >= static_cast<int>(rows_.size())) return "";
        const Row& r = rows_[i];
        if (r.catalog < 0) return tr(STORE_NOT_LISTED);
        const auto st = appstore::status(catalog_[r.catalog], r.installed ? &r.local : nullptr,
                                         kLuaApiLevel);
        switch (st) {
            case appstore::Status::Installed:       return tr(STORE_INSTALLED);
            case appstore::Status::UpdateAvailable: return tr(STORE_UPDATE);
            case appstore::Status::NeedsFirmware:   return tr(STORE_NEEDS_FW);
            default:                                return "";
        }
    });
    screen_.add(&list_);

    requestLoad();
}

void StoreApp::onResume() { rebuildRows(); }   // back from a detail page

void StoreApp::requestLoad()
{
    if (!sdMounted()) {
        status_.setText(tr(STORE_NO_SD));
    } else if (!hal::wifi::connected()) {
        status_.setText(tr(STORE_NO_WIFI));
    } else {
        status_.setText(tr(STORE_LOADING));
        load_in_ticks_ = kPaintTicks;
    }
    rebuildRows();
}

void StoreApp::onTick()
{
    if (load_in_ticks_ > 0 && --load_in_ticks_ == 0) {
        load_in_ticks_ = -1;
        load();
    }
}

void StoreApp::load()
{
    base_ = appstore::catalogBase();
    std::string err;
    if (appstore::fetchCatalog(base_, catalog_, err)) {
        status_.setText("");
    } else {
        ESP_LOGW(TAG, "catalog: %s", err.c_str());
        status_.setText(netFailText(tr(STORE_LOAD_FAIL), err));
    }
    rebuildRows();
}

void StoreApp::rebuildRows()
{
    SdAppInfo sd[kMaxSdApps];
    const int sdn = scanSdApps(sd, kMaxSdApps);
    std::vector<bool> matched(static_cast<size_t>(sdn), false);

    rows_.clear();
    for (size_t c = 0; c < catalog_.size(); ++c) {
        Row r;
        r.catalog = static_cast<int>(c);
        for (int j = 0; j < sdn; ++j)
            if (sd[j].m.id == catalog_[c].m.id) { r.installed = true; r.local = sd[j].m; matched[j] = true; }
        rows_.push_back(std::move(r));
    }
    for (int j = 0; j < sdn; ++j) {
        if (matched[j]) continue;
        Row r;
        r.installed = true;
        r.local = sd[j].m;
        rows_.push_back(std::move(r));
    }

    std::vector<std::string> items;
    for (const Row& r : rows_)
        items.push_back(displayName(r.catalog >= 0 ? catalog_[r.catalog].m : r.local));
    items.emplace_back(tr(STORE_REFRESH));
    list_.setItems(items);
}

void StoreApp::onRow(int i)
{
    if (i == static_cast<int>(rows_.size())) { requestLoad(); return; }
    if (i < 0 || i > static_cast<int>(rows_.size()) || !ctx_.push) return;
    const Row& r = rows_[i];
    ctx_.push(new StoreDetailApp(base_, r.catalog >= 0 ? &catalog_[r.catalog] : nullptr,
                                 r.installed ? &r.local : nullptr));
}

// ---- StoreDetailApp -----------------------------------------------------------

StoreDetailApp::StoreDetailApp(std::string base, const appstore::CatalogEntry* entry,
                               const appstore::AppManifest* installed)
    : base_(std::move(base))
{
    if (entry) { has_entry_ = true; entry_ = *entry; }
    if (installed) { installed_ = true; local_ = *installed; }
}

void StoreDetailApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    const appstore::AppManifest& m = has_entry_ ? entry_.m : local_;
    title_.setFont(ctx.ui_font);
    title_.setText(displayName(m));
    title_.setBounds({120, ui::StatusBar::kHeight + 10, 240, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const int lh = ctx.ui_font ? ctx.ui_font->lineHeight() : 30;
    const int btn_y = 800 - 64 - 3 * lh - 40;
    body_.setFont(ctx.ui_font);
    body_.setGapPercent(100);
    body_.setBounds({0, bodyTop(), 480, btn_y - bodyTop() - 12});
    screen_.add(&body_);

    install_.setFont(ctx.ui_font);
    install_.setBounds({24, btn_y, 204, 64});
    install_.onClick([this] { tapInstall(); });
    screen_.add(&install_);

    delete_.setFont(ctx.ui_font);
    delete_.setLabel(tr(DELETE));
    delete_.setBounds({252, btn_y, 204, 64});
    delete_.onClick([this] { tapDelete(); });
    screen_.add(&delete_);

    hint_.setFont(ctx.ui_font);
    hint_.setBounds({16, btn_y + 64 + 16, 448, lh + 8});
    hint_.setAlign(ui::Label::Align::Center);
    screen_.add(&hint_);

    refresh();
}

void StoreDetailApp::refresh()
{
    const appstore::AppManifest* local = installed_ ? &local_ : nullptr;
    const appstore::AppManifest& m = has_entry_ ? entry_.m : local_;

    std::vector<std::string> lines;
    std::string ver = std::string(tr(ABOUT_VERSION)) + " ";
    const std::string local_ver = local ? (local->version_str.empty() ? "-" : local->version_str) : "";
    if (has_entry_ && local && local_ver != entry_.m.version_str)
        ver += local_ver + " → " + entry_.m.version_str;
    else
        ver += has_entry_ ? entry_.m.version_str : local_ver;
    lines.push_back(ver);
    if (!m.author.empty()) lines.push_back(formatArg(tr(STORE_AUTHOR), m.author));
    if (has_entry_) {
        char size[48];
        std::snprintf(size, sizeof(size), tr(STORE_SIZE),
                      static_cast<unsigned>((entry_.size + 1023) / 1024));
        lines.emplace_back(size);
    }
    if (!summary(m).empty()) { lines.emplace_back(""); lines.push_back(summary(m)); }
    lines.emplace_back("");
    if (m.net.empty()) {
        lines.emplace_back(tr(STORE_NET_NONE));
    } else {
        lines.emplace_back(tr(STORE_NET));
        const std::vector<std::string> added =
            has_entry_ && local ? appstore::newHosts(entry_, local) : std::vector<std::string>{};
        for (const std::string& h : m.net) {
            bool is_new = false;
            for (const std::string& a : added) is_new |= a == h;
            lines.push_back("  " + h + (is_new ? std::string(" ") + tr(STORE_NET_NEW) : ""));
        }
    }
    body_.setLines(std::move(lines));

    const auto st = has_entry_ ? appstore::status(entry_, local, kLuaApiLevel)
                               : appstore::Status::Installed;
    const bool can_install = has_entry_ && (st == appstore::Status::NotInstalled ||
                                            st == appstore::Status::UpdateAvailable);
    install_.setLabel(st == appstore::Status::UpdateAvailable ? tr(STORE_UPDATE) : tr(STORE_INSTALL));
    install_.setVisible(can_install);
    delete_.setVisible(installed_);
    if (has_entry_ && st == appstore::Status::NeedsFirmware) hint_.setText(tr(STORE_NEEDS_FW));
}

void StoreDetailApp::tapInstall()
{
    if (installer_.state() == appstore::Installer::State::Running || start_in_ticks_ > 0) return;
    delete_armed_us_ = 0;
    if (!sdMounted())               { hint_.setText(tr(STORE_NO_SD)); return; }
    if (!hal::wifi::connected())    { hint_.setText(tr(STORE_NO_WIFI)); return; }
    char buf[64];
    std::snprintf(buf, sizeof(buf), tr(STORE_DOWNLOADING), 0, static_cast<int>(entry_.files.size()));
    hint_.setText(buf);
    start_in_ticks_ = kPaintTicks;
}

void StoreDetailApp::tapDelete()
{
    if (installer_.state() == appstore::Installer::State::Running || start_in_ticks_ > 0) return;
    const int64_t now = hal::sys::nowUs();
    if (delete_armed_us_ == 0 || now - delete_armed_us_ > kArmWindowUs) {
        delete_armed_us_ = now;
        hint_.setText(tr(STORE_DELETE_AGAIN));
        return;
    }
    delete_armed_us_ = 0;
    if (appstore::removeApp(local_.id)) {
        installed_ = false;
        local_ = {};
        hint_.setText(tr(STORE_DELETE_OK));
        if (!has_entry_) { if (ctx_.pop) ctx_.pop(); return; }
    } else {
        hint_.setText(tr(STORE_DELETE_FAIL));
    }
    refresh();
}

void StoreDetailApp::onTick()
{
    if (delete_armed_us_ != 0 && hal::sys::nowUs() - delete_armed_us_ > kArmWindowUs) {
        delete_armed_us_ = 0;
        hint_.setText("");
    }

    using S = appstore::Installer::State;
    if (start_in_ticks_ > 0) {
        if (--start_in_ticks_ > 0) return;
        start_in_ticks_ = -1;
        if (installer_.begin(base_, entry_) != S::Running) {
            hint_.setText(netFailText(tr(STORE_INSTALL_FAIL), installer_.error()));
            return;
        }
    }
    if (installer_.state() != S::Running) return;

    const S st = installer_.step();
    if (st == S::Running) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), tr(STORE_DOWNLOADING), installer_.done(), installer_.total());
        hint_.setText(buf);
    } else if (st == S::Done) {
        installed_ = readManifest(sdAppsRoot() + "/" + entry_.m.id, local_);
        hint_.setText(tr(STORE_INSTALL_OK));
        refresh();
    } else {
        hint_.setText(netFailText(tr(STORE_INSTALL_FAIL), installer_.error()));
    }
}

void StoreDetailApp::onSystemChanged()
{
    if (!sdMounted() && installer_.state() == appstore::Installer::State::Running) {
        installer_.cancel();
        hint_.setText(tr(STORE_NO_SD));
    }
}

}  // namespace apps
