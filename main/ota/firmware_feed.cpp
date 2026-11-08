#include "firmware_feed.h"

#include <cctype>
#include <tuple>

namespace ota {
namespace {

// Reads 1-5 digits at s[i], advancing i.
bool readNum(const std::string& s, size_t& i, int& out)
{
    const size_t start = i;
    out = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) && i - start < 5)
        out = out * 10 + (s[i++] - '0');
    return i > start && (i == s.size() || !std::isdigit(static_cast<unsigned char>(s[i])));
}

}  // namespace

bool parseFirmwareVersion(const std::string& s, FirmwareVersion& out)
{
    size_t i = 0;
    if (i < s.size() && s[i] == 'v') ++i;
    FirmwareVersion v;
    if (!readNum(s, i, v.major) || i >= s.size() || s[i++] != '.') return false;
    if (!readNum(s, i, v.minor) || i >= s.size() || s[i++] != '.') return false;
    if (!readNum(s, i, v.patch)) return false;
    if (i < s.size()) {
        if (s[i] != '-' || i + 1 == s.size()) return false;
        v.dev = true;
    }
    out = v;
    return true;
}

bool updateAvailable(const std::string& running, const std::string& latest)
{
    FirmwareVersion l, r;
    if (!parseFirmwareVersion(latest, l) || l.dev) return false;
    if (!parseFirmwareVersion(running, r)) return true;
    return std::tie(l.major, l.minor, l.patch) > std::tie(r.major, r.minor, r.patch);
}

bool validateFeed(const Feed& f, size_t slot_bytes, std::string& why)
{
    FirmwareVersion v;
    if (!parseFirmwareVersion(f.version, v) || v.dev) { why = "bad version"; return false; }
    if (f.size < kMinImageBytes)                       { why = "bad size"; return false; }
    if (f.size > slot_bytes)                           { why = "too large"; return false; }
    if (f.sha256.size() != 64)                         { why = "bad sha256"; return false; }
    for (char c : f.sha256)
        if (!std::isdigit(static_cast<unsigned char>(c)) && (c < 'a' || c > 'f')) {
            why = "bad sha256";
            return false;
        }
    if (f.file.empty() || f.file.size() > 64 || f.file[0] == '.') { why = "bad file"; return false; }
    for (char c : f.file)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-' && c != '_') {
            why = "bad file";
            return false;
        }
    return true;
}

}  // namespace ota
