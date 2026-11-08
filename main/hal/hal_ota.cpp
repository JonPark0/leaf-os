#include "hal_ota.h"

#include <cstdint>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>

namespace hal::ota {
namespace {

constexpr char TAG[] = "hal_ota";

esp_ota_handle_t toHandle(void* h) { return static_cast<esp_ota_handle_t>(reinterpret_cast<uintptr_t>(h)); }
void* fromHandle(esp_ota_handle_t h) { return reinterpret_cast<void*>(static_cast<uintptr_t>(h)); }

void confirmNow(void*)
{
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
        const esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "new firmware confirmed: %s", esp_err_to_name(e));
    }
}

}  // namespace

size_t nextSlotBytes()
{
    const esp_partition_t* p = esp_ota_get_next_update_partition(nullptr);
    return p ? p->size : 0;
}

void confirmAfter(int ms)
{
    esp_ota_img_states_t st;
    const esp_partition_t* run = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY) return;
    ESP_LOGI(TAG, "running %s on probation, confirming in %d ms", run->label, ms);
    esp_timer_create_args_t args = {};
    args.callback = confirmNow;
    args.name = "ota_confirm";
    esp_timer_handle_t t = nullptr;
    if (esp_timer_create(&args, &t) == ESP_OK) esp_timer_start_once(t, static_cast<uint64_t>(ms) * 1000);
}

std::string rolledBackVersion()
{
    const esp_partition_t* p = esp_ota_get_last_invalid_partition();
    if (!p) return "";
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(p, &desc) != ESP_OK) return "?";
    return desc.version;
}

Writer::~Writer() { abort(); }

bool Writer::begin(size_t size)
{
    abort();
    error_.clear();
    const esp_partition_t* p = esp_ota_get_next_update_partition(nullptr);
    if (!p) { error_ = "no update slot"; return false; }
    if (size > p->size) { error_ = "too large"; return false; }
    esp_ota_handle_t h = 0;
    const esp_err_t e = esp_ota_begin(p, OTA_WITH_SEQUENTIAL_WRITES, &h);
    if (e != ESP_OK) { error_ = esp_err_to_name(e); return false; }
    handle_ = fromHandle(h);
    part_ = p;
    open_ = true;
    ESP_LOGI(TAG, "writing %u bytes to %s", static_cast<unsigned>(size), p->label);
    return true;
}

bool Writer::write(const char* data, size_t n)
{
    if (!open_) { error_ = "not open"; return false; }
    const esp_err_t e = esp_ota_write(toHandle(handle_), data, n);
    if (e != ESP_OK) { error_ = esp_err_to_name(e); return false; }
    return true;
}

bool Writer::end()
{
    if (!open_) { error_ = "not open"; return false; }
    open_ = false;
    const esp_err_t e = esp_ota_end(toHandle(handle_));
    handle_ = nullptr;
    if (e != ESP_OK) {
        error_ = e == ESP_ERR_OTA_VALIDATE_FAILED ? "invalid image" : esp_err_to_name(e);
        return false;
    }
    return true;
}

bool Writer::setBoot()
{
    if (!part_) { error_ = "nothing written"; return false; }
    const esp_err_t e = esp_ota_set_boot_partition(static_cast<const esp_partition_t*>(part_));
    if (e != ESP_OK) { error_ = esp_err_to_name(e); return false; }
    ESP_LOGI(TAG, "next boot: %s", static_cast<const esp_partition_t*>(part_)->label);
    return true;
}

void Writer::abort()
{
    if (open_) esp_ota_abort(toHandle(handle_));
    open_ = false;
    handle_ = nullptr;
}

}  // namespace hal::ota
