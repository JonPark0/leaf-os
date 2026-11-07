#include "hal_keys.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace hal::keys {
namespace {

constexpr char TAG[] = "hal_keys";

constexpr gpio_num_t kPins[kCount] = {GPIO_NUM_2, GPIO_NUM_3};

constexpr int64_t kDebounceUs  = 15'000;
constexpr int64_t kLongPressUs = 350'000;  // hold-to-ramp kicks in quickly
constexpr int64_t kRepeatUs    = 90'000;   // fast auto-repeat while held

struct KeyState {
    bool down            = false;  // debounced logical state
    bool raw             = false;  // last raw sample (active-low inverted)
    int64_t edge_us      = 0;      // when raw last changed
    int64_t down_us      = 0;      // when it became down
    bool long_fired      = false;
    int64_t next_repeat  = 0;
};

KeyState s_state[kCount];

int64_t now_us() { return esp_timer_get_time(); }

}  // namespace

esp_err_t init()
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << kPins[kA]) | (1ULL << kPins[kB]);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config: %s", esp_err_to_name(err));
        return err;
    }
    const int64_t t = now_us();
    for (auto& k : s_state) {
        k.edge_us = t;
    }
    ESP_LOGI(TAG, "keys on GPIO%d (A) / GPIO%d (B)", kPins[kA], kPins[kB]);
    return ESP_OK;
}

int poll(KeyEvent* out, int max)
{
    int n = 0;
    const int64_t t = now_us();

    for (int i = 0; i < kCount; ++i) {
        KeyState& s = s_state[i];
        const bool raw = gpio_get_level(kPins[i]) == 0;  // active-low

        if (raw != s.raw) {
            s.raw = raw;
            s.edge_us = t;
        }

        // Commit the debounced state once the raw level has been stable.
        if (raw != s.down && (t - s.edge_us) >= kDebounceUs) {
            s.down = raw;
            if (raw) {
                s.down_us     = t;
                s.long_fired  = false;
                s.next_repeat = 0;
                if (n < max) out[n++] = {static_cast<Key>(i), Event::Down};
            } else {
                if (n < max) out[n++] = {static_cast<Key>(i), Event::Up};
                if (!s.long_fired && n < max) {
                    out[n++] = {static_cast<Key>(i), Event::Click};
                }
            }
        }

        if (s.down) {
            if (!s.long_fired && (t - s.down_us) >= kLongPressUs) {
                s.long_fired  = true;
                s.next_repeat = t + kRepeatUs;
                if (n < max) out[n++] = {static_cast<Key>(i), Event::LongPress};
            } else if (s.long_fired && t >= s.next_repeat) {
                s.next_repeat = t + kRepeatUs;
                if (n < max) out[n++] = {static_cast<Key>(i), Event::Repeat};
            }
        }
    }
    return n;
}

bool isDown(Key k) { return s_state[k].down; }

}  // namespace hal::keys
