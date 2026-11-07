#include "hal_rtc.h"

#include "hal_i2c.h"

#include <esp_log.h>

namespace hal::rtc {
namespace {

constexpr char TAG[]      = "hal_rtc";
constexpr uint8_t kAddr   = 0x32;
constexpr uint32_t kFreq  = 400000;

// RX8130CE register map (from M5Unified RX8130_Class).
constexpr uint8_t REG_SEC   = 0x10;  // 0x10..0x16 = sec,min,hour,week,day,mon,year (BCD)
constexpr uint8_t REG_FLAG  = 0x1D;  // bit7 = VLF (voltage low -> data invalid)
constexpr uint8_t REG_CTRL0 = 0x1E;
constexpr uint8_t REG_CTRL1 = 0x1F;
constexpr uint8_t REG_DIG_OFF = 0x30;

uint8_t bcd2bin(uint8_t v) { return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F)); }
uint8_t bin2bcd(int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

}  // namespace

esp_err_t init()
{
    // Enable the RTC block, zero the digital-offset and control-0 registers
    // (matches the vendor begin()).
    uint8_t c1 = 0;
    hal::i2c::readReg8(kAddr, REG_CTRL1, &c1, kFreq);
    hal::i2c::writeReg8(kAddr, REG_CTRL1, c1 | 0x30, kFreq);
    hal::i2c::writeReg8(kAddr, REG_DIG_OFF, 0x00, kFreq);
    hal::i2c::writeReg8(kAddr, REG_CTRL0, 0x00, kFreq);

    uint8_t flag = 0;
    hal::i2c::readReg8(kAddr, REG_FLAG, &flag, kFreq);
    ESP_LOGI(TAG, "up, flags=0x%02X%s", flag, (flag & 0x80) ? " (VLF: time invalid)" : "");
    return ESP_OK;
}

bool timeInvalid()
{
    uint8_t flag = 0;
    if (hal::i2c::readReg8(kAddr, REG_FLAG, &flag, kFreq) != ESP_OK) return true;
    return (flag & 0x80) != 0;
}

bool get(DateTime& o)
{
    uint8_t b[7] = {};
    if (hal::i2c::readRegN(kAddr, REG_SEC, b, 7, kFreq) != ESP_OK) return false;

    const uint8_t sec = b[0] & 0x7F, min = b[1] & 0x7F, hour = b[2] & 0x3F;
    const uint8_t wd = b[3], day = b[4] & 0x3F, mon = b[5] & 0x1F, yr = b[6];
    if (wd == 0 || (wd & (wd - 1)) != 0) return false;  // weekday must be one bit

    o.second  = bcd2bin(sec);
    o.minute  = bcd2bin(min);
    o.hour    = bcd2bin(hour);
    o.weekday = __builtin_ctz(wd);
    o.day     = bcd2bin(day);
    o.month   = bcd2bin(mon);
    o.year    = 2000 + bcd2bin(yr);

    return o.second < 60 && o.minute < 60 && o.hour < 24 &&
           o.day >= 1 && o.day <= 31 && o.month >= 1 && o.month <= 12;
}

bool set(const DateTime& d)
{
    uint8_t b[7] = {
        bin2bcd(d.second), bin2bcd(d.minute), bin2bcd(d.hour),
        static_cast<uint8_t>(1u << (d.weekday & 7)),
        bin2bcd(d.day), bin2bcd(d.month), bin2bcd(d.year % 100),
    };
    esp_err_t e = ESP_OK;
    for (int i = 0; i < 7; ++i) {
        e |= hal::i2c::writeReg8(kAddr, REG_SEC + i, b[i], kFreq);
    }
    // Clear the voltage-low flag (write-0-to-clear on bit 7).
    hal::i2c::writeReg8(kAddr, REG_FLAG, 0x7F, kFreq);
    return e == ESP_OK;
}

time_t toUnix(const DateTime& d)
{
    // Days-from-civil (Howard Hinnant). Treats the stored time as UTC; avoids
    // timegm(), which newlib on ESP-IDF does not provide.
    int y = d.year;
    const unsigned m = static_cast<unsigned>(d.month);
    const unsigned day = static_cast<unsigned>(d.day);
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
    return static_cast<time_t>(days * 86400 + d.hour * 3600 + d.minute * 60 + d.second);
}

bool setUnix(time_t t)
{
    struct tm tm = {};
    gmtime_r(&t, &tm);
    DateTime d;
    d.year = tm.tm_year + 1900;
    d.month = tm.tm_mon + 1;
    d.day = tm.tm_mday;
    d.hour = tm.tm_hour;
    d.minute = tm.tm_min;
    d.second = tm.tm_sec;
    d.weekday = tm.tm_wday;
    return set(d);
}

}  // namespace hal::rtc
