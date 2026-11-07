/*
 * Wi-Fi station + SNTP time sync.
 *
 * STA only (no SoftAP). Credentials live in core::Settings; this module never
 * persists them. Connection is asynchronous — drive poll() from the kernel tick
 * and read status(). On the first got-IP an SNTP sync is kicked automatically;
 * on success the RX8130CE is set (local time, see kTzOffsetSec) via hal::rtc.
 */
#pragma once

#include <cstdint>

#include <esp_err.h>

namespace hal::wifi {

enum class Status : uint8_t {
    Off,         // driver up, not attempting a connection
    Connecting,  // association / DHCP in progress (incl. back-off retries)
    Connected,   // got an IP
    Failed,      // gave up after the retry budget
};

struct ScanResult {
    char    ssid[33];
    int8_t  rssi;
    bool    secured;
};

// esp_netif + default event loop + Wi-Fi driver, STA mode. NVS must be ready.
// Does not connect.
esp_err_t init();

// Associate with an explicit AP (pass "" / nullptr for an open network).
// Asynchronous; watch status().
esp_err_t connect(const char* ssid, const char* pass);

// connect() using core::Settings; no-op (returns ESP_ERR_INVALID_STATE) when no
// SSID is stored.
esp_err_t connectSaved();

void disconnect();  // also stops auto-retry

Status  status();
bool    connected();
bool    ipString(char out[16]);  // false + "" when not connected
int8_t  rssi();                  // 0 when not connected

// --- asynchronous scan -------------------------------------------------------
// scanStart() kicks a non-blocking active scan and returns immediately. The
// caller shows a "scanning…" state and polls scanBusy(); once it is false,
// scanCollect() copies the results (RSSI descending, hidden APs dropped).
esp_err_t scanStart();
bool scanBusy();
int  scanCollect(ScanResult* out, int max);

// Request an SNTP resync on the next poll while connected.
void syncTimeSoon();

// Count of SNTP syncs applied to the RTC since boot. Poll it around a
// syncTimeSoon() call to detect completion (it increments once the RTC is set).
uint32_t ntpSyncCount();

// Kernel-tick driver: retry backoff + deferred RTC write after an SNTP sync.
// Returns true on the tick where status() changes.
bool poll();

}  // namespace hal::wifi
