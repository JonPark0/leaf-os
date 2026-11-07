#include "xhtml_parser.h"

#include <cstring>
#include <string>
#include <vector>

#include "xml_lite.h"

namespace book {

namespace {
std::string s_img_placeholder = "[image]";
}

void setImagePlaceholder(const char* s) { s_img_placeholder = (s && *s) ? s : "[image]"; }

namespace {

bool eq(const std::string& q, const char* local)
{
    // case-insensitive compare of the local part
    std::string l = XmlLite::localName(q);
    if (l.size() != std::strlen(local)) return false;
    for (size_t i = 0; i < l.size(); ++i)
        if ((l[i] | 0x20) != (local[i] | 0x20)) return false;
    return true;
}

bool isBlock(const std::string& q)
{
    return eq(q, "p") || eq(q, "div") || eq(q, "li") || eq(q, "blockquote") ||
           eq(q, "pre") || eq(q, "section") || eq(q, "article") ||
           eq(q, "figcaption") || eq(q, "dd") || eq(q, "dt") ||
           eq(q, "h1") || eq(q, "h2") || eq(q, "h3") ||
           eq(q, "h4") || eq(q, "h5") || eq(q, "h6");
}

int headingLevel(const std::string& q)
{
    std::string l = XmlLite::localName(q);
    if (l.size() == 2 && (l[0] | 0x20) == 'h' && l[1] >= '1' && l[1] <= '6')
        return l[1] - '0';
    return 0;
}

// One block being assembled: a list of (text, style) segments plus its heading.
struct Block {
    struct Seg { std::string text; uint8_t style; };
    std::vector<Seg> segs;
    uint8_t heading = 0;
    bool started = false;

    void addText(const std::string& t, uint8_t style)
    {
        if (t.empty()) return;
        if (!segs.empty() && segs.back().style == style) segs.back().text += t;
        else segs.push_back({t, style});
        started = true;
    }
    bool empty() const
    {
        for (auto& s : segs)
            if (s.text.find_first_not_of(" \n") != std::string::npos) return false;
        return true;
    }
    void clear() { segs.clear(); heading = 0; started = false; }
};

void trimSegs(Block& b)
{
    // trim leading space of first seg, trailing of last
    if (b.segs.empty()) return;
    std::string& f = b.segs.front().text;
    size_t s = f.find_first_not_of(" \n");
    f = (s == std::string::npos) ? std::string() : f.substr(s);
    std::string& l = b.segs.back().text;
    size_t e = l.find_last_not_of(" \n");
    l = (e == std::string::npos) ? std::string() : l.substr(0, e + 1);
}

void flush(MemoryBook& out, Block& b)
{
    if (b.empty()) { b.clear(); return; }
    trimSegs(b);
    bool first = true;
    for (auto& seg : b.segs) {
        if (seg.text.empty()) continue;
        TextRun r;
        r.text = seg.text;
        r.style = seg.style;
        r.heading = b.heading;
        r.block_start = first;
        out.addRun(std::move(r));
        first = false;
    }
    b.clear();
}

// Append text with whitespace collapsed; keeps explicit '\n' from <br>.
void appendCollapsed(Block& b, const std::string& raw, uint8_t style, bool pre)
{
    std::string t;
    t.reserve(raw.size());
    bool sp = false;
    for (char c : raw) {
        if (!pre && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            sp = true;
            continue;
        }
        if (sp && !t.empty()) t.push_back(' ');
        sp = false;
        t.push_back(c);
    }
    if (sp && !t.empty() && !pre) t.push_back(' ');
    b.addText(t, style);
}

}  // namespace

void parseXhtmlInto(MemoryBook& out, const char* data, size_t len)
{
    XmlLite xml(data, len);

    Block blk;
    std::vector<uint8_t> style_stack{0};
    int skip_depth = 0;                 // inside <head>/<script>/<style>
    std::string skip_tag;
    bool in_pre = false;

    auto curStyle = [&] { return style_stack.back(); };

    for (;;) {
        XmlEvent ev = xml.next();
        if (ev == XmlEvent::End) break;

        if (skip_depth > 0) {
            if (ev == XmlEvent::StartElement && !xml.selfClosing() &&
                eq(xml.name(), skip_tag.c_str()))
                ++skip_depth;
            else if (ev == XmlEvent::EndElement && eq(xml.name(), skip_tag.c_str()))
                --skip_depth;
            continue;
        }

        if (ev == XmlEvent::Text) {
            appendCollapsed(blk, xml.text(), curStyle(), in_pre);
            continue;
        }

        if (ev == XmlEvent::StartElement) {
            const std::string& n = xml.name();

            if (eq(n, "head") || eq(n, "script") || eq(n, "style")) {
                if (!xml.selfClosing()) { skip_depth = 1; skip_tag = XmlLite::localName(n); }
                continue;
            }
            if (eq(n, "br")) { blk.addText("\n", curStyle()); continue; }
            if (eq(n, "hr")) {
                flush(out, blk);
                TextRun r; r.rule = true; r.block_start = true;
                out.addRun(std::move(r));
                continue;
            }
            if (eq(n, "img") || eq(n, "image")) {
                blk.addText(s_img_placeholder.c_str(), curStyle() | kItalic);
                continue;
            }
            if (eq(n, "strong") || eq(n, "b")) {
                style_stack.push_back(curStyle() | kBold);
                continue;
            }
            if (eq(n, "em") || eq(n, "i")) {
                style_stack.push_back(curStyle() | kItalic);
                continue;
            }
            if (isBlock(n)) {
                flush(out, blk);
                blk.heading = static_cast<uint8_t>(headingLevel(n));
                if (eq(n, "pre")) in_pre = true;
                continue;
            }
            // other inline elements: transparent
            if (!xml.selfClosing()) style_stack.push_back(curStyle());
            continue;
        }

        if (ev == XmlEvent::EndElement) {
            const std::string& n = xml.name();
            if (eq(n, "strong") || eq(n, "b") || eq(n, "em") || eq(n, "i")) {
                if (style_stack.size() > 1) style_stack.pop_back();
                continue;
            }
            if (isBlock(n)) {
                flush(out, blk);
                if (eq(n, "pre")) in_pre = false;
                continue;
            }
            if (style_stack.size() > 1) style_stack.pop_back();
        }
    }
    flush(out, blk);
}

}  // namespace book
