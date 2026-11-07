#include "settings.h"

#include <algorithm>

#include <esp_log.h>

#include "hal_nvs.h"

namespace core {
namespace {
constexpr char TAG[] = "settings";
constexpr char NS[]  = "settings";
}  // namespace

Settings& Settings::instance()
{
    static Settings s;
    return s;
}

bool Settings::init()
{
    if (ready_) return true;

    if (!hal::nvs::open(NS)) return false;

    int32_t i;
    if (hal::nvs::getInt("bright", &i)) brightness_ = i;
    if (hal::nvs::getInt("rfsize", &i)) read_font_size_ = i;
    if (hal::nvs::getInt("rmargin", &i)) read_margin_ = i;
    if (hal::nvs::getInt("rlextra", &i)) read_line_extra_ = i;
    if (hal::nvs::getInt("uifsize", &i)) ui_font_size_ = i;
    if (hal::nvs::getInt("uipad", &i)) ui_row_pad_ = i;
    if (hal::nvs::getInt("lang", &i)) language_ = i;
    if (hal::nvs::getInt("wauto", &i)) wifi_auto_ = (i != 0);
    if (hal::nvs::getInt("bgauge", &i)) batt_gauge_ = (i != 0);
    if (hal::nvs::getInt("brgauge", &i)) bright_gauge_ = (i != 0);

    hal::nvs::getStr("uifam", &ui_font_family_);
    hal::nvs::getStr("rfam", &read_font_family_);
    std::string font;
    if (hal::nvs::getStr("font", &font) && !font.empty()) font_name_ = font;
    hal::nvs::getStr("langc", &lang_code_);
    hal::nvs::getStr("wssid", &wifi_ssid_);
    hal::nvs::getStr("wpass", &wifi_pass_);

    ready_ = true;
    ESP_LOGI(TAG, "loaded (bright=%d rfsize=%d ssid='%s')",
             brightness_, read_font_size_, wifi_ssid_.c_str());
    return true;
}

void Settings::putInt(const char* k, int32_t v) { hal::nvs::putInt(k, v); }
void Settings::putStr(const char* k, const std::string& v) { hal::nvs::putStr(k, v); }
void Settings::commit() { hal::nvs::commit(); }

void Settings::setBrightnessPerMille(int v)
{
    v = std::clamp(v, 0, 1000);
    if (v == brightness_) return;
    brightness_ = v;
    putInt("bright", v);
    commit();
}

void Settings::setFontName(const std::string& v)
{
    if (v == font_name_ || v.empty()) return;
    font_name_ = v;
    putStr("font", v);
    commit();
}

void Settings::setReadFontSize(int px)
{
    px = std::clamp(px, 12, 48);
    if (px == read_font_size_) return;
    read_font_size_ = px;
    putInt("rfsize", px);
    commit();
}

void Settings::setReadMarginPx(int px)
{
    px = std::clamp(px, 8, 64);
    if (px == read_margin_) return;
    read_margin_ = px;
    putInt("rmargin", px);
    commit();
}

void Settings::setReadLineExtra(int px)
{
    px = std::clamp(px, 0, 16);
    if (px == read_line_extra_) return;
    read_line_extra_ = px;
    putInt("rlextra", px);
    commit();
}

void Settings::setUiFontSize(int px)
{
    if (px != 0) px = std::clamp(px, 12, 48);
    if (px == ui_font_size_) return;
    ui_font_size_ = px;
    putInt("uifsize", px);
    commit();
    markRestartPending();
}

void Settings::setUiFontFamily(const std::string& fam)
{
    if (fam == ui_font_family_) return;
    ui_font_family_ = fam;
    putStr("uifam", fam);
    commit();
    markRestartPending();
}

void Settings::setReadFontFamily(const std::string& fam)
{
    if (fam == read_font_family_) return;
    read_font_family_ = fam;
    putStr("rfam", fam);
    commit();
}

void Settings::setUiRowPad(bool on)
{
    const int v = on ? 1 : 0;
    if (v == ui_row_pad_) return;
    ui_row_pad_ = v;
    putInt("uipad", v);
    commit();
}

void Settings::setLanguage(int lang)
{
    lang = std::clamp(lang, 0, 1);
    if (lang == language_) return;
    language_ = lang;
    putInt("lang", lang);
    commit();
    markRestartPending();
}

void Settings::setLangCode(const std::string& code)
{
    if (code == lang_code_) return;
    lang_code_ = code;
    putStr("langc", code);
    commit();
    markRestartPending();
}

void Settings::setWifi(const std::string& ssid, const std::string& pass)
{
    wifi_ssid_ = ssid;
    wifi_pass_ = pass;
    putStr("wssid", ssid);
    putStr("wpass", pass);
    commit();
}

void Settings::setWifiAutoConnect(bool v)
{
    if (v == wifi_auto_) return;
    wifi_auto_ = v;
    putInt("wauto", v ? 1 : 0);
    commit();
}

void Settings::setBatteryGauge(bool v)
{
    if (v == batt_gauge_) return;
    batt_gauge_ = v;
    putInt("bgauge", v ? 1 : 0);
    commit();
}

void Settings::setBrightnessGauge(bool v)
{
    if (v == bright_gauge_) return;
    bright_gauge_ = v;
    putInt("brgauge", v ? 1 : 0);
    commit();
}

}  // namespace core
