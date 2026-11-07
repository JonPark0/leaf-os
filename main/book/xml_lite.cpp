#include "xml_lite.h"

#include <cstdlib>
#include <cstring>

namespace book {

const std::string XmlLite::kEmpty;

namespace {
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool isNameChar(char c)
{
    return !isSpace(c) && c != '>' && c != '/' && c != '=' && c != '<';
}

void appendUtf8(std::string& out, unsigned cp)
{
    if (cp < 0x80) out.push_back(char(cp));
    else if (cp < 0x800) {
        out.push_back(char(0xC0 | (cp >> 6)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(char(0xE0 | (cp >> 12)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(char(0xF0 | (cp >> 18)));
        out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(char(0x80 | (cp & 0x3F)));
    }
}
}  // namespace

void XmlLite::skipSpace() { while (p_ < end_ && isSpace(*p_)) ++p_; }

bool XmlLite::startsWith(const char* s) const
{
    size_t n = std::strlen(s);
    return (size_t)(end_ - p_) >= n && std::memcmp(p_, s, n) == 0;
}

std::string XmlLite::decodeEntities(const char* b, const char* e) const
{
    std::string out;
    out.reserve(e - b);
    for (const char* q = b; q < e; ) {
        if (*q != '&') { out.push_back(*q++); continue; }
        const char* semi = q + 1;
        while (semi < e && *semi != ';' && (semi - q) < 12) ++semi;
        if (semi >= e || *semi != ';') { out.push_back(*q++); continue; }
        std::string ent(q + 1, semi);
        if (ent == "lt") out.push_back('<');
        else if (ent == "gt") out.push_back('>');
        else if (ent == "amp") out.push_back('&');
        else if (ent == "quot") out.push_back('"');
        else if (ent == "apos") out.push_back('\'');
        else if (ent == "nbsp") out.push_back(' ');
        else if (!ent.empty() && ent[0] == '#') {
            unsigned cp = 0;
            if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
                cp = (unsigned)strtoul(ent.c_str() + 2, nullptr, 16);
            else
                cp = (unsigned)strtoul(ent.c_str() + 1, nullptr, 10);
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
            if (cp) appendUtf8(out, cp);
        } else {
            out.push_back('&'); out += ent; out.push_back(';');  // unknown, keep
        }
        q = semi + 1;
    }
    return out;
}

XmlEvent XmlLite::next()
{
    attrs_.clear();
    name_.clear();
    text_.clear();
    self_closing_ = false;

    for (;;) {
        if (p_ >= end_) return XmlEvent::End;

        if (*p_ != '<') {
            // Text node up to the next '<'.
            const char* b = p_;
            while (p_ < end_ && *p_ != '<') ++p_;
            text_ = decodeEntities(b, p_);
            return XmlEvent::Text;
        }

        // Something starting with '<'.
        if (startsWith("<!--")) {
            p_ += 4;
            while (p_ < end_ && !startsWith("-->")) ++p_;
            if (p_ < end_) p_ += 3;
            continue;
        }
        if (startsWith("<![CDATA[")) {
            p_ += 9;
            const char* b = p_;
            while (p_ < end_ && !startsWith("]]>")) ++p_;
            text_.assign(b, p_);
            if (p_ < end_) p_ += 3;
            return XmlEvent::Text;
        }
        if (startsWith("<?")) {
            p_ += 2;
            while (p_ < end_ && !startsWith("?>")) ++p_;
            if (p_ < end_) p_ += 2;
            continue;
        }
        if (startsWith("<!")) {   // DOCTYPE or similar
            p_ += 2;
            int depth = 1;
            while (p_ < end_ && depth) {
                if (*p_ == '<') ++depth;
                else if (*p_ == '>') --depth;
                ++p_;
            }
            continue;
        }

        // End tag?
        if (startsWith("</")) {
            p_ += 2;
            const char* b = p_;
            while (p_ < end_ && isNameChar(*p_)) ++p_;
            name_.assign(b, p_);
            while (p_ < end_ && *p_ != '>') ++p_;
            if (p_ < end_) ++p_;
            return XmlEvent::EndElement;
        }

        // Start tag.
        ++p_;  // consume '<'
        const char* b = p_;
        while (p_ < end_ && isNameChar(*p_)) ++p_;
        name_.assign(b, p_);

        // Attributes.
        for (;;) {
            skipSpace();
            if (p_ >= end_) break;
            if (*p_ == '/') { self_closing_ = true; ++p_; skipSpace(); }
            if (p_ < end_ && *p_ == '>') { ++p_; break; }
            if (p_ >= end_ || !isNameChar(*p_)) {
                // malformed; bail out of the tag
                while (p_ < end_ && *p_ != '>') ++p_;
                if (p_ < end_) ++p_;
                break;
            }
            const char* ab = p_;
            while (p_ < end_ && isNameChar(*p_) && *p_ != '=') ++p_;
            XmlAttr a;
            a.name.assign(ab, p_);
            skipSpace();
            if (p_ < end_ && *p_ == '=') {
                ++p_;
                skipSpace();
                if (p_ < end_ && (*p_ == '"' || *p_ == '\'')) {
                    char q = *p_++;
                    const char* vb = p_;
                    while (p_ < end_ && *p_ != q) ++p_;
                    a.value = decodeEntities(vb, p_);
                    if (p_ < end_) ++p_;
                }
            }
            attrs_.push_back(std::move(a));
        }
        return XmlEvent::StartElement;
    }
}

const std::string& XmlLite::attr(const char* n) const
{
    for (const auto& a : attrs_) if (a.name == n) return a.value;
    return kEmpty;
}

std::string XmlLite::localName(const std::string& q)
{
    size_t c = q.find(':');
    return c == std::string::npos ? q : q.substr(c + 1);
}

}  // namespace book
