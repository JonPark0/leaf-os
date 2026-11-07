/*
 * (X)HTML content document -> book::TextRun stream.
 *
 * Structured text + basic inline formatting, per the plan:
 *   - blocks: <p> <div> <h1..6> <li> <blockquote> <pre>
 *   - inline: <strong>/<b> -> bold, <em>/<i> -> italic
 *   - <br> -> hard line break inside a block
 *   - <hr> -> a rule run
 *   - <img> -> a text placeholder run (see setImagePlaceholder)
 *   - <head>/<script>/<style> subtrees are dropped; whitespace is collapsed.
 *
 * Appends to a caller-owned MemoryBook so an EPUB's spine documents concatenate
 * into one book. Hardware-free (host-testable).
 */
#pragma once

#include <cstddef>

#include "book.h"

namespace book {

// Text substituted for <img>/<image>. Defaults to "[image]"; the reader sets a
// localised string. Kept here (not via core/i18n) so book/ stays hardware-free.
void setImagePlaceholder(const char* s);

void parseXhtmlInto(MemoryBook& out, const char* data, size_t len);

}  // namespace book
