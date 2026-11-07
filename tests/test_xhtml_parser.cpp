#include "check.h"
#include "xhtml_parser.h"
#include "xml_lite.h"

using namespace book;

// --- xml_lite ---------------------------------------------------------------

TEST(xml_elements_and_text)
{
    XmlLite x("<a x=\"1\"><b>hi</b></a>", 21);
    EXPECT_TRUE(x.next() == XmlEvent::StartElement);
    EXPECT_STREQ(x.name(), "a");
    EXPECT_STREQ(x.attr("x"), "1");
    EXPECT_TRUE(x.next() == XmlEvent::StartElement);
    EXPECT_STREQ(x.name(), "b");
    EXPECT_TRUE(x.next() == XmlEvent::Text);
    EXPECT_STREQ(x.text(), "hi");
    EXPECT_TRUE(x.next() == XmlEvent::EndElement);
    EXPECT_STREQ(x.name(), "b");
    EXPECT_TRUE(x.next() == XmlEvent::EndElement);
    EXPECT_TRUE(x.next() == XmlEvent::End);
}

TEST(xml_self_closing_and_entities)
{
    XmlLite x("<p>A &amp; B<br/>C&#65;</p>", 26);
    x.next();  // <p>
    EXPECT_TRUE(x.next() == XmlEvent::Text);
    EXPECT_STREQ(x.text(), "A & B");
    EXPECT_TRUE(x.next() == XmlEvent::StartElement);
    EXPECT_STREQ(x.name(), "br");
    EXPECT_TRUE(x.selfClosing());
    EXPECT_TRUE(x.next() == XmlEvent::Text);
    EXPECT_STREQ(x.text(), "CA");
}

TEST(xml_comment_pi_cdata_skipped)
{
    const char* s = "<?xml v=\"1\"?><!-- c --><r><![CDATA[<x>]]></r>";
    XmlLite x(s, (size_t)__builtin_strlen(s));
    EXPECT_TRUE(x.next() == XmlEvent::StartElement);
    EXPECT_STREQ(x.name(), "r");
    EXPECT_TRUE(x.next() == XmlEvent::Text);
    EXPECT_STREQ(x.text(), "<x>");
}

TEST(xml_localname)
{
    EXPECT_STREQ(XmlLite::localName("opf:item"), "item");
    EXPECT_STREQ(XmlLite::localName("item"), "item");
}

// --- xhtml_parser ---------------------------------------------------------

TEST(xhtml_paragraphs_and_heading)
{
    MemoryBook b("t");
    const char* h = "<html><body><h1>Title</h1><p>First para.</p>"
                    "<p>Second\n  para.</p></body></html>";
    parseXhtmlInto(b, h, __builtin_strlen(h));
    EXPECT_EQ(b.runCount(), 3u);
    EXPECT_STREQ(b.run(0).text, "Title");
    EXPECT_EQ(b.run(0).heading, 1u);
    EXPECT_STREQ(b.run(1).text, "First para.");
    EXPECT_STREQ(b.run(2).text, "Second para.");   // whitespace collapsed
}

TEST(xhtml_inline_bold_italic_split_runs)
{
    MemoryBook b("t");
    const char* h = "<p>plain <strong>bold</strong> and <em>it</em> end</p>";
    parseXhtmlInto(b, h, __builtin_strlen(h));
    // one block -> multiple runs, only the first has block_start
    EXPECT_EQ(b.runCount(), 5u);
    EXPECT_STREQ(b.run(0).text, "plain ");
    EXPECT_TRUE(b.run(0).block_start);
    EXPECT_EQ(b.run(1).style, (unsigned)kBold);
    EXPECT_STREQ(b.run(1).text, "bold");
    EXPECT_TRUE(!b.run(1).block_start);
    EXPECT_EQ(b.run(3).style, (unsigned)kItalic);
    EXPECT_STREQ(b.run(3).text, "it");
}

TEST(xhtml_hr_and_img)
{
    MemoryBook b("t");
    const char* h = "<p>a</p><hr/><p>b<img src=\"x\"/></p>";
    parseXhtmlInto(b, h, __builtin_strlen(h));
    EXPECT_EQ(b.runCount(), 4u);
    EXPECT_TRUE(b.run(1).rule);
    EXPECT_STREQ(b.run(3).text, "[image]");   // default; the reader sets tr(READER_IMG)
}

TEST(xhtml_img_placeholder_override)
{
    setImagePlaceholder("<IMG>");
    MemoryBook b("t");
    const char* h = "<p><img src=\"x\"/></p>";
    parseXhtmlInto(b, h, __builtin_strlen(h));
    EXPECT_STREQ(b.run(0).text, "<IMG>");
    setImagePlaceholder(nullptr);             // back to "[image]" for later tests
}

TEST(xhtml_head_script_style_dropped)
{
    MemoryBook b("t");
    const char* h = "<html><head><title>x</title></head>"
                    "<body><script>var a=1;</script><p>visible</p>"
                    "<style>p{}</style></body></html>";
    parseXhtmlInto(b, h, __builtin_strlen(h));
    EXPECT_EQ(b.runCount(), 1u);
    EXPECT_STREQ(b.run(0).text, "visible");
}
