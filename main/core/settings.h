/*
 * Settings — a thin typed wrapper over an NVS namespace ("settings").
 * Values are cached in RAM; set*() writes through to flash immediately.
 */
#pragma once

#include <cstdint>
#include <string>

namespace core {

class Settings {
public:
    static Settings& instance();

    bool init();  // opens NVS; safe to call once

    int  brightnessPerMille() const { return brightness_; }
    void setBrightnessPerMille(int v);

    const std::string& fontName() const { return font_name_; }
    void setFontName(const std::string& v);

    int  readFontSize() const { return read_font_size_; }
    void setReadFontSize(int px);

    int  readMarginPx() const { return read_margin_; }
    void setReadMarginPx(int px);

    int  readLineExtra() const { return read_line_extra_; }
    void setReadLineExtra(int px);

    // System (UI) font, resolved from /sd/fonts at boot. Size 0 or a family "" =
    // "script default" (the embedded NanumGothic 22 px when no SD font matches).
    int  uiFontSize() const { return ui_font_size_; }
    void setUiFontSize(int px);
    const std::string& uiFontFamily() const { return ui_font_family_; }
    void setUiFontFamily(const std::string& fam);

    // Reader body font family ("" = script default); size is readFontSize().
    // Applied on the next book open — no restart needed.
    const std::string& readFontFamily() const { return read_font_family_; }
    void setReadFontFamily(const std::string& fam);

    // Extra vertical padding in list rows. -1 = not chosen by the user (caller
    // derives a default from the language / pack); 0 or 1 = explicit. Applied
    // live, no restart.
    int  uiRowPad() const { return ui_row_pad_; }
    void setUiRowPad(bool on);

    int  language() const { return language_; }   // 0 = 한국어, 1 = English
    void setLanguage(int lang);

    // "" => use the built-in language() table; otherwise /sd/lang/<code>.lang.
    const std::string& langCode() const { return lang_code_; }
    void setLangCode(const std::string& code);

    // RAM-only: set true when a boot-time setting (language, system font)
    // changed this session, so the UI can offer a restart. Never persisted.
    bool restartPending() const { return restart_pending_; }

    const std::string& wifiSsid() const { return wifi_ssid_; }
    const std::string& wifiPass() const { return wifi_pass_; }
    void setWifi(const std::string& ssid, const std::string& pass);

    bool wifiAutoConnect() const { return wifi_auto_; }
    void setWifiAutoConnect(bool v);

    // Status-bar readouts: true = graphic (battery gauge icon / brightness bar),
    // false = "NN%" text. Battery defaults to the gauge, brightness to text.
    bool batteryGauge()    const { return batt_gauge_; }
    void setBatteryGauge(bool v);
    bool brightnessGauge() const { return bright_gauge_; }
    void setBrightnessGauge(bool v);

    // System gestures: pull down from the status bar, swipe up from the
    // bottom edge (ui::GestureArbiter). Read by the kernel at each touch-down.
    bool gesturesEnabled() const { return gestures_; }
    void setGesturesEnabled(bool v);

private:
    Settings() = default;
    void putInt(const char* key, int32_t v);
    void putStr(const char* key, const std::string& v);
    void commit();

    void markRestartPending() { restart_pending_ = true; }

    bool ready_ = false;
    bool restart_pending_ = false;
    int  brightness_      = 0;
    int  read_font_size_  = 22;
    int  read_margin_     = 24;
    int  read_line_extra_ = 0;
    int  ui_font_size_    = 22;
    int  ui_row_pad_      = -1;   // -1 = unset (derive from language)
    int  language_        = 0;
    bool wifi_auto_       = true;
    bool batt_gauge_      = true;    // status bar: battery as a gauge icon
    bool bright_gauge_    = false;   // status bar: brightness as text %
    bool gestures_        = true;
    std::string font_name_ = "NanumGothic";
    std::string ui_font_family_;
    std::string read_font_family_;
    std::string lang_code_;
    std::string wifi_ssid_;
    std::string wifi_pass_;
};

}  // namespace core
