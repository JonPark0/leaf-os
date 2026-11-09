#include "app_wifi.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

#include <esp_log.h>

#include "display_manager.h"
#include "hal_storage.h"
#include "i18n.h"
#include "settings.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr char TAG[] = "app_wifi";

constexpr char kKnownDir[]     = "/sd/.wifi";
constexpr char kKnownPath[]    = "/sd/.wifi/known.txt";
constexpr char kKnownPathOld[] = "/sd/wifi/known.txt";   // pre-dot location

const char* statusText(hal::wifi::Status s)
{
    switch (s) {
        case hal::wifi::Status::Connecting: return tr(WIFI_CONNECTING);
        case hal::wifi::Status::Connected:  return tr(WIFI_CONNECTED);
        case hal::wifi::Status::Failed:     return tr(WIFI_FAILED);
        default:                            return tr(WIFI_OFF);
    }
}
}  // namespace

void WifiApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, tr(WIFI_TITLE));   // showList() rebuilds the widget set

    // Wi-Fi on/off row — its own line, well clear of "다시 검색". The checkbox
    // bounds must NOT overlap the label (Checkbox::draw fills its whole rect
    // white, which would erase the title text drawn under it).
    wifi_label_.setFont(ctx.ui_font);
    wifi_label_.setText(tr(WIFI_ENABLE));
    wifi_label_.setAlign(ui::Label::Align::Left);
    wifi_label_.setBounds({16, ui::StatusBar::kHeight + 50, 280, 42});

    wifi_on_.setFont(ctx.ui_font);
    wifi_on_.setBounds({300, ui::StatusBar::kHeight + 50, 164, 42});
    wifi_on_.setChecked(core::Settings::instance().wifiAutoConnect());
    wifi_on_.onToggle([this](bool on) { setRadioEnabled(on); });

    status_.setFont(ctx.ui_font);
    status_.setBounds({16, ui::StatusBar::kHeight + 106, 448, 28});
    status_.setAlign(ui::Label::Align::Left);

    rescan_.setFont(ctx.ui_font);
    rescan_.setLabel(tr(WIFI_RESCAN));
    rescan_.setBounds({300, ui::StatusBar::kHeight + 144, 164, 42});
    rescan_.onClick([this] { startScan(); });

    list_.setFont(ctx.ui_font);
    list_.setBounds({0, bodyTop() + 138, ui::metrics::kScreenW, ui::metrics::kScreenH - (bodyTop() + 138)});
    list_.onSelect([this](int i) {
        if (i < 0 || i >= static_cast<int>(rows_.size())) return;
        const NetRow& r = rows_[i];
        sel_ssid_  = r.ssid;
        sel_known_ = r.known;
        sel_pass_  = r.pass;
        if (r.secured || r.known) {
            showPassword();          // pre-filled when known
        } else {
            joinSelected("");
        }
    });
    list_.iconFor([this](int i) -> const char* {
        if (i < 0 || i >= static_cast<int>(rows_.size())) return nullptr;
        return rows_[i].secured ? "lock" : nullptr;
    });
    list_.detailFor([this](int i) -> std::string {
        if (i < 0 || i >= static_cast<int>(rows_.size())) return "";
        const NetRow& r = rows_[i];
        char b[48];
        if (r.in_range && r.known)
            std::snprintf(b, sizeof(b), tr(WIFI_SAVED_DBM_FMT), r.rssi);
        else if (r.in_range)
            std::snprintf(b, sizeof(b), "%ddBm", r.rssi);
        else
            std::snprintf(b, sizeof(b), "%s", tr(WIFI_SAVED));
        return b;
    });

    pw_prompt_.setFont(ctx.ui_font);
    pw_prompt_.setAlign(ui::Label::Align::Left);
    pw_prompt_.setBounds({20, ui::StatusBar::kHeight + 56, 440, 30});

    pw_field_.setFont(ctx.ui_font);
    pw_field_.setPassword(true);
    pw_field_.setPlaceholder(tr(WIFI_PASSWORD));
    pw_field_.setBounds({20, ui::StatusBar::kHeight + 96, 336, 56});
    pw_field_.setFocused(true);

    pw_reveal_.setFont(ctx.ui_font);
    pw_reveal_.setLabel(tr(SHOW));
    pw_reveal_.setBounds({364, ui::StatusBar::kHeight + 96, 96, 56});
    pw_reveal_.onClick([this] {
        pw_visible_ = !pw_visible_;
        pw_field_.setPassword(!pw_visible_);
        pw_reveal_.setLabel(pw_visible_ ? tr(HIDE) : tr(SHOW));
    });

    pw_cancel_.setFont(ctx.ui_font);
    pw_cancel_.setLabel(tr(CANCEL));
    pw_cancel_.setBounds({20, ui::StatusBar::kHeight + 164, 96, 40});
    pw_cancel_.onClick([this] { showList(); });

    pw_connect_.setFont(ctx.ui_font);
    pw_connect_.setLabel(tr(CONNECT));
    pw_connect_.setBounds({124, ui::StatusBar::kHeight + 164, 96, 40});
    pw_connect_.onClick([this] { joinSelected(pw_field_.text()); });

    pw_forget_.setFont(ctx.ui_font);
    pw_forget_.setLabel(tr(DELETE));
    pw_forget_.setBounds({228, ui::StatusBar::kHeight + 164, 96, 40});
    pw_forget_.onClick([this] {
        forgetKnown(sel_ssid_);
        rebuildRows();   // drop the removed entry from the list
        showList();
    });

    kb_.setFont(ctx.ui_font);
    kb_.setBounds({0, ui::metrics::kScreenH - 300, ui::metrics::kScreenW, 300});
    kb_.onChar([this](uint32_t cp) { pw_field_.insertUtf8(cp); });
    kb_.onBackspace([this] { pw_field_.backspace(); });
    kb_.onEnter([this] { joinSelected(pw_field_.text()); });

    loadKnown();
    showList();
    if (wifi_on_.checked()) startScan();
    else                    rebuildRows();   // show saved networks only
}

// Persist the choice (it also gates the boot auto-connect) and act on the
// radio now: an "off" that left the station associated is exactly the
// complaint this toggle exists to fix.
void setWifiEnabled(bool on)
{
    core::Settings::instance().setWifiAutoConnect(on);
    if (on) {
        hal::wifi::connectSaved();   // a scan now would fail: "STA is connecting"
    } else {
        hal::wifi::disconnect();
    }
}

// User flipped the Wi-Fi checkbox on this page.
void WifiApp::setRadioEnabled(bool on)
{
    setWifiEnabled(on);
    if (on) rebuildRows();   // show saved networks; user taps 다시 검색
    refreshStatusLine();
}

void WifiApp::onResume()
{
    // Quick settings can switch Wi-Fi while this page is covered; show the
    // radio as it is now, as if the box had been toggled here.
    const bool on = core::Settings::instance().wifiAutoConnect();
    const bool changed = on != wifi_on_.checked();
    wifi_on_.setChecked(on);
    if (in_password_) {
        core::DisplayManager::instance().holdFullRefresh(true);
    } else {
        if (changed && on) rebuildRows();   // as setRadioEnabled does
        refreshStatusLine();
    }
}

void WifiApp::onPause()
{
    // Never leave the DC-balance full refresh suppressed once we're not the
    // foreground app.
    core::DisplayManager::instance().holdFullRefresh(false);
}

void WifiApp::onTick()
{
    if (scan_pending_ && !hal::wifi::scanBusy()) {
        scan_pending_ = false;
        collectScan();
    }
    const auto s = hal::wifi::status();
    if (s != last_status_) {
        last_status_ = s;
        if (!in_password_) refreshStatusLine();
    }
}

void WifiApp::showList()
{
    in_password_ = false;
    core::DisplayManager::instance().holdFullRefresh(false);
    screen_.clearWidgets();
    addHeader();
    screen_.add(&wifi_label_);
    screen_.add(&wifi_on_);
    screen_.add(&status_);
    screen_.add(&rescan_);
    screen_.add(&list_);
    refreshStatusLine();
}

void WifiApp::showPassword()
{
    in_password_ = true;
    // No ~4 s full flash may interrupt typing on the keyboard.
    core::DisplayManager::instance().holdFullRefresh(true);
    pw_visible_ = false;
    pw_reveal_.setLabel(tr(SHOW));
    pw_field_.setPassword(true);
    if (sel_known_ && !sel_pass_.empty()) pw_field_.setText(sel_pass_);
    else                                  pw_field_.clear();
    pw_field_.setFocused(true);

    char b[64];
    std::snprintf(b, sizeof(b), tr(WIFI_PW_PROMPT_FMT), sel_ssid_.c_str());
    pw_prompt_.setText(b);

    screen_.clearWidgets();
    screen_.add(&back_);
    screen_.add(&pw_prompt_);
    screen_.add(&pw_field_);
    screen_.add(&pw_reveal_);
    screen_.add(&pw_cancel_);
    screen_.add(&pw_connect_);
    if (sel_known_) screen_.add(&pw_forget_);   // forget a saved network
    screen_.add(&kb_);
}

void WifiApp::startScan()
{
    ESP_LOGI(TAG, "scanning…");
    scan_pending_ = true;
    n_results_ = 0;
    rows_.clear();
    list_.setItems({tr(WIFI_SCANNING)});
    status_.setText(tr(WIFI_SCANNING_FULL));
    hal::wifi::scanStart();
}

void WifiApp::collectScan()
{
    n_results_ = hal::wifi::scanCollect(results_, 24);
    ESP_LOGI(TAG, "%d networks", n_results_);
    rebuildRows();
    refreshStatusLine();
}

void WifiApp::rebuildRows()
{
    rows_.clear();

    // In-range APs first (already RSSI-sorted by scanCollect).
    for (int i = 0; i < n_results_; ++i) {
        NetRow r;
        r.ssid     = results_[i].ssid;
        r.rssi     = results_[i].rssi;
        r.secured  = results_[i].secured;
        r.in_range = true;
        if (const std::string* kp = knownPass(r.ssid)) {
            r.known = true;
            r.pass  = *kp;
        }
        rows_.push_back(std::move(r));
    }
    // Then saved networks that aren't currently visible.
    for (const KnownNet& k : known_) {
        bool seen = false;
        for (const NetRow& r : rows_) if (r.ssid == k.ssid) { seen = true; break; }
        if (seen) continue;
        NetRow r;
        r.ssid    = k.ssid;
        r.pass    = k.pass;
        r.known   = true;
        r.secured = !k.pass.empty();
        rows_.push_back(std::move(r));
    }

    std::vector<std::string> disp;
    disp.reserve(rows_.size());
    for (const NetRow& r : rows_) disp.push_back(r.ssid);
    if (disp.empty()) disp.emplace_back(tr(WIFI_NO_NETWORKS));
    list_.setItems(std::move(disp));
}

void WifiApp::joinSelected(const std::string& pass)
{
    ESP_LOGI(TAG, "join '%s'", sel_ssid_.c_str());
    hal::wifi::connect(sel_ssid_.c_str(), pass.c_str());
    core::Settings::instance().setWifi(sel_ssid_, pass);
    rememberKnown(sel_ssid_, pass);
    rebuildRows();
    showList();
    status_.setText(tr(WIFI_CONNECTING));
}

const std::string* WifiApp::knownPass(const std::string& ssid) const
{
    for (const KnownNet& k : known_)
        if (k.ssid == ssid) return &k.pass;
    return nullptr;
}

void WifiApp::loadKnown()
{
    known_.clear();
    if (hal::storage::state() != hal::storage::State::Mounted) return;

    FILE* f = std::fopen(kKnownPath, "r");
    bool from_legacy = false;
    if (!f) { f = std::fopen(kKnownPathOld, "r"); from_legacy = (f != nullptr); }
    if (!f) return;

    int raw = 0;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        line[std::strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        ++raw;
        std::string ssid, pass;
        if (char* tab = std::strchr(line, '\t')) { *tab = '\0'; ssid = line; pass = tab + 1; }
        else { ssid = line; }
        if (ssid.empty()) continue;
        bool dup = false;
        for (KnownNet& k : known_) if (k.ssid == ssid) { k.pass = pass; dup = true; break; }
        if (!dup) known_.push_back({ssid, pass});
    }
    std::fclose(f);
    ESP_LOGI(TAG, "known.txt: %d entries (%d raw lines)",
             static_cast<int>(known_.size()), raw);

    // Rewrite once if we collapsed duplicates or migrated the location.
    if (from_legacy || raw != static_cast<int>(known_.size())) saveKnown();
}

void WifiApp::saveKnown()
{
    if (hal::storage::state() != hal::storage::State::Mounted) return;
    ::mkdir(kKnownDir, 0777);
    FILE* f = std::fopen(kKnownPath, "w");
    if (!f) { ESP_LOGW(TAG, "cannot write %s", kKnownPath); return; }
    for (const KnownNet& k : known_)
        std::fprintf(f, "%s\t%s\n", k.ssid.c_str(), k.pass.c_str());
    std::fclose(f);
}

void WifiApp::rememberKnown(const std::string& ssid, const std::string& pass)
{
    if (ssid.empty()) return;
    for (KnownNet& k : known_) {
        if (k.ssid == ssid) {
            if (k.pass == pass) return;   // nothing changed
            k.pass = pass;
            saveKnown();
            return;
        }
    }
    known_.push_back({ssid, pass});
    saveKnown();
}

void WifiApp::forgetKnown(const std::string& ssid)
{
    const size_t before = known_.size();
    for (auto it = known_.begin(); it != known_.end(); ++it) {
        if (it->ssid == ssid) { known_.erase(it); break; }
    }
    if (known_.size() != before) {
        ESP_LOGI(TAG, "forgot '%s'", ssid.c_str());
        saveKnown();
    }
}

void WifiApp::refreshStatusLine()
{
    if (!wifi_on_.checked()) { status_.setText(tr(WIFI_DISABLED)); return; }

    char b[96];
    char ip[16] = "";
    if (hal::wifi::connected() && hal::wifi::ipString(ip)) {
        std::snprintf(b, sizeof(b), "%s (%s)", statusText(hal::wifi::status()), ip);
    } else {
        std::snprintf(b, sizeof(b), "%s", statusText(hal::wifi::status()));
    }
    status_.setText(b);
}

}  // namespace apps
