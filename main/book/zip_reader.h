/*
 * Read-only ZIP over an in-memory buffer — just enough for EPUB.
 *
 * Parses the End-of-Central-Directory + central directory; extracts a named
 * entry (stored or DEFLATE). DEFLATE uses `tinfl_decompress_mem_to_mem` from
 * the ESP32-S3 ROM, so this file only builds under ESP-IDF (the rest of `book/`
 * stays host-testable). No Zip64 (EPUBs never need it).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace book {

class ZipReader {
public:
    // `data` must outlive the reader and every extract() call.
    bool open(const uint8_t* data, size_t len);

    size_t count() const { return entries_.size(); }
    const std::string& name(size_t i) const { return entries_[i].name; }
    int indexOf(const char* name) const;

    bool extract(size_t i, std::string& out) const;
    bool extract(const char* name, std::string& out) const;

private:
    struct Entry {
        std::string name;
        uint16_t method = 0;
        uint32_t csize = 0, usize = 0, local_off = 0;
    };
    const uint8_t* data_ = nullptr;
    size_t len_ = 0;
    std::vector<Entry> entries_;
};

}  // namespace book
