#include "hal_nvs.h"

#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <utility>

namespace hal::nvs {
namespace {
constexpr char TAG[] = "hal_nvs";
nvs_handle_t s_handle = 0;
}  // namespace

bool open(const char* ns)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
        return false;
    }
    if (nvs_open(ns, NVS_READWRITE, &s_handle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return false;
    }
    return true;
}

bool getInt(const char* key, int32_t* out)
{
    return nvs_get_i32(s_handle, key, out) == ESP_OK;
}

bool getStr(const char* key, std::string* out)
{
    size_t n = 0;  // includes the terminating NUL
    if (nvs_get_str(s_handle, key, nullptr, &n) != ESP_OK || n == 0) return false;
    std::string b(n, '\0');
    nvs_get_str(s_handle, key, b.data(), &n);
    b.resize(n ? n - 1 : 0);
    *out = std::move(b);
    return true;
}

void putInt(const char* key, int32_t value)
{
    if (s_handle) nvs_set_i32(s_handle, key, value);
}

void putStr(const char* key, const std::string& value)
{
    if (s_handle) nvs_set_str(s_handle, key, value.c_str());
}

void commit()
{
    if (s_handle) nvs_commit(s_handle);
}

}  // namespace hal::nvs
