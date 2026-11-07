/*
 * RX8130CE real-time clock (system I2C bus, addr 0x32).
 *
 * INT is wired to M5PM1 as a low-power wake source (docs.md); alarm/timer setup
 * is deferred to the power-management phase. This module does time only.
 */
#pragma once

#include <cstdint>
#include <ctime>

#include <esp_err.h>

namespace hal::rtc {

struct DateTime {
    int year = 2025;  // full year
    int month = 1;    // 1..12
    int day = 1;      // 1..31
    int hour = 0;     // 0..23
    int minute = 0;
    int second = 0;
    int weekday = 0;  // 0=Sun .. 6=Sat
};

// Clears the "power-on / voltage-low" flag housekeeping. Requires hal::i2c.
esp_err_t init();

// True if the RTC lost power since it was last set — the time is not trustworthy.
bool timeInvalid();

bool get(DateTime& out);
bool set(const DateTime& dt);

// Convenience: set from a Unix timestamp (UTC; caller applies any offset first).
bool setUnix(time_t t);
time_t toUnix(const DateTime& dt);

}  // namespace hal::rtc
