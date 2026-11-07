#include "settings.h"

#include <algorithm>
#include <cstring>

#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

namespace core {
namespace {
constexpr char TAG[] = "settings";
constexpr char NS[]  = "settings";
nvs_handle_t s_nvs = 0;
}  // namespace

Settings& Settings::instance()
{
    static Settings s;
    return s;
}

bool Settings::init()
{
    if (ready_) return true;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
        return false;
    }
    if (nvs_open(NS, NVS_READWRITE, &s_nvs) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return false;
    }

    int32_t i;
    if (nvs_get_i32(s_nvs, "bright", &i) == ESP_OK) brightness_ = i;
    if (nvs_get_i32(s_nvs, "rfsize", &i) == ESP_OK) read_font_size_ = i;
    if (nvs_get_i32(s_nvs, "rmargin", &i) == ESP_OK) read_margin_ = i;
    if (nvs_get_i32(s_nvs, "rlextra", &i) == ESP_OK) read_line_extra_ = i;
    if (nvs_get_i32(s_nvs, "uifsize", &i) == ESP_OK) ui_font_size_ = i;
    if (nvs_get_i32(s_nvs, "uipad", &i) == ESP_OK) ui_row_pad_ = i;
    if (nvs_get_i32(s_nvs, "lang", &i) == ESP_OK) language_ = i;
    if (nvs_get_i32(s_nvs, "wauto", &i) == ESP_OK) wifi_auto_ = (i != 0);
    if (nvs_get_i32(s_nvs, "bgauge", &i) == ESP_OK) batt_gauge_ = (i != 0);
    if (nvs_get_i32(s_nvs, "brgauge", &i) == ESP_OK) bright_gauge_ = (i != 0);

    for (const char* k : {"uifam", "rfam"}) {
        size_t sn = 0;
        if (nvs_get_str(s_nvs, k, nullptr, &sn) == ESP_OK && sn) {
            std::string b(sn, '\0'); nvs_get_str(s_nvs, k, b.data(), &sn);
            b.resize(sn ? sn - 1 : 0);
            (std::strcmp(k, "uifam") == 0 ? ui_font_family_ : read_font_family_) = b;
        }
    }

    size_t n = 0;
    if (nvs_get_str(s_nvs, "font", nullptr, &n) == ESP_OK && n) {
        std::string b(n, '\0');
        nvs_get_str(s_nvs, "font", b.data(), &n);
        b.resize(n ? n - 1 : 0);
        if (!b.empty()) font_name_ = b;
    }
    n = 0;
    if (nvs_get_str(s_nvs, "langc", nullptr, &n) == ESP_OK && n) {
        std::string b(n, '\0'); nvs_get_str(s_nvs, "langc", b.data(), &n);
        b.resize(n ? n - 1 : 0); lang_code_ = b;
    }
    n = 0;
    if (nvs_get_str(s_nvs, "wssid", nullptr, &n) == ESP_OK && n) {
        std::string b(n, '\0'); nvs_get_str(s_nvs, "wssid", b.data(), &n);
        b.resize(n ? n - 1 : 0); wifi_ssid_ = b;
    }
    n = 0;
    if (nvs_get_str(s_nvs, "wpass", nullptr, &n) == ESP_OK && n) {
        std::string b(n, '\0'); nvs_get_str(s_nvs, "wpass", b.data(), &n);
        b.resize(n ? n - 1 : 0); wifi_pass_ = b;
    }

    ready_ = true;
    ESP_LOGI(TAG, "loaded (bright=%d rfsize=%d ssid='%s')",
             brightness_, read_font_size_, wifi_ssid_.c_str());
    return true;
}

void Settings::putInt(const char* k, int32_t v) { if (s_nvs) nvs_set_i32(s_nvs, k, v); }
void Settings::putStr(const char* k, const std::string& v) { if (s_nvs) nvs_set_str(s_nvs, k, v.c_str()); }
void Settings::commit() { if (s_nvs) nvs_commit(s_nvs); }

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
