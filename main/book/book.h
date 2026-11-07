/*
 * Neutral document model shared by every book format.
 *
 * A parser turns a file into an ordered stream of TextRuns; the paginator and
 * renderer consume that stream and know nothing about txt / epub / xhtml. This
 * header is deliberately free of ESP-IDF so `book/` compiles on the host for
 * unit tests (see tests/).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace book {

// Inline style, OR-able.
enum Style : uint8_t {
    kBold   = 1u << 0,
    kItalic = 1u << 1,
};

// One run of same-styled text with no hard line breaks inside it. A block
// (paragraph, heading, list item, quote) is one or more consecutive runs; the
// first run of a block has `block_start = true`.
struct TextRun {
    std::string text;              // UTF-8; empty when `rule` is set
    uint8_t     style   = 0;       // Style bitmask
    uint8_t     heading = 0;       // 0 = body, 1..6 = <h1>..<h6>
    bool        block_start = false;
    bool        rule = false;      // horizontal divider; `text` is ignored
};

class Book {
public:
    virtual ~Book() = default;

    virtual const std::string& title() const = 0;

    virtual size_t runCount() const = 0;
    virtual const TextRun& run(size_t i) const = 0;

    // Reading position is stored as a Unicode-scalar offset into the whole
    // document so it survives font / size changes. charOffsetOf(runCount())
    // == totalChars().
    virtual size_t charOffsetOf(size_t runIndex) const = 0;
    virtual size_t totalChars() const = 0;
};

// Concrete Book backed by an owned vector of runs. Parsers build one of these.
class MemoryBook : public Book {
public:
    explicit MemoryBook(std::string title) : title_(std::move(title)) {}

    void addRun(TextRun r)
    {
        offsets_.push_back(total_chars_);
        total_chars_ += scalarLen(r.text);
        runs_.push_back(std::move(r));
    }

    const std::string& title() const override { return title_; }
    size_t runCount() const override { return runs_.size(); }
    const TextRun& run(size_t i) const override { return runs_[i]; }
    size_t charOffsetOf(size_t i) const override
    {
        return i < offsets_.size() ? offsets_[i] : total_chars_;
    }
    size_t totalChars() const override { return total_chars_; }

    // Count of UTF-8 scalars in a byte string (continuation bytes excluded).
    static size_t scalarLen(const std::string& s)
    {
        size_t n = 0;
        for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
        return n;
    }

private:
    std::string           title_;
    std::vector<TextRun>   runs_;
    std::vector<size_t>    offsets_;
    size_t                 total_chars_ = 0;
};

}  // namespace book
