#include "hal_power.h"

#include "hal_i2c.h"

#include <algorithm>

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace hal::power {
namespace {

constexpr char TAG[] = "hal_power";
constexpr uint32_t kFreq = 100000;

// M5PM1 registers (see managed_components/m5pm1/src/M5PM1.h).
constexpr uint8_t REG_PWR_SRC   = 0x04;  // R [2:0] power source
constexpr uint8_t REG_SYS_CMD   = 0x0C;  // write 0xA0|cmd: 1=off, 2=reboot, 3=dl
constexpr uint8_t REG_VBAT_L    = 0x22;  // + 0x23 high nibble, millivolts
constexpr uint8_t REG_GPIO_DRV  = 0x13;  // per-GPIO drive: bit N 0=push-pull 1=open-drain
constexpr uint8_t REG_GPIO_FUNC0 = 0x16; // [7:6] GPIO3 func: 0b11 = PWM0
constexpr uint8_t REG_PWM0_L    = 0x30;
constexpr uint8_t REG_PWM0_HC   = 0x31;  // [5]POL [4]EN [3:0] duty high nibble
constexpr uint8_t REG_PWM_FREQ_L = 0x34; // + 0x35, Hz
constexpr uint8_t REG_PWM_FREQ_H = 0x35;

constexpr uint8_t GPIO3_BIT = 1u << 3;

constexpr uint16_t kPwmFreqHz   = 2000;
constexpr int kDefaultPerMille  = 0;

int s_per_mille = 0;

void write_duty(uint16_t duty12, bool enable)
{
    duty12 = std::min<uint16_t>(duty12, 4095);
    hal::i2c::writeReg8(hal::i2c::kAddrPm1, REG_PWM0_L, duty12 & 0xFF, kFreq);
    const uint8_t hc = static_cast<uint8_t>((enable ? 0x10 : 0x00) | ((duty12 >> 8) & 0x0F));
    hal::i2c::writeReg8(hal::i2c::kAddrPm1, REG_PWM0_HC, hc, kFreq);
}

}  // namespace

esp_err_t init()
{
    // GPIO3 must be PUSH-PULL for the front-light: as open-drain (the M5PM1
    // default) the PWM can only sink, the high phase floats, and the LED
    // driver never turns on. M5GFX's Light_M5PaperMono clears 0x13 bit 3
    // before switching the pin to PWM — do the same, same order.
    esp_err_t e = hal::i2c::rmw8(hal::i2c::kAddrPm1, REG_GPIO_DRV, GPIO3_BIT, 0x00, kFreq);
    // GPIO3 -> PWM0 (bits [7:6] = 0b11), preserve the rest.
    e |= hal::i2c::rmw8(hal::i2c::kAddrPm1, REG_GPIO_FUNC0, 0xC0, 0xC0, kFreq);
    hal::i2c::writeReg8(hal::i2c::kAddrPm1, REG_PWM_FREQ_L, kPwmFreqHz & 0xFF, kFreq);
    hal::i2c::writeReg8(hal::i2c::kAddrPm1, REG_PWM_FREQ_H, kPwmFreqHz >> 8, kFreq);

    setBrightness(kDefaultPerMille);
    ESP_LOGI(TAG, "front-light PWM0 @ %u Hz, brightness %d%%%s",
             kPwmFreqHz, brightnessPercent(), e == ESP_OK ? "" : " (cfg write err)");
    return ESP_OK;
}

void setBrightness(int per_mille)
{
    s_per_mille = std::clamp(per_mille, 0, 1000);
    const uint16_t duty = static_cast<uint16_t>(s_per_mille * 4095 / 1000);
    write_duty(duty, s_per_mille > 0);
}

int brightness() { return s_per_mille; }

void setBrightnessPercent(int pct) { setBrightness(std::clamp(pct, 0, 100) * 10); }
int  brightnessPercent() { return (s_per_mille + 5) / 10; }

int batteryMilliVoltsRaw()
{
    // M5PM1 latches the pair on a single burst read; VBAT is a full 16-bit
    // little-endian millivolt value (the header's "high 4 bits" note is wrong —
    // the vendor driver reads all 16 bits). Two separate byte reads return
    // garbage (~150).
    uint8_t b[2] = {};
    if (hal::i2c::readRegN(hal::i2c::kAddrPm1, REG_VBAT_L, b, 2, kFreq) != ESP_OK) return 0;
    return b[0] | (b[1] << 8);
}

int batteryMilliVolts()
{
    static int ema = 0;
    const int raw = batteryMilliVoltsRaw();
    if (raw <= 0) return ema;                 // keep last good value on error
    ema = (ema == 0) ? raw : (ema * 7 + raw) / 8;
    return ema;
}

int batteryPercent()
{
    const int mv = batteryMilliVolts();
    if (mv <= 0) return -1;
    // Coarse LiPo curve: 3.30 V empty, 4.20 V full.
    const int p = (mv - 3300) * 100 / (4200 - 3300);
    return std::clamp(p, 0, 100);
}

void reboot()
{
    ESP_LOGI(TAG, "reboot requested");
    esp_restart();
}

bool shutdown()
{
    // M5PM1 PWR_SRC (reg 0x04) [2:0] is a presence bitmask, not an enum:
    //   bit0 = 5VIN present, bit1 = 5VINOUT present, bit2 = battery present.
    // (observed: 0x05 = USB + battery, powers off cleanly.)
    //
    // The battery bridges the rail while the M5PM1 cuts external 5V, so a cell
    // that is present makes power-off safe. Only "external 5V, no battery" would
    // brown out (`E BOD`) into a reset — refuse that one case with a message.
    uint8_t raw = 0xFF;
    const bool ok =
        hal::i2c::readReg8(hal::i2c::kAddrPm1, REG_PWR_SRC, &raw, kFreq) == ESP_OK;
    const uint8_t src = raw & 0x07;
    const bool ext_5v  = (src & 0x03) != 0;
    const bool has_batt = (src & 0x04) != 0;
    ESP_LOGI(TAG, "power off: PWR_SRC raw=0x%02x (5v=%d batt=%d %s)",
             raw, ext_5v, has_batt, ok ? "ok" : "READ FAIL");
    if (ok && ext_5v && !has_batt) {
        ESP_LOGW(TAG, "power off refused: on external 5V with no battery");
        return false;
    }

    setBrightness(0);
    hal::i2c::writeReg8(hal::i2c::kAddrPm1, REG_SYS_CMD, 0xA1, kFreq);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));   // the rail drops here
}

}  // namespace hal::power
