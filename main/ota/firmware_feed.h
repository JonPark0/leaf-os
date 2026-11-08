/*
 * Firmware versions and the update feed (firmware/v1/latest.json). Host-clean:
 * no ESP-IDF, so tests/ builds it. See docs/design/ota-update.md.
 */
#pragma once

#include <cstddef>
#include <string>

namespace ota {

// `git describe` output: "v1.2.3", or "v1.2.3-4-gabcdef0[-dirty]" on a dev build.
struct FirmwareVersion {
    int  major = 0, minor = 0, patch = 0;
    bool dev = false;   // anything after the numbers: commits past the tag
};

bool parseFirmwareVersion(const std::string& s, FirmwareVersion& out);

// Whether `latest` (a release tag) should be offered to a device running
// `running`: only when its numbers are higher. A dev build ("v0.2.0-4-g…") is
// already past its tag, so the same tag is not offered. An unreadable running
// version always gets the offer; an unreadable or prerelease `latest` never.
bool updateAvailable(const std::string& running, const std::string& latest);

struct Feed {
    std::string version;   // release tag, e.g. "v0.3.0"
    size_t      size = 0;
    std::string sha256;    // 64 lower-case hex
    std::string file;      // name next to latest.json
    std::string notes;     // release page URL, may be empty
};

constexpr size_t kMinImageBytes = 64 * 1024;
constexpr size_t kMaxFeedBytes  = 4 * 1024;

// Checks the fields of a parsed feed; `slot_bytes` is the target slot's size.
bool validateFeed(const Feed& f, size_t slot_bytes, std::string& why);

}  // namespace ota
