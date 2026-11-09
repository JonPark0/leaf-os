#include "app_settings.h"

#include <cstdio>

#include <esp_log.h>

#include "app_about/app_about.h"
#include "app_display/app_display.h"
#include "app_language/app_language.h"
#include "app_power/app_power.h"
#include "app_sysinfo/app_sysinfo.h"
#include "app_time/app_time.h"
#include "app_update/app_update.h"
#include "app_wifi/app_wifi.h"
#include "i18n.h"
#include "settings.h"

namespace apps {
namespace {
using namespace core::i18n;
[[maybe_unused]] constexpr char TAG[] = "app_settings";
}  // namespace

void SettingsApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, tr(APP_SETTINGS));

    list_.setFont(ctx.ui_font);
    list_.setBounds(ui::metrics::body());
    list_.onSelect([this](int i) { onRow(i); });
    screen_.add(&list_);

    rebuildRows();
}

void SettingsApp::onResume() { rebuildRows(); }

void SettingsApp::rebuildRows()
{
    auto& st = core::Settings::instance();
    char wifi[64];
    const char* wifi_val = !st.wifiAutoConnect() ? tr(SET_WIFI_OFF)
                         : st.wifiSsid().empty() ? tr(SET_WIFI_UNSET)
                                                 : st.wifiSsid().c_str();
    std::snprintf(wifi, sizeof(wifi), tr(SET_WIFI_FMT), wifi_val);

    std::vector<std::string> items;
    row_kind_.clear();
    if (st.restartPending()) { items.emplace_back(tr(SET_RESTART_APPLY)); row_kind_.push_back(kRestart); }
    items.emplace_back(tr(SET_DISPLAY));   row_kind_.push_back(kDisplay);
    items.emplace_back(wifi);              row_kind_.push_back(kWifi);
    items.emplace_back(tr(SET_TIME));      row_kind_.push_back(kTime);
    items.emplace_back(tr(SET_LANGUAGE));  row_kind_.push_back(kLanguage);
    items.emplace_back(tr(SET_SYSINFO));   row_kind_.push_back(kSysInfo);
    items.emplace_back(tr(SET_POWER));     row_kind_.push_back(kPower);
    items.emplace_back(tr(SET_UPDATE));    row_kind_.push_back(kUpdate);
    items.emplace_back(tr(SET_ABOUT));     row_kind_.push_back(kAbout);
    list_.setItems(items);
}

void SettingsApp::onRow(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(row_kind_.size()) || !ctx_.push) return;
    switch (row_kind_[idx]) {
        case kRestart:  ctx_.push(new PowerApp());    return;
        case kDisplay:  ctx_.push(new DisplayApp());  return;
        case kWifi:     ctx_.push(new WifiApp());     return;
        case kTime:     ctx_.push(new TimeApp());     return;
        case kLanguage: ctx_.push(new LanguageApp()); return;
        case kSysInfo:  ctx_.push(new SysInfoApp());  return;
        case kPower:    ctx_.push(new PowerApp());    return;
        case kUpdate:   ctx_.push(new UpdateApp());   return;
        case kAbout:    ctx_.push(new AboutApp());    return;
    }
}

}  // namespace apps
