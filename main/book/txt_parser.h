/*
 * Plain-text (.txt) -> Book.
 *
 * Encoding: a UTF-8 / UTF-16 BOM is honoured (UTF-16 is transcoded); otherwise
 * the bytes are treated as UTF-8. Legacy single-byte / multibyte codecs
 * (CP949, EUC-JP, GBK) are handled by a separate transcode step fed from SD
 * `/codecs/` in a later phase — this parser stays UTF-8-only, matching the
 * "optional language support" model in the plan.
 *
 * Blocking: a blank line (two or more newlines) starts a new paragraph. A
 * single newline inside a paragraph is treated as a soft space, which is right
 * for the hard-wrapped .txt files most e-books ship as. `--- ` / `***` lines
 * become a horizontal rule.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "book.h"

namespace book {

std::unique_ptr<Book> parseTxt(const uint8_t* data, size_t len,
                               std::string title = "");

// Convenience for tests / host tools.
inline std::unique_ptr<Book> parseTxt(const std::string& s, std::string title = "")
{
    return parseTxt(reinterpret_cast<const uint8_t*>(s.data()), s.size(),
                    std::move(title));
}

}  // namespace book
