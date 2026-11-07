#include "zip_reader.h"

#include <cstring>

extern "C" {
#include "miniz.h"   // ESP32-S3 ROM: tinfl_decompress_mem_to_mem
}

namespace book {
namespace {

uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

constexpr uint32_t kEOCD = 0x06054b50;
constexpr uint32_t kCDH  = 0x02014b50;
constexpr uint32_t kLFH  = 0x04034b50;

// Ceiling for one inflated entry. C++ exceptions are off, so an oversized
// resize() from a hostile `usize` would abort rather than fail; a single
// XHTML chapter is far below this on an 8 MB-PSRAM device.
constexpr uint32_t kMaxEntryBytes = 4u * 1024 * 1024;

}  // namespace

bool ZipReader::open(const uint8_t* data, size_t len)
{
    entries_.clear();
    data_ = data;
    len_ = len;
    if (!data || len < 22) return false;

    // Find the EOCD by scanning back from the end (no comment expected).
    size_t scan = len < 66000 ? 0 : len - 66000;
    long eocd = -1;
    for (size_t i = len - 22; i + 4 <= len && i >= scan; --i) {
        if (rd32(data + i) == kEOCD) { eocd = long(i); break; }
        if (i == 0) break;
    }
    if (eocd < 0) return false;

    const uint8_t* e = data + eocd;
    const uint16_t n_entries = rd16(e + 10);
    const uint32_t cd_off = rd32(e + 16);
    if (cd_off >= len) return false;

    const uint8_t* p = data + cd_off;
    const uint8_t* limit = data + len;
    for (uint16_t k = 0; k < n_entries; ++k) {
        if (p + 46 > limit || rd32(p) != kCDH) break;
        Entry en;
        en.method    = rd16(p + 10);
        en.csize     = rd32(p + 20);
        en.usize     = rd32(p + 24);
        const uint16_t nlen = rd16(p + 28);
        const uint16_t xlen = rd16(p + 30);
        const uint16_t clen = rd16(p + 32);
        en.local_off = rd32(p + 42);
        const size_t rec = 46u + nlen + xlen + clen;
        if (rec > static_cast<size_t>(limit - p)) break;
        en.name.assign(reinterpret_cast<const char*>(p + 46), nlen);
        entries_.push_back(std::move(en));
        p += rec;
    }
    return !entries_.empty();
}

int ZipReader::indexOf(const char* name) const
{
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].name == name) return int(i);
    return -1;
}

bool ZipReader::extract(size_t i, std::string& out) const
{
    if (i >= entries_.size() || !data_) return false;
    const Entry& en = entries_[i];

    // Offsets come from the file: compare by subtraction so a hostile value
    // near 4 GiB cannot wrap size_t (32-bit here) past the bounds checks.
    if (len_ < 30 || en.local_off > len_ - 30) return false;
    const uint8_t* lh = data_ + en.local_off;
    if (rd32(lh) != kLFH) return false;
    const uint16_t nlen = rd16(lh + 26);
    const uint16_t xlen = rd16(lh + 28);
    const size_t hdr = 30u + nlen + xlen;
    if (hdr > len_ - en.local_off) return false;
    const size_t data_off = en.local_off + hdr;
    if (en.csize > len_ - data_off) return false;
    const uint8_t* src = data_ + data_off;

    if (en.method == 0) {                      // stored
        if (en.usize > en.csize) return false;
        out.assign(reinterpret_cast<const char*>(src), en.usize);
        return true;
    }
    if (en.method == 8) {                      // deflate
        if (en.usize > kMaxEntryBytes) return false;
        out.resize(en.usize);
        if (en.usize == 0) return true;
        size_t got = tinfl_decompress_mem_to_mem(
            out.data(), en.usize, src, en.csize,
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        if (got == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) { out.clear(); return false; }
        out.resize(got);
        return true;
    }
    return false;  // unsupported method
}

bool ZipReader::extract(const char* name, std::string& out) const
{
    int i = indexOf(name);
    return i >= 0 && extract(size_t(i), out);
}

}  // namespace book
