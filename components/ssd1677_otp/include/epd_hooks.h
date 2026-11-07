/*
 * Panel power / reset hooks the SSD1677 driver needs from the platform layer
 * (hal::board), kept in its own header so neither the driver nor its callers
 * pull in a transport header.
 *
 *   power(true/false)  -> M5IOE1 PYG3  EPD_3V3_EN
 *   reset_pulse()      -> M5IOE1 PYG5  EPD_RST: low ~10 ms, high ~10 ms
 */
#pragma once

namespace ssd1677 {

struct HwHooks {
    void (*power)(bool on) = nullptr;
    void (*reset_pulse)()  = nullptr;
};

}  // namespace ssd1677
