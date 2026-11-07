#include "hal_i2c.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace hal::i2c {
namespace {

constexpr char TAG[]           = "hal_i2c";
constexpr gpio_num_t kPinSda   = GPIO_NUM_47;
constexpr gpio_num_t kPinScl   = GPIO_NUM_48;
constexpr i2c_port_t kPort     = I2C_NUM_0;
constexpr int kProbeTimeoutMs  = 50;
constexpr int kXferTimeoutMs   = 100;

i2c_master_bus_handle_t s_bus = nullptr;
SemaphoreHandle_t s_mutex     = nullptr;

// One cached device handle per (addr, freq) pair. The bus supports adding the
// same address twice only with different configs, so cache by addr and reuse
// at whatever freq it was first opened; callers here use one freq per device.
struct DevSlot {
    uint8_t addr                 = 0;
    uint32_t freq                = 0;
    i2c_master_dev_handle_t handle = nullptr;
};
DevSlot s_slots[8];

i2c_master_dev_handle_t device(uint8_t addr, uint32_t freq_hz)
{
    for (auto& s : s_slots) {
        if (s.handle && s.addr == addr) {
            return s.handle;
        }
    }
    for (auto& s : s_slots) {
        if (s.handle == nullptr) {
            i2c_device_config_t dc = {};
            dc.dev_addr_length     = I2C_ADDR_BIT_LEN_7;
            dc.device_address      = addr;
            dc.scl_speed_hz        = freq_hz;
            if (i2c_master_bus_add_device(s_bus, &dc, &s.handle) != ESP_OK) {
                return nullptr;
            }
            s.addr = addr;
            s.freq = freq_hz;
            return s.handle;
        }
    }
    ESP_LOGE(TAG, "device slot table full");
    return nullptr;
}

struct Guard {
    Guard() { xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY); }
    ~Guard() { xSemaphoreGiveRecursive(s_mutex); }
};

}  // namespace

esp_err_t init()
{
    if (s_bus != nullptr) {
        return ESP_OK;
    }
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateRecursiveMutex();
    }

    i2c_master_bus_config_t bc = {};
    bc.i2c_port                     = kPort;
    bc.sda_io_num                   = kPinSda;
    bc.scl_io_num                   = kPinScl;
    bc.clk_source                   = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt            = 7;
    bc.flags.enable_internal_pullup = true;

    const esp_err_t err = i2c_new_master_bus(&bc, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "system bus up (SDA=%d SCL=%d)", kPinSda, kPinScl);
    return ESP_OK;
}

esp_err_t probe(uint8_t dev_addr)
{
    Guard g;
    return i2c_master_probe(s_bus, dev_addr, kProbeTimeoutMs);
}

esp_err_t readReg8(uint8_t dev_addr, uint8_t reg, uint8_t* out, uint32_t freq_hz)
{
    Guard g;
    auto dev = device(dev_addr, freq_hz);
    if (dev == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(dev, &reg, 1, out, 1, kXferTimeoutMs);
}

esp_err_t readRegN(uint8_t dev_addr, uint8_t reg, uint8_t* out, size_t len, uint32_t freq_hz)
{
    Guard g;
    auto dev = device(dev_addr, freq_hz);
    if (dev == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(dev, &reg, 1, out, len, kXferTimeoutMs);
}

esp_err_t writeReg8(uint8_t dev_addr, uint8_t reg, uint8_t val, uint32_t freq_hz)
{
    Guard g;
    auto dev = device(dev_addr, freq_hz);
    if (dev == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, sizeof(buf), kXferTimeoutMs);
}

esp_err_t rmw8(uint8_t dev_addr, uint8_t reg, uint8_t clear_mask, uint8_t set_mask, uint32_t freq_hz)
{
    Guard g;
    uint8_t v = 0;
    esp_err_t err = readReg8(dev_addr, reg, &v, freq_hz);
    if (err != ESP_OK) {
        return err;
    }
    v = static_cast<uint8_t>((v & ~clear_mask) | set_mask);
    return writeReg8(dev_addr, reg, v, freq_hz);
}

void lock()   { xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGiveRecursive(s_mutex); }

}  // namespace hal::i2c
