/*
 * microSD storage — SDMMC mount + hot-plug detection.
 *
 * Bus (docs.md "microSD"): 4-bit SDMMC on native ESP32-S3 pins
 *   CMD=G12  CLK=G13  D0=G11  D1=G10  D2=G9  D3=G8
 * Slot power-enable and card-detect are on the M5IOE1 expander:
 *   SD_EN  = PYG14  (pin index 13 -> *_H reg bit 5) — drive high to power the slot
 *   SD_DET = PYG1   (pin index 0  -> *_L reg bit 0) — active LOW when a card is in
 *
 * DET is pulled up by the microSD power domain, so SD_EN must be on for it to
 * mean anything; init() powers the slot and leaves it on. A later low-power
 * phase can gate SD_EN around mount/unmount.
 */
#pragma once

#include <cstdint>

#include <esp_err.h>

namespace hal::storage {

enum class State : uint8_t {
    NoCard,       // detect line says the slot is empty
    Mounted,      // FAT filesystem mounted at mountPoint()
    MountFailed,  // card present but mount failed (unformatted / bad card)
};

// Configure the M5IOE1 SD_EN (output high) and SD_DET (input, pull-up) pins and
// power the slot. Does not mount — the first poll() does that if a card is in.
esp_err_t init();

// Force an immediate (blocking) mount attempt, bypassing the poll() debounce.
// Useful at boot to read config before the kernel tick loop starts. No-op if
// already mounted; updates state() either way.
esp_err_t mount();

// Raw card-detect read (M5IOE1 PYG1, active-low decoded to "card present").
// Not debounced; poll() is the stable view.
bool cardDetected();

State state();
const char* mountPoint();  // "/sd"

// Debounce the detect line; mount on a stable insert, unmount on a stable
// removal. Cheap to call every kernel tick — it rate-limits its own I2C reads.
// Returns true on the tick where state() changes.
bool poll();

// Bytes on the mounted volume (both 0 when not mounted).
bool usage(uint64_t* total_bytes, uint64_t* free_bytes);

}  // namespace hal::storage
