#include "txt_parser.h"

#include <algorithm>
#include <cctype>

namespace book {
namespace {

void appendUtf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Decode `data` into a UTF-8 std::string, honouring a BOM. Unknown bytes in the
// no-BOM (assumed UTF-8) path are kept as-is; a validator/scrubber can come
// later. UTF-16 surrogate pairs are combined.
std::string toUtf8(const uint8_t* data, size_t len)
{
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return std::string(reinterpret_cast<const char*>(data + 3), len - 3);
    }
    auto decodeUtf16 = [&](bool big) {
        std::string out;
        out.reserve(len);
        for (size_t i = 2; i + 1 < len; i += 2) {
            uint32_t u = big ? (data[i] << 8) | data[i + 1]
                             : (data[i + 1] << 8) | data[i];
            if (u >= 0xD800 && u <= 0xDBFF && i + 3 < len) {
                uint32_t lo = big ? (data[i + 2] << 8) | data[i + 3]
                                  : (data[i + 3] << 8) | data[i + 2];
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                    i += 2;
                }
            }
            appendUtf8(out, u);
        }
        return out;
    };
    if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE) return decodeUtf16(false);
    if (len >= 2 && data[0] == 0xFE && data[1] == 0xFF) return decodeUtf16(true);

    return std::string(reinterpret_cast<const char*>(data), len);
}

void rtrim(std::string& s)
{
    while (!s.empty()) {
        unsigned char c = s.back();
        if (c == ' ' || c == '\t') s.pop_back();
        else break;
    }
}

bool isRuleLine(const std::string& line)
{
    std::string t;
    for (char c : line) if (c != ' ' && c != '\t') t.push_back(c);
    if (t.size() < 3) return false;
    char c0 = t[0];
    if (c0 != '-' && c0 != '*' && c0 != '_' && c0 != '=') return false;
    return std::all_of(t.begin(), t.end(), [c0](char c) { return c == c0; });
}

}  // namespace

std::unique_ptr<Book> parseTxt(const uint8_t* data, size_t len, std::string title)
{
    auto bk = std::make_unique<MemoryBook>(std::move(title));

    const std::string text = toUtf8(data, len);

    // Split into lines on \n, dropping \r.
    std::vector<std::string> lines;
    std::string cur;
    for (char ch : text) {
        if (ch == '\r') continue;
        if (ch == '\n') { lines.push_back(std::move(cur)); cur.clear(); }
        else cur.push_back(ch);
    }
    lines.push_back(std::move(cur));

    // Accumulate lines into paragraphs; a blank line flushes.
    std::string para;
    auto flushPara = [&]() {
        rtrim(para);
        if (!para.empty()) {
            TextRun r;
            r.text = para;
            r.block_start = true;
            bk->addRun(std::move(r));
        }
        para.clear();
    };

    for (std::string& ln : lines) {
        // strip a leading BOM that survived on the first line only
        bool blank = ln.find_first_not_of(" \t") == std::string::npos;
        if (blank) { flushPara(); continue; }
        if (isRuleLine(ln)) {
            flushPara();
            TextRun r;
            r.rule = true;
            r.block_start = true;
            bk->addRun(std::move(r));
            continue;
        }
        if (!para.empty()) para.push_back(' ');
        // trim leading whitespace of a continuation line
        size_t s = ln.find_first_not_of(" \t");
        para += (s == std::string::npos) ? std::string() : ln.substr(s);
    }
    flushPara();

    return bk;
}

}  // namespace book
