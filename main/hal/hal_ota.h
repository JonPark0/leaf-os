/*
 * Firmware update slots (esp_ota). See docs/design/ota-update.md.
 *
 * Writer fills the app slot that is not running; setBoot() makes it the next
 * boot. With CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE the new image starts on
 * probation: confirmAfter() marks it good once it has run for a while, and a
 * reset before that sends the bootloader back to the previous slot.
 */
#pragma once

#include <cstddef>
#include <string>

namespace hal::ota {

// Size of the slot an update would be written to (0 if there is none).
size_t nextSlotBytes();

// Marks the running image valid after `ms`, if it is still on probation.
// Call once at boot, after the UI is up.
void confirmAfter(int ms);

// Version of an image that failed to start and was rolled back, or "" if none.
std::string rolledBackVersion();

class Writer {
public:
    Writer() = default;
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    // Opens the next slot for `size` bytes. On failure `error()` says why.
    bool begin(size_t size);
    bool write(const char* data, size_t n);
    // Validates the image (header, chip, checksum) and closes the slot.
    bool end();
    // Makes the written slot the next boot. Call after end().
    bool setBoot();
    // Drops a partly written slot; the running image stays the boot image.
    void abort();

    const std::string& error() const { return error_; }

private:
    void* handle_ = nullptr;      // esp_ota_handle_t, 0 when closed
    bool open_ = false;
    const void* part_ = nullptr;  // const esp_partition_t*
    std::string error_;
};

}  // namespace hal::ota
