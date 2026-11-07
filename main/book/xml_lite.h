/*
 * Minimal pull XML tokenizer — enough for EPUB's container.xml, OPF, NCX and
 * the XHTML content documents. Hardware-free (host-testable).
 *
 * Handles: elements + attributes (single/double quoted), self-closing tags,
 * text with entity decoding (&lt; &gt; &amp; &quot; &apos; &#dd; &#xhh;),
 * <!-- comments -->, <![CDATA[ ... ]]>, and skips <?...?> and <!DOCTYPE ...>.
 * Namespace prefixes are kept verbatim in names ("opf:item"); callers that
 * don't care can compare the local part via localName().
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace book {

enum class XmlEvent { StartElement, EndElement, Text, End };

struct XmlAttr { std::string name, value; };

class XmlLite {
public:
    XmlLite(const char* data, size_t len) : p_(data), end_(data + len) {}

    XmlEvent next();

    const std::string& name() const { return name_; }
    const std::vector<XmlAttr>& attrs() const { return attrs_; }
    const std::string& text() const { return text_; }
    bool selfClosing() const { return self_closing_; }

    // Attribute lookup (exact name); returns "" when absent.
    const std::string& attr(const char* n) const;

    // Local part of `name()` after any "prefix:".
    static std::string localName(const std::string& qname);

private:
    void skipSpace();
    bool startsWith(const char* s) const;
    std::string decodeEntities(const char* b, const char* e) const;

    const char* p_;
    const char* end_;
    std::string name_;
    std::string text_;
    std::vector<XmlAttr> attrs_;
    bool self_closing_ = false;
    static const std::string kEmpty;
};

}  // namespace book
