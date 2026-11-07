/*
 * Key/value persistence for core::Settings, backed by the NVS flash partition.
 */
#pragma once

#include <cstdint>
#include <string>

namespace hal::nvs {

// Initialises the partition (erasing it if its layout is stale) and opens
// `ns` for read/write. The other calls are no-ops until this succeeds.
bool open(const char* ns);

// Return true and fill `out` only when the key exists.
bool getInt(const char* key, int32_t* out);
bool getStr(const char* key, std::string* out);

// Staged until commit().
void putInt(const char* key, int32_t value);
void putStr(const char* key, const std::string& value);
void commit();

}  // namespace hal::nvs
