#include "manifest.h"

#include <cctype>
#include <cstdlib>

#include "kv.h"

namespace appstore {
namespace {

std::string lower(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parseInt(const std::string& s, int& out)
{
    if (s.empty() || s.size() > 6) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    out = std::atoi(s.c_str());
    return true;
}

std::vector<std::string> splitList(const std::string& s)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t comma = s.find(',', start);
        if (comma == std::string::npos) comma = s.size();
        std::string item = lower(core::kv::trim(s.substr(start, comma - start)));
        if (!item.empty()) out.push_back(item);
        start = comma + 1;
    }
    return out;
}

}  // namespace

bool parseVersion(const std::string& s, Version& out)
{
    int parts[3];
    size_t pos = 0;
    for (int i = 0; i < 3; ++i) {
        size_t end = pos;
        while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end]))) ++end;
        if (end == pos || end - pos > 5) return false;
        parts[i] = std::atoi(s.substr(pos, end - pos).c_str());
        if (i < 2) {
            if (end >= s.size() || s[end] != '.') return false;
            pos = end + 1;
        } else if (end != s.size()) {
            return false;
        }
    }
    out = {parts[0], parts[1], parts[2]};
    return true;
}

int compareVersion(const Version& a, const Version& b)
{
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

AppManifest parseManifest(const std::string& text)
{
    AppManifest m;
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        const std::string line = core::kv::trim(text.substr(start, nl - start));
        start = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        std::string k, v;
        if (!core::kv::split(line, k, v)) continue;
        if      (k == "name")       m.name = v;
        else if (k == "name_en")    m.name_en = v;
        else if (k == "summary")    m.summary = v;
        else if (k == "summary_en") m.summary_en = v;
        else if (k == "author")     m.author = v;
        else if (k == "icon")       m.icon = v;
        else if (k == "entry")      { if (!v.empty()) m.entry = v; }
        else if (k == "order")      { int n; if (parseInt(v, n)) m.order = n; }
        else if (k == "api")        { int n; if (parseInt(v, n) && n > 0) m.api = n; }
        else if (k == "net")        m.net = splitList(v);
        else if (k == "version") {
            m.version_str = v;
            m.has_version = parseVersion(v, m.version);
        }
    }
    return m;
}

bool validId(const std::string& id)
{
    if (id.empty() || id.size() > 32 || id[0] == '-') return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

bool validHost(const std::string& host)
{
    if (host.empty() || host.size() > 253) return false;
    size_t label = 0;
    for (size_t i = 0; i <= host.size(); ++i) {
        const char c = i < host.size() ? host[i] : '.';
        if (c == '.') {
            if (label == 0 || label > 63) return false;
            if (host[i - 1] == '-' || host[i - label] == '-') return false;
            label = 0;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
            ++label;
        } else {
            return false;
        }
    }
    return true;
}

bool validRelPath(const std::string& path)
{
    if (path.empty() || path.size() > 96) return false;
    int slashes = 0;
    size_t seg = 0;   // start of the current segment
    for (size_t i = 0; i <= path.size(); ++i) {
        const char c = i < path.size() ? path[i] : '/';
        if (c == '/') {
            if (i == seg) return false;          // empty segment (leading '/', "//")
            if (path[seg] == '.') return false;  // ".", "..", hidden files
            if (i < path.size()) ++slashes;
            seg = i + 1;
        } else if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) {
            return false;
        }
    }
    return slashes <= 2;
}

}  // namespace appstore
