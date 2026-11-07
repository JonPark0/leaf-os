#include "epub_parser.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "xhtml_parser.h"
#include "xml_lite.h"
#include "zip_reader.h"

namespace book {
namespace {

std::string dirOf(const std::string& path)
{
    size_t s = path.find_last_of('/');
    return s == std::string::npos ? std::string() : path.substr(0, s + 1);
}

// Resolve `rel` against `base` (a directory ending in '/' or empty) and
// collapse "." / "..". EPUB paths use '/'.
std::string resolve(const std::string& base, const std::string& rel)
{
    std::string p = rel.empty() || rel[0] == '/' ? rel : base + rel;
    std::vector<std::string> parts;
    size_t i = 0;
    while (i < p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        std::string seg = p.substr(i, j - i);
        if (seg == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (seg != "." && !seg.empty()) parts.push_back(seg);
        i = j + 1;
    }
    std::string out;
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k) out.push_back('/');
        out += parts[k];
    }
    return out;
}

bool loc(const std::string& q, const char* name)
{
    return XmlLite::localName(q) == name;
}

// container.xml -> OPF path
std::string findOpf(ZipReader& zip)
{
    std::string xml;
    if (!zip.extract("META-INF/container.xml", xml)) return {};
    XmlLite x(xml.data(), xml.size());
    for (;;) {
        XmlEvent e = x.next();
        if (e == XmlEvent::End) break;
        if (e == XmlEvent::StartElement && loc(x.name(), "rootfile")) {
            const std::string& fp = x.attr("full-path");
            if (!fp.empty()) return fp;
        }
    }
    return {};
}

}  // namespace

std::unique_ptr<Book> parseEpub(const uint8_t* data, size_t len)
{
    ZipReader zip;
    if (!zip.open(data, len)) return nullptr;

    const std::string opf_path = findOpf(zip);
    if (opf_path.empty()) return nullptr;

    std::string opf;
    if (!zip.extract(opf_path.c_str(), opf)) return nullptr;
    const std::string base = dirOf(opf_path);

    std::string title, creator;
    std::unordered_map<std::string, std::string> id_href;   // manifest id -> href
    std::unordered_map<std::string, std::string> id_type;
    std::vector<std::string> spine;                         // ordered idrefs

    {
        XmlLite x(opf.data(), opf.size());
        std::string pending_meta;   // which dc:* element we're inside
        for (;;) {
            XmlEvent e = x.next();
            if (e == XmlEvent::End) break;
            if (e == XmlEvent::StartElement) {
                const std::string& n = x.name();
                if (loc(n, "item")) {
                    const std::string& id = x.attr("id");
                    if (!id.empty()) {
                        id_href[id] = x.attr("href");
                        id_type[id] = x.attr("media-type");
                    }
                } else if (loc(n, "itemref")) {
                    const std::string& r = x.attr("idref");
                    if (!r.empty()) spine.push_back(r);
                } else if (loc(n, "title") || loc(n, "creator")) {
                    pending_meta = XmlLite::localName(n);
                }
            } else if (e == XmlEvent::Text && !pending_meta.empty()) {
                if (pending_meta == "title" && title.empty()) title = x.text();
                else if (pending_meta == "creator" && creator.empty()) creator = x.text();
            } else if (e == XmlEvent::EndElement) {
                if (loc(x.name(), "title") || loc(x.name(), "creator")) pending_meta.clear();
            }
        }
    }

    auto bk = std::make_unique<MemoryBook>(title.empty() ? "EPUB" : title);

    // Cover-ish front matter.
    if (!title.empty()) {
        TextRun t; t.text = title; t.heading = 1; t.block_start = true;
        bk->addRun(std::move(t));
    }
    if (!creator.empty()) {
        TextRun c; c.text = creator; c.style = kItalic; c.block_start = true;
        bk->addRun(std::move(c));
    }

    for (const std::string& idref : spine) {
        auto it = id_href.find(idref);
        if (it == id_href.end()) continue;
        auto tt = id_type.find(idref);
        if (tt != id_type.end() && tt->second.find("xhtml") == std::string::npos &&
            tt->second.find("html") == std::string::npos)
            continue;

        const std::string path = resolve(base, it->second);
        std::string doc;
        if (!zip.extract(path.c_str(), doc) || doc.empty()) continue;
        parseXhtmlInto(*bk, doc.data(), doc.size());
    }

    if (bk->runCount() == 0) return nullptr;
    return bk;
}

}  // namespace book
