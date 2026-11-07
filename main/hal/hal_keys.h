/*
 * The two side keys.  ESP32-S3 GPIO2 = Button A (KEY1), GPIO3 = Button B (KEY2)
 * (product docs HMI). Active-low with internal pull-up. The M5PM1's own "GPIO3"
 * (front-light PWM) is a different pin — no conflict.
 *
 * Poll-based: call poll() once per kernel tick; it returns discrete events.
 * Default system behaviour (front-light brightness on A/B) is applied by the
 * caller when the foreground app does not consume the key.
 */
#pragma once

#include <cstdint>

#include <esp_err.h>

namespace hal::keys {

enum Key : uint8_t { kA = 0, kB = 1, kCount = 2 };

enum class Event : uint8_t {
    Down,       // pressed
    Up,         // released (any)
    Click,      // pressed and released under the long-press threshold
    LongPress,  // held past the threshold (fires once)
    Repeat,     // held: fires periodically after LongPress (e.g. brightness ramp)
};

struct KeyEvent {
    Key key;
    Event ev;
};

esp_err_t init();

// Fill `out` with events since the previous call; returns the count.
int poll(KeyEvent* out, int max);

// Current raw state (true = held down).
bool isDown(Key k);

}  // namespace hal::keys
