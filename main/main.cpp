/*
 * LEAF OS (Lightweight/Lua EPD Application Framework) — entry point.
 *
 * Boot the OS shell: hal::* brings up the hardware (no M5.begin()),
 * core::Settings restores NVS state, core::Kernel runs the app stack with
 * HomeApp as root and the shared status bar as an overlay.
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_home/app_home.h"
#include "font.h"
#include "font_scan.h"
#include "i18n.h"
#include "iconset.h"
#include "lang_pack.h"
#include "hal_board.h"
#include "hal_i2c.h"
#include "hal_keys.h"
#include "hal_ota.h"
#include "hal_power.h"
#include "hal_rtc.h"
#include "hal_storage.h"
#include "hal_touch.h"
#include "hal_wifi.h"
#include "kernel.h"
#include "settings.h"
#include "ssd1677.h"
#include "widgets/list_view.h"

extern "C" const uint8_t font_ui_start[] asm("_binary_font_ui_m5f_start");
extern "C" const uint8_t font_ui_end[]   asm("_binary_font_ui_m5f_end");
extern "C" const uint8_t icons_start[]      asm("_binary_icons_m5i_start");
extern "C" const uint8_t icons_end[]        asm("_binary_icons_m5i_end");

namespace {
constexpr char TAG[] = "main";
gfx::Font g_ui_font;       // embedded fallback
gfx::Font g_ui_font_sd;    // optional per-size UI font from /sd/fonts
gfx::IconSet g_ui_icons;

[[noreturn]] void die(const char* why)
{
    ESP_LOGE(TAG, "%s", why);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

// Resolve the system (UI) font. `script` is "ko" for the built-in Korean/Latin
// font, or a language pack's "# font" value ("ja"/"sc"/"tc"). `family` is a face
// within that script ("" = script default); `px_setting` 0 = embedded size.
// File convention: /sd/fonts/<script>-<family>-<px>.m5f (see gfx/font_scan).
const gfx::Font* pickUiFont(const char* script, const char* family, int px_setting)
{
    const bool embedded_ok =
        std::strcmp(script, "ko") == 0 &&
        (family == nullptr || family[0] == '\0' || std::strcmp(family, "gothic") == 0) &&
        (px_setting == 0 || px_setting == 22);
    if (embedded_ok) return &g_ui_font;               // embedded 22 px NanumGothic

    static gfx::SdFont fonts[64];
    const int n = gfx::scanSdFonts("/sd/fonts", fonts, 64);
    const int want = px_setting ? px_setting : 22;
    const gfx::SdFont* hit = gfx::pickSdFont(fonts, n, script, family, want);
    if (hit && g_ui_font_sd.loadFromFile(hit->path.c_str())) {
        ESP_LOGI(TAG, "UI font: %s (%dpx)", hit->path.c_str(), g_ui_font_sd.pxSize());
        return &g_ui_font_sd;
    }
    ESP_LOGW(TAG, "no SD UI font for '%s/%s' — embedded (non-Latin text will be blank)",
             script, family ? family : "");
    return &g_ui_font;
}

// Headless Wi-Fi provisioning: /sd/.wifi.txt (or the older /sd/wifi.txt),
// SSID on line 1, password on line 2. Used only when no credentials are stored
// yet (before the on-screen keyboard exists). Copies into NVS, deletes nothing.
void provisionWifiFromSd()
{
    if (hal::storage::mount() != ESP_OK) return;
    FILE* f = std::fopen("/sd/.wifi.txt", "r");
    if (!f) f = std::fopen("/sd/wifi.txt", "r");
    if (!f) return;
    char ssid[64] = "", pass[128] = "";
    if (std::fgets(ssid, sizeof(ssid), f)) {
        std::fgets(pass, sizeof(pass), f);
    }
    std::fclose(f);
    for (char* p : {ssid, pass}) {
        p[strcspn(p, "\r\n")] = '\0';
    }
    if (ssid[0]) {
        core::Settings::instance().setWifi(ssid, pass);
        ESP_LOGI(TAG, "Wi-Fi provisioned from /sd/wifi.txt (ssid='%s')", ssid);
    }
}
}  // namespace

extern "C" void app_main()
{
    ESP_LOGI(TAG, "LEAF OS booting");

    if (!g_ui_font.load(font_ui_start,
                        static_cast<size_t>(font_ui_end - font_ui_start))) {
        die("embedded UI font failed to load");
    }
    if (g_ui_icons.load(icons_start,
                        static_cast<size_t>(icons_end - icons_start))) {
        gfx::setUiIcons(&g_ui_icons);
        ESP_LOGI(TAG, "icons: %d @ %dpx", g_ui_icons.count(), g_ui_icons.cellPx());
    } else {
        ESP_LOGW(TAG, "embedded icon sheet failed to load");
    }

    if (hal::i2c::init() != ESP_OK)   die("i2c init failed");
    if (hal::board::init() != ESP_OK) die("board init failed");
    hal::power::init();
    hal::rtc::init();
    // Seed the C library clock from the RTC so filesystem timestamps (e.g. the
    // reader's ".pos" sidecars, used to pin the most-recent book) are ordered
    // sensibly across reboots. The RTC holds local wall-clock; we feed it as-is
    // (the whole system treats time as local — see hal_wifi kTzOffsetSec).
    if (!hal::rtc::timeInvalid()) {
        hal::rtc::DateTime dt{};
        if (hal::rtc::get(dt)) {
            struct timeval tv{};
            tv.tv_sec = hal::rtc::toUnix(dt);
            settimeofday(&tv, nullptr);
        }
    }
    if (hal::keys::init() != ESP_OK)  die("keys init failed");
    if (hal::touch::init() != ESP_OK) ESP_LOGW(TAG, "touch unavailable");
    if (hal::storage::init() != ESP_OK) ESP_LOGW(TAG, "SD detect unavailable");
    hal::storage::mount();   // best-effort; needed for SD fonts + wifi provisioning

    auto& st = core::Settings::instance();
    if (!st.init()) ESP_LOGW(TAG, "settings NVS unavailable — using defaults");
    hal::power::setBrightness(st.brightnessPerMille());

    // Language: an SD /sd/lang/<code>.lang pack when one is chosen (English is
    // then the fall-back for any keys it omits), otherwise the built-in ko/en.
    const char* font_tag = "ko";
    if (!st.langCode().empty() && core::loadLangPack(st.langCode().c_str())) {
        core::i18n::setLang(core::i18n::En);
        if (core::langPackFont()[0] != '\0') font_tag = core::langPackFont();
    } else {
        core::i18n::setLang(st.language() == 1 ? core::i18n::En : core::i18n::Ko);
    }

    const gfx::Font* ui_font =
        pickUiFont(font_tag, st.uiFontFamily().c_str(), st.uiFontSize());

    // Extra list-row padding: the user's explicit choice, else the pack's
    // "# ui_pad", else on for the built-in ko/en (whose font is short) and off
    // for CJK packs (whose fonts are already tall).
    int row_pad = st.uiRowPad();
    if (row_pad < 0) row_pad = (core::langPackUiPad() >= 0)
                                   ? core::langPackUiPad()
                                   : (st.langCode().empty() ? 1 : 0);
    ui::ListView::setGlobalRowPadding(row_pad ? 12 : 0);

    if (st.wifiSsid().empty()) provisionWifiFromSd();

    if (hal::wifi::init() != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi unavailable");
    } else if (st.wifiAutoConnect() && !st.wifiSsid().empty()) {
        ESP_LOGI(TAG, "Wi-Fi auto-connect to '%s'", st.wifiSsid().c_str());
        hal::wifi::connectSaved();
    }

    ESP_LOGI(TAG, "battery %d mV (%d%%), RTC %s",
             hal::power::batteryMilliVolts(), hal::power::batteryPercent(),
             hal::rtc::timeInvalid() ? "unset" : "ok");

    const ssd1677::HwHooks hooks{hal::board::epdPower, hal::board::epdResetPulse};
    auto& kernel = core::Kernel::instance();
    if (!kernel.init(hooks)) die("kernel init failed");
    kernel.setUiFont(ui_font);

    static apps::HomeApp home;
    kernel.setRootApp(&home);

    // A freshly updated image is on probation until it has run this long; a
    // crash or reset before then boots the previous one (docs/design/ota-update.md).
    hal::ota::confirmAfter(10000);

    ESP_LOGI(TAG, "home ready");
    kernel.run();  // never returns
}
