#pragma once

#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "hal_wifi.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/keyboard.h"
#include "widgets/label.h"
#include "widgets/list_view.h"
#include "widgets/text_field.h"

namespace apps {

// Wi-Fi setup: scan -> pick an AP -> (password via on-screen keyboard) -> join.
// A successful join is written to core::Settings and upserted into
// /sd/.wifi/known.txt (dot-prefixed, so the file manager hides it) when a card
// is mounted. Known networks are shown in the list; tapping one pre-fills its
// saved password, and the password screen offers a "삭제" (forget) button.
class WifiApp : public App {
public:
    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onPause() override;
    void onTick() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    struct KnownNet { std::string ssid; std::string pass; };
    struct NetRow {
        std::string ssid;
        std::string pass;      // saved password when known, else empty
        int8_t rssi = 0;
        bool secured = false;
        bool known = false;
        bool in_range = false;
    };

    void showList();
    void showPassword();
    void setRadioEnabled(bool on);   // user toggle: persist + connect/disconnect
    void startScan();
    void collectScan();
    void rebuildRows();     // merge last scan + known_ into rows_ and repaint
    void joinSelected(const std::string& pass);
    void refreshStatusLine();

    void loadKnown();                                   // read + de-dup known.txt
    void saveKnown();                                   // rewrite known.txt clean
    void rememberKnown(const std::string& ssid, const std::string& pass);
    void forgetKnown(const std::string& ssid);
    const std::string* knownPass(const std::string& ssid) const;

    AppMeta meta_{"wifi", "Wi-Fi 설정"};
    ui::Screen    screen_;
    ui::Button    back_;
    ui::Button    rescan_;
    ui::Label     title_;
    ui::Label     wifi_label_;
    ui::Checkbox  wifi_on_;
    ui::Label     status_;
    ui::ListView  list_{nullptr};

    ui::Label     pw_prompt_;
    ui::TextField pw_field_;
    ui::Keyboard  kb_;
    ui::Button    pw_cancel_;
    ui::Button    pw_connect_;
    ui::Button    pw_forget_;
    ui::Button    pw_reveal_;
    bool          pw_visible_ = false;

    hal::wifi::ScanResult results_[24];
    std::vector<NetRow>   rows_;
    std::vector<KnownNet> known_;
    int          n_results_ = 0;
    std::string  sel_ssid_;
    std::string  sel_pass_;       // pre-fill for the password field
    bool         sel_known_ = false;
    bool         in_password_ = false;
    bool         scan_pending_ = false;
    hal::wifi::Status last_status_ = hal::wifi::Status::Off;
};

}  // namespace apps
