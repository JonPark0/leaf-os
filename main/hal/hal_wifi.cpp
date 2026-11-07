#include "hal_wifi.h"

#include "hal_rtc.h"
#include "settings.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <sys/time.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace hal::wifi {
namespace {

constexpr char TAG[] = "hal_wifi";

// The RX8130CE holds local wall-clock time (hal_rtc keeps no timezone). Korea
// is UTC+9 with no DST. A settings-driven timezone is a later task.
constexpr int kTzOffsetSec = 9 * 3600;

// Reconnect back-off: 1 s, 2 s, 5 s, 10 s, then give up.
constexpr int64_t kBackoffUs[] = {1'000'000, 2'000'000, 5'000'000, 10'000'000};
constexpr int kMaxRetries = sizeof(kBackoffUs) / sizeof(kBackoffUs[0]);

esp_netif_t* s_netif      = nullptr;
bool         s_ready      = false;
bool         s_want_conn  = false;   // user asked to stay connected
bool         s_sntp_inited = false;

Status   s_status        = Status::Off;
Status   s_reported      = Status::Off;
char     s_ip[16]        = "";
int      s_retry         = 0;
int64_t  s_retry_at_us   = 0;        // 0 = no pending retry
volatile int64_t s_ntp_epoch = 0;   // set by the SNTP callback, drained by poll()
bool     s_ntp_pending   = false;
uint32_t s_ntp_syncs     = 0;       // count of syncs applied to the RTC

wifi_config_t s_cfg      = {};       // desired STA config
bool     s_apply_on_down  = false;   // re-apply s_cfg + connect on next DISCONNECTED

volatile bool s_scanning  = false;   // set by scanStart, cleared by SCAN_DONE

void start_sntp()
{
    if (!s_sntp_inited) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        cfg.start = true;
        cfg.sync_cb = [](struct timeval* tv) { s_ntp_epoch = tv->tv_sec; };
        esp_netif_sntp_init(&cfg);
        s_sntp_inited = true;
    } else {
        esp_netif_sntp_start();
    }
    s_ntp_pending = true;
}

void on_wifi_event(void*, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        // Do NOT auto-connect here — connect() drives association so it can set
        // the config first without racing the driver.
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        s_scanning = false;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip[0] = '\0';
        if (s_apply_on_down) {              // connect() asked for a fresh AP
            s_apply_on_down = false;
            esp_wifi_set_config(WIFI_IF_STA, &s_cfg);
            s_status = Status::Connecting;
            esp_wifi_connect();
        } else if (s_want_conn && s_retry < kMaxRetries) {
            s_status = Status::Connecting;
            s_retry_at_us = esp_timer_get_time() + kBackoffUs[s_retry];
        } else if (s_want_conn) {
            s_status = Status::Failed;
        } else {
            s_status = Status::Off;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* ev = static_cast<ip_event_got_ip_t*>(data);
        std::snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry = 0;
        s_retry_at_us = 0;
        s_status = Status::Connected;
        start_sntp();
        ESP_LOGI(TAG, "connected, ip=%s", s_ip);
    }
}

esp_err_t ensure_started()
{
    if (s_ready) return ESP_OK;

    esp_err_t e = esp_netif_init();
    if (e != ESP_OK) return e;
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;  // settings/others may have made it

    s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t ic = WIFI_INIT_CONFIG_DEFAULT();
    e = esp_wifi_init(&ic);
    if (e != ESP_OK) return e;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, nullptr, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, nullptr, nullptr);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);  // core::Settings is our credential store
    e = esp_wifi_start();
    if (e != ESP_OK) return e;

    s_ready = true;
    return ESP_OK;
}

}  // namespace

esp_err_t init()
{
    esp_err_t e = ensure_started();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(e));
        return e;
    }
    ESP_LOGI(TAG, "station ready");
    return ESP_OK;
}

esp_err_t connect(const char* ssid, const char* pass)
{
    if (ssid == nullptr || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;
    esp_err_t e = ensure_started();
    if (e != ESP_OK) return e;

    s_cfg = {};
    std::strncpy(reinterpret_cast<char*>(s_cfg.sta.ssid), ssid, sizeof(s_cfg.sta.ssid) - 1);
    if (pass && pass[0]) {
        std::strncpy(reinterpret_cast<char*>(s_cfg.sta.password), pass,
                     sizeof(s_cfg.sta.password) - 1);
        s_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    s_cfg.sta.pmf_cfg.capable = true;

    const bool had_link = (s_status == Status::Connected || s_status == Status::Connecting);
    s_want_conn = true;
    s_retry = 0;
    s_retry_at_us = 0;
    s_status = Status::Connecting;

    // If an association already exists, tear it down and let the DISCONNECTED
    // handler apply the new config; otherwise set + connect straight away.
    if (had_link) {
        s_apply_on_down = true;
        esp_wifi_disconnect();
        return ESP_OK;
    }
    esp_wifi_set_config(WIFI_IF_STA, &s_cfg);
    return esp_wifi_connect();
}

esp_err_t connectSaved()
{
    const auto& st = core::Settings::instance();
    if (st.wifiSsid().empty()) return ESP_ERR_INVALID_STATE;
    return connect(st.wifiSsid().c_str(), st.wifiPass().c_str());
}

void disconnect()
{
    s_want_conn = false;
    s_retry_at_us = 0;
    s_status = Status::Off;
    if (s_ready) esp_wifi_disconnect();
    s_ip[0] = '\0';
}

Status status() { return s_status; }
bool connected() { return s_status == Status::Connected; }

bool ipString(char out[16])
{
    std::strncpy(out, s_ip, 16);
    out[15] = '\0';
    return s_ip[0] != '\0';
}

int8_t rssi()
{
    wifi_ap_record_t ap;
    if (s_status == Status::Connected && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

esp_err_t scanStart()
{
    if (ensure_started() != ESP_OK) return ESP_FAIL;
    if (s_scanning) return ESP_OK;             // already running

    wifi_scan_config_t sc = {};
    sc.show_hidden = false;
    s_scanning = true;
    esp_err_t e = esp_wifi_scan_start(&sc, /*block=*/false);
    if (e != ESP_OK) {
        s_scanning = false;
        ESP_LOGW(TAG, "scan start failed: %s", esp_err_to_name(e));
    }
    return e;
}

bool scanBusy() { return s_scanning; }

int scanCollect(ScanResult* out, int max)
{
    if (!out || max <= 0 || s_scanning) return 0;

    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n == 0) return 0;

    static wifi_ap_record_t recs[24];
    uint16_t take = std::min<uint16_t>(n, 24);
    if (esp_wifi_scan_get_ap_records(&take, recs) != ESP_OK) return 0;

    int w = 0;
    for (uint16_t i = 0; i < take && w < max; ++i) {
        char ssid[33];
        std::strncpy(ssid, reinterpret_cast<const char*>(recs[i].ssid), 32);
        ssid[32] = '\0';
        if (ssid[0] == '\0') continue;  // hidden

        // De-dup by SSID (multiple BSSIDs / bands); keep the strongest.
        int at = -1;
        for (int k = 0; k < w; ++k)
            if (std::strcmp(out[k].ssid, ssid) == 0) { at = k; break; }
        if (at >= 0) {
            if (recs[i].rssi > out[at].rssi) out[at].rssi = recs[i].rssi;
            continue;
        }
        std::strcpy(out[w].ssid, ssid);
        out[w].rssi = recs[i].rssi;
        out[w].secured = recs[i].authmode != WIFI_AUTH_OPEN;
        ++w;
    }
    return w;
}

void syncTimeSoon()
{
    if (s_status == Status::Connected) start_sntp();
}

uint32_t ntpSyncCount() { return s_ntp_syncs; }

bool poll()
{
    if (!s_ready) return false;

    // Deferred reconnect after the back-off window.
    if (s_retry_at_us != 0 && esp_timer_get_time() >= s_retry_at_us) {
        s_retry_at_us = 0;
        ++s_retry;
        ESP_LOGI(TAG, "reconnect attempt %d/%d", s_retry, kMaxRetries);
        esp_wifi_connect();
    }

    // Drain an SNTP result into the RTC (local time).
    if (s_ntp_pending && s_ntp_epoch != 0) {
        const time_t local = static_cast<time_t>(s_ntp_epoch) + kTzOffsetSec;
        hal::rtc::setUnix(local);
        // Keep the C library clock on local wall-clock too (SNTP's own
        // settimeofday leaves it on UTC, which then disagrees with the RTC and
        // with filesystem timestamps).
        struct timeval tv{};
        tv.tv_sec = local;
        settimeofday(&tv, nullptr);
        s_ntp_pending = false;
        s_ntp_epoch = 0;
        ++s_ntp_syncs;
        struct tm tmv;
        gmtime_r(&local, &tmv);
        ESP_LOGI(TAG, "time synced: %04d-%02d-%02d %02d:%02d:%02d (local)",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    }

    if (s_status != s_reported) {
        s_reported = s_status;
        return true;
    }
    return false;
}

}  // namespace hal::wifi
