#include "hal_storage.h"

#include "hal_i2c.h"

#include <cstring>

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace hal::storage {
namespace {

constexpr char TAG[] = "hal_storage";
constexpr char kMount[] = "/sd";

// SDMMC pins (native ESP32-S3 GPIO, routed through the GPIO matrix on slot 1).
constexpr int PIN_CLK = 13, PIN_CMD = 12;
constexpr int PIN_D0 = 11, PIN_D1 = 10, PIN_D2 = 9, PIN_D3 = 8;

// M5IOE1 GPIO registers (see M5IOE1.h register map).
constexpr uint8_t IOE_MODE_L = 0x03, IOE_MODE_H = 0x04;
constexpr uint8_t IOE_OUT_H  = 0x06;
constexpr uint8_t IOE_IN_L   = 0x07;
constexpr uint8_t IOE_PU_L   = 0x09;
constexpr uint8_t IOE_DRV_H  = 0x14;
constexpr uint8_t SD_EN_H_BIT  = 1u << 5;  // PYG14 -> P14 -> bit 5 of *_H
constexpr uint8_t SD_DET_L_BIT = 1u << 0;  // PYG1  -> P1  -> bit 0 of *_L
constexpr uint32_t kIoeFreq = 100000;

// Detect-line debounce and I2C poll rate-limit.
constexpr int64_t kDebounceUs   = 350'000;
constexpr int64_t kPollEveryUs  = 200'000;

// After SD_EN goes high the slot rail + card need time to come up; on a cold
// power-cycle the very first sdmmc_io_reset otherwise times out (0x108).
constexpr int     kPowerSettleMs  = 100;
constexpr int     kMountAttempts  = 3;
constexpr int     kMountGapMs     = 120;
constexpr int64_t kRetryBackoffUs = 3'000'000;   // between poll() re-mount tries

sdmmc_card_t* s_card       = nullptr;
State         s_state      = State::NoCard;
bool          s_ready      = false;

bool     s_last_raw        = false;
int64_t  s_raw_since_us    = 0;
int64_t  s_next_poll_us    = 0;
int64_t  s_retry_after_us  = 0;

esp_err_t setup_ioe_pins()
{
    esp_err_t e = ESP_OK;
    // SD_EN: push-pull output, driven high (slot powered).
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_DRV_H, SD_EN_H_BIT, 0x00, kIoeFreq);
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_MODE_H, 0x00, SD_EN_H_BIT, kIoeFreq);
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_OUT_H, 0x00, SD_EN_H_BIT, kIoeFreq);
    // SD_DET: input with pull-up (external PU also present via the SD power rail).
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_MODE_L, SD_DET_L_BIT, 0x00, kIoeFreq);
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_PU_L, 0x00, SD_DET_L_BIT, kIoeFreq);
    return e;
}

esp_err_t do_mount()
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;  // 40 MHz; the host negotiates down

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = static_cast<gpio_num_t>(PIN_CLK);
    slot.cmd = static_cast<gpio_num_t>(PIN_CMD);
    slot.d0  = static_cast<gpio_num_t>(PIN_D0);
    slot.d1  = static_cast<gpio_num_t>(PIN_D1);
    slot.d2  = static_cast<gpio_num_t>(PIN_D2);
    slot.d3  = static_cast<gpio_num_t>(PIN_D3);
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    const esp_vfs_fat_mount_config_t mcfg = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(kMount, &host, &slot, &mcfg, &s_card);
    if (err != ESP_OK) {
        s_card = nullptr;
        return err;
    }
    return ESP_OK;
}

// do_mount() with a few retries — the first sdmmc_io_reset after a cold power-up
// often times out (0x108) and then succeeds once the card settles.
esp_err_t try_mount()
{
    esp_err_t e = ESP_FAIL;
    for (int i = 0; i < kMountAttempts; ++i) {
        if (i) vTaskDelay(pdMS_TO_TICKS(kMountGapMs));
        e = do_mount();
        if (e == ESP_OK) {
            if (i) ESP_LOGI(TAG, "mounted on attempt %d", i + 1);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "mount attempt %d/%d: %s", i + 1, kMountAttempts, esp_err_to_name(e));
    }
    return e;
}

void do_unmount()
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(kMount, s_card);
        s_card = nullptr;
    }
}

}  // namespace

esp_err_t init()
{
    if (s_ready) return ESP_OK;
    esp_err_t err = setup_ioe_pins();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "M5IOE1 SD pin config failed");
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(kPowerSettleMs));   // SD_EN just went high — let it come up
    s_ready = true;
    s_last_raw = cardDetected();
    s_raw_since_us = esp_timer_get_time();
    ESP_LOGI(TAG, "ready (slot powered, DET=%d)", s_last_raw);
    return ESP_OK;
}

esp_err_t mount()
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (s_state == State::Mounted) return ESP_OK;
    esp_err_t e = try_mount();
    s_state = (e == ESP_OK) ? State::Mounted : State::MountFailed;
    if (e == ESP_OK) {
        s_last_raw = true;
        s_raw_since_us = esp_timer_get_time();
    } else {
        s_retry_after_us = 0;   // let poll() re-try as soon as the kernel runs
    }
    return e;
}

bool cardDetected()
{
    uint8_t v = 0;
    if (hal::i2c::readReg8(hal::i2c::kAddrIoe1, IOE_IN_L, &v, kIoeFreq) != ESP_OK) {
        return s_last_raw;  // keep the last known value on a transient bus error
    }
    return (v & SD_DET_L_BIT) == 0;  // active low
}

State state() { return s_state; }
const char* mountPoint() { return kMount; }

bool poll()
{
    if (!s_ready) return false;
    const int64_t now = esp_timer_get_time();
    if (now < s_next_poll_us) return false;
    s_next_poll_us = now + kPollEveryUs;

    const bool raw = cardDetected();
    if (raw != s_last_raw) {
        s_last_raw = raw;
        s_raw_since_us = now;
        return false;  // wait for it to settle
    }
    if (now - s_raw_since_us < kDebounceUs) return false;

    // Stable. Reconcile mount state with the (settled) detect line. A card that
    // is present but not mounted (NoCard on hot-insert, or MountFailed from a
    // cold boot) is re-tried, rate-limited by kRetryBackoffUs.
    const State prev = s_state;
    if (raw && s_state != State::Mounted) {
        if (now >= s_retry_after_us) {
            esp_err_t e = do_mount();
            if (e == ESP_OK) {
                s_state = State::Mounted;
                uint64_t total = 0, freeb = 0;
                usage(&total, &freeb);
                ESP_LOGI(TAG, "mounted %s (%llu MB free / %llu MB)",
                         kMount, freeb >> 20, total >> 20);
            } else {
                s_state = State::MountFailed;
                s_retry_after_us = now + kRetryBackoffUs;
                ESP_LOGW(TAG, "mount failed: %s (retry in %llds)",
                         esp_err_to_name(e), (long long)(kRetryBackoffUs / 1000000));
            }
        }
    } else if (!raw && s_state != State::NoCard) {
        do_unmount();
        s_state = State::NoCard;
        s_retry_after_us = 0;
        ESP_LOGI(TAG, "card removed, unmounted");
    }
    return s_state != prev;
}

bool usage(uint64_t* total_bytes, uint64_t* free_bytes)
{
    if (total_bytes) *total_bytes = 0;
    if (free_bytes)  *free_bytes = 0;
    if (s_state != State::Mounted) return false;

    FATFS* fs = nullptr;
    DWORD free_clusters = 0;
    if (f_getfree("0:", &free_clusters, &fs) != FR_OK || fs == nullptr) return false;

    const uint64_t sect = static_cast<uint64_t>(fs->csize) * 512;
    if (total_bytes) *total_bytes = static_cast<uint64_t>(fs->n_fatent - 2) * sect;
    if (free_bytes)  *free_bytes = static_cast<uint64_t>(free_clusters) * sect;
    return true;
}

}  // namespace hal::storage
