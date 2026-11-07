#include "check.h"
#include "txt_parser.h"

using namespace book;

TEST(single_paragraph)
{
    auto b = parseTxt("Hello world.");
    EXPECT_EQ(b->runCount(), 1u);
    EXPECT_STREQ(b->run(0).text, "Hello world.");
    EXPECT_TRUE(b->run(0).block_start);
    EXPECT_EQ(b->run(0).heading, 0u);
}

TEST(blank_line_splits_paragraphs)
{
    auto b = parseTxt("First para.\n\nSecond para.\n\n\nThird.");
    EXPECT_EQ(b->runCount(), 3u);
    EXPECT_STREQ(b->run(1).text, "Second para.");
    EXPECT_STREQ(b->run(2).text, "Third.");
}

TEST(hard_wrap_joined_with_space)
{
    auto b = parseTxt("A hard-wrapped\nline of prose\nin one paragraph.\n\nNext.");
    EXPECT_EQ(b->runCount(), 2u);
    EXPECT_STREQ(b->run(0).text, "A hard-wrapped line of prose in one paragraph.");
}

TEST(crlf_normalised)
{
    auto b = parseTxt("one\r\n\r\ntwo\r\n");
    EXPECT_EQ(b->runCount(), 2u);
    EXPECT_STREQ(b->run(0).text, "one");
    EXPECT_STREQ(b->run(1).text, "two");
}

TEST(trailing_ws_trimmed_leading_ws_of_cont_line_dropped)
{
    auto b = parseTxt("line one   \n   line two");
    EXPECT_EQ(b->runCount(), 1u);
    EXPECT_STREQ(b->run(0).text, "line one line two");
}

TEST(rule_line)
{
    auto b = parseTxt("Before.\n\n----\n\nAfter.");
    EXPECT_EQ(b->runCount(), 3u);
    EXPECT_TRUE(b->run(1).rule);
    EXPECT_TRUE(b->run(1).text.empty());
    EXPECT_STREQ(b->run(2).text, "After.");
}

TEST(utf8_bom_stripped)
{
    std::string s = "\xEF\xBB\xBF" "café";
    auto b = parseTxt(s);
    EXPECT_EQ(b->runCount(), 1u);
    EXPECT_STREQ(b->run(0).text, "café");
}

TEST(utf16le_bom_transcoded)
{
    // "Hi" + newline*2 + "Bye" in UTF-16LE with BOM
    const unsigned char raw[] = {
        0xFF, 0xFE, 'H', 0, 'i', 0, '\n', 0, '\n', 0, 'B', 0, 'y', 0, 'e', 0,
    };
    auto b = parseTxt(raw, sizeof(raw));
    EXPECT_EQ(b->runCount(), 2u);
    EXPECT_STREQ(b->run(0).text, "Hi");
    EXPECT_STREQ(b->run(1).text, "Bye");
}

TEST(char_offsets_are_scalar_counts)
{
    auto b = parseTxt("café\n\nnoir");         // 4 scalars, 4 scalars
    EXPECT_EQ(b->charOffsetOf(0), 0u);
    EXPECT_EQ(b->charOffsetOf(1), 4u);
    EXPECT_EQ(b->totalChars(), 8u);
    EXPECT_EQ(b->charOffsetOf(b->runCount()), b->totalChars());
}

TEST(korean_paragraphs)
{
    auto b = parseTxt("첫 번째 문단입니다.\n\n두 번째 문단.");
    EXPECT_EQ(b->runCount(), 2u);
    EXPECT_STREQ(b->run(0).text, "첫 번째 문단입니다.");
    EXPECT_EQ(b->totalChars(), MemoryBook::scalarLen("첫 번째 문단입니다.")
                             + MemoryBook::scalarLen("두 번째 문단."));
}

TEST(empty_input)
{
    auto b = parseTxt("");
    EXPECT_EQ(b->runCount(), 0u);
    EXPECT_EQ(b->totalChars(), 0u);
}
