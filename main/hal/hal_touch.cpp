#include "hal_touch.h"

#include "hal_i2c.h"

#include <algorithm>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace hal::touch {
namespace {

constexpr char TAG[] = "hal_touch";

constexpr uint8_t  kAddr    = 0x38;
constexpr uint32_t kFreq    = 400000;
constexpr uint8_t  REG_TD_STATUS = 0x02;  // then 5 bytes: XH XL YH YL for point 1

// M5IOE1 gating pins. TP_VDD_EN = PYG13 -> bit 4 of the *_H registers.
// TP_RST = PYG6 -> bit 5 of the *_L registers.
constexpr uint8_t IOE_MODE_L = 0x03, IOE_OUT_L = 0x05, IOE_DRV_L = 0x13;
constexpr uint8_t IOE_MODE_H = 0x04, IOE_OUT_H = 0x06, IOE_DRV_H = 0x14;
constexpr uint8_t TP_VDD_H_BIT = 1u << 4;  // PYG13 in H regs
constexpr uint8_t TP_RST_L_BIT = 1u << 5;  // PYG6  in L regs

// Firmware-clamped active area (docs.md).
constexpr uint16_t X_MIN = 5, X_MAX = 475;
constexpr uint16_t Y_MIN = 5, Y_MAX = 795;
constexpr int PANEL_H = 800;

// FT6336G raw coordinates are already aligned with the un-flipped framebuffer
// (verified: tapping the top-left tile reads ~(89,127), not ~(89,672)). No axis
// inversion — the display's uiToRam transform makes fb(x,y) -> physical(x,y).
constexpr bool kInvertY = false;

bool s_ready = false;

void power_up_via_ioe()
{
    // TP_VDD_EN: push-pull output, high.
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_DRV_H, TP_VDD_H_BIT, 0x00);
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_MODE_H, 0x00, TP_VDD_H_BIT);
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_OUT_H, 0x00, TP_VDD_H_BIT);
    vTaskDelay(pdMS_TO_TICKS(5));

    // TP_RST: push-pull output, pulse low then high.
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_DRV_L, TP_RST_L_BIT, 0x00);
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_MODE_L, 0x00, TP_RST_L_BIT);
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_OUT_L, TP_RST_L_BIT, 0x00);  // low
    vTaskDelay(pdMS_TO_TICKS(10));
    hal::i2c::rmw8(hal::i2c::kAddrIoe1, IOE_OUT_L, 0x00, TP_RST_L_BIT);  // high
    vTaskDelay(pdMS_TO_TICKS(300));  // controller boot
}

}  // namespace

esp_err_t init()
{
    s_ready = false;
    power_up_via_ioe();

    esp_err_t err = hal::i2c::probe(kAddr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FT6336G not responding at 0x%02X: %s", kAddr, esp_err_to_name(err));
        return err;
    }
    s_ready = true;
    ESP_LOGI(TAG, "FT6336G up");
    return ESP_OK;
}

bool ready() { return s_ready; }

int read(Point* out, int max)
{
    if (!s_ready || max <= 0) {
        return 0;
    }
    uint8_t buf[11] = {};  // TD_STATUS + up to 2 points * 6 bytes... read 11 from 0x02
    if (hal::i2c::readRegN(kAddr, REG_TD_STATUS, buf, sizeof(buf), kFreq) != ESP_OK) {
        return 0;
    }
    int n = buf[0] & 0x0F;
    if (n > 2) {
        n = 0;  // stale / invalid
    }
    n = std::min(n, max);
    for (int i = 0; i < n; ++i) {
        const uint8_t* p = &buf[1 + i * 6];  // XH XL YH YL (+2 pad)
        uint16_t x = static_cast<uint16_t>(((p[0] & 0x0F) << 8) | p[1]);
        uint16_t y = static_cast<uint16_t>(((p[2] & 0x0F) << 8) | p[3]);
        if (kInvertY) {
            const int fy = (PANEL_H - 1) - static_cast<int>(y);
            y = static_cast<uint16_t>(fy < 0 ? 0 : fy);
        }
        out[i].x = std::clamp<uint16_t>(x, X_MIN, X_MAX);
        out[i].y = std::clamp<uint16_t>(y, Y_MIN, Y_MAX);
    }
    return n;
}

}  // namespace hal::touch
