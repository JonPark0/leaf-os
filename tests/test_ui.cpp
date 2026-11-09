#include "check.h"
#include "text_wrap.h"

using namespace ui::text;

namespace {

// 10 px per ASCII byte, 20 px per multi-byte scalar (a CJK-ish glyph).
int fakeWidth(const char* s, int len)
{
    int w = 0;
    for (int i = 0; i < len;) {
        const int n = u8len(static_cast<unsigned char>(s[i]));
        w += n == 1 ? 10 : 20;
        i += n;
    }
    return w;
}

const Measure kM = fakeWidth;

std::vector<std::string> texts(const std::vector<Line>& lines)
{
    std::vector<std::string> out;
    for (const Line& l : lines) out.push_back(l.gap ? "<gap>" : l.text);
    return out;
}

}  // namespace

TEST(wrap_keeps_short_lines)
{
    const auto out = texts(wrap({"hello", "", "world"}, 100, kM));
    EXPECT_EQ(out.size(), size_t(3));
    EXPECT_STREQ(out[0], "hello");
    EXPECT_STREQ(out[1], "<gap>");
    EXPECT_STREQ(out[2], "world");
}

TEST(wrap_breaks_after_spaces)
{
    // 10 chars per line.
    const auto out = texts(wrap({"aaa bbb ccc ddd"}, 100, kM));
    EXPECT_EQ(out.size(), size_t(2));
    EXPECT_STREQ(out[0], "aaa bbb");
    EXPECT_STREQ(out[1], "ccc ddd");
}

TEST(wrap_splits_a_long_word)
{
    const auto out = texts(wrap({"abcdefghijklmno"}, 100, kM));
    EXPECT_EQ(out.size(), size_t(2));
    EXPECT_STREQ(out[0], "abcdefghij");
    EXPECT_STREQ(out[1], "klmno");
}

TEST(wrap_cjk_per_scalar)
{
    // Five 20 px scalars per 100 px line; never split inside a scalar.
    const std::string s = "가나다라마바사";   // 7 x 3 bytes
    const auto out = texts(wrap({s}, 100, kM));
    EXPECT_EQ(out.size(), size_t(2));
    EXPECT_STREQ(out[0], "가나다라마");
    EXPECT_STREQ(out[1], "바사");
}

TEST(wrap_too_narrow_still_progresses)
{
    const auto out = texts(wrap({"가나"}, 5, kM));
    EXPECT_EQ(out.size(), size_t(2));
    EXPECT_STREQ(out[0], "가");
}

TEST(paginate_fills_pages)
{
    std::vector<Line> lines;
    for (int i = 0; i < 7; ++i) lines.push_back({"x", false});
    const auto starts = paginate(lines, 30, 15, 100);   // 3 lines per page
    EXPECT_EQ(starts.size(), size_t(3));
    EXPECT_EQ(starts[0], size_t(0));
    EXPECT_EQ(starts[1], size_t(3));
    EXPECT_EQ(starts[2], size_t(6));
}

TEST(paginate_drops_a_gap_at_the_top_of_a_page)
{
    const std::vector<Line> lines = {{"a", false}, {"b", false}, {"c", false},
                                     {"", true}, {"d", false}};
    const auto starts = paginate(lines, 30, 15, 90);
    EXPECT_EQ(starts.size(), size_t(2));
    EXPECT_EQ(starts[1], size_t(4));
}

TEST(paginate_empty_and_tall_lines)
{
    EXPECT_EQ(paginate({}, 30, 15, 100).size(), size_t(1));
    const std::vector<Line> lines = {{"a", false}, {"b", false}};
    EXPECT_EQ(paginate(lines, 120, 60, 100).size(), size_t(2));   // each line its own page
}

TEST(fit_suffix_keeps_the_end)
{
    EXPECT_EQ(fitSuffixStart("hello", 50, 30, kM), size_t(0));          // fits whole
    EXPECT_EQ(fitSuffixStart("/sd/books/novels", 100, 30, kM), size_t(9));  // "...novels"
    EXPECT_EQ(fitSuffixStart("가나다", 50, 10, kM), size_t(3));           // "..." + 나다
    EXPECT_EQ(fitSuffixStart("가나다", 5, 10, kM), size_t(9));            // nothing fits
}

TEST(fit_prefix_cuts_on_scalars)
{
    EXPECT_EQ(fitPrefix("hello", 50, 30, kM), size_t(5));        // fits whole
    EXPECT_EQ(fitPrefix("hello world", 60, 30, kM), size_t(3));  // "hel" + "..."
    EXPECT_EQ(fitPrefix("가나다", 50, 10, kM), size_t(6));        // two scalars + suffix
    EXPECT_EQ(fitPrefix("가나다", 5, 10, kM), size_t(0));
}
