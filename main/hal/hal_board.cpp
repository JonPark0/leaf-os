#include "hal_board.h"

#include "hal_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace hal::board {
namespace {

constexpr char TAG[] = "hal_board";

// --- M5PM1 (0x6E) registers (from M5GFX PaperMono autodetect sequence) ------
constexpr uint8_t PM1_REG_I2C_CFG = 0x09;  // write 0x00: disable I2C idle sleep
constexpr uint8_t PM1_REG_WDT_CNT = 0x0A;  // write 0x00: disable watchdog
constexpr uint8_t PM1_REG_PWR_CFG = 0x06;  // set 0x17: LED_CTRL | 3V3_LDO | 3V3_DCDC | CHG
constexpr uint8_t PM1_PWR_BITS    = 0x17;

// --- M5IOE1 (0x4F) registers (datasheet; bit N of *_L == pin index N) ------
constexpr uint8_t IOE_REG_MODE_L = 0x03;  // 1 = output
constexpr uint8_t IOE_REG_OUT_L  = 0x05;  // output level, pins 0..7
constexpr uint8_t IOE_REG_DRV_L  = 0x13;  // 0 = push-pull, 1 = open-drain
constexpr uint8_t IOE_REG_I2C_CFG = 0x23; // [3:0] SLEEP = 0: disable idle sleep

// product docs PinMap: EPD 3V3 enable = PYG3 (pin idx 2), EPD reset = PYG5 (idx 4).
constexpr uint8_t EPD_EN_BIT  = 1u << 2;
constexpr uint8_t EPD_RST_BIT = 1u << 4;
constexpr uint8_t EPD_BITS    = EPD_EN_BIT | EPD_RST_BIT;

constexpr uint32_t kPm1Freq = 100000;
constexpr uint32_t kIoeFreq = 100000;

bool s_ready = false;

esp_err_t setup_pm1()
{
    esp_err_t e = ESP_OK;
    e |= hal::i2c::writeReg8(hal::i2c::kAddrPm1, PM1_REG_I2C_CFG, 0x00, kPm1Freq);
    e |= hal::i2c::writeReg8(hal::i2c::kAddrPm1, PM1_REG_WDT_CNT, 0x00, kPm1Freq);
    e |= hal::i2c::rmw8(hal::i2c::kAddrPm1, PM1_REG_PWR_CFG, 0x00, PM1_PWR_BITS, kPm1Freq);
    return e;
}

esp_err_t setup_ioe1_epd_pins()
{
    esp_err_t e = ESP_OK;
    e |= hal::i2c::writeReg8(hal::i2c::kAddrIoe1, IOE_REG_I2C_CFG, 0x00, kIoeFreq);
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_REG_DRV_L, EPD_BITS, 0x00, kIoeFreq);   // push-pull
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_REG_MODE_L, 0x00, EPD_BITS, kIoeFreq);  // output
    e |= hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_REG_OUT_L, 0x00, EPD_EN_BIT, kIoeFreq); // power on, hold reset high later
    return e;
}

}  // namespace

esp_err_t init()
{
    if (s_ready) {
        return ESP_OK;
    }
    esp_err_t err = hal::i2c::init();
    if (err != ESP_OK) {
        return err;
    }

    if (setup_pm1() != ESP_OK) {
        ESP_LOGW(TAG, "M5PM1 config had I2C errors (continuing)");
    }
    if (setup_ioe1_epd_pins() != ESP_OK) {
        ESP_LOGE(TAG, "M5IOE1 EPD pin config failed");
        return ESP_FAIL;
    }

    // Drive EPD power on and release reset.
    epdPower(true);
    epdResetPulse();

    s_ready = true;
    ESP_LOGI(TAG, "board init ok (PM1 quieted, EPD powered)");
    return ESP_OK;
}

void epdPower(bool on)
{
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_REG_OUT_L,
                   on ? 0x00 : EPD_EN_BIT,
                   on ? EPD_EN_BIT : 0x00,
                   kIoeFreq);
}

void epdResetPulse()
{
    // Ensure power, then low 10 ms / high 10 ms.
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_REG_OUT_L, EPD_RST_BIT, EPD_EN_BIT, kIoeFreq);
    vTaskDelay(pdMS_TO_TICKS(10));
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_REG_OUT_L, 0x00, EPD_RST_BIT, kIoeFreq);
    vTaskDelay(pdMS_TO_TICKS(10));
}

}  // namespace hal::board
