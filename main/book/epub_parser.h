/*
 * EPUB (.epub) -> Book.
 *
 *   ZIP -> META-INF/container.xml -> OPF (metadata + manifest + spine)
 *       -> each spine XHTML doc -> xhtml_parser -> concatenated TextRun stream
 *
 * Structured text + basic formatting; body images become "[그림]" placeholders.
 * DRM-free EPUB 2/3 only. Needs zip_reader, so it builds under ESP-IDF only.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "book.h"

namespace book {

std::unique_ptr<Book> parseEpub(const uint8_t* data, size_t len);

}  // namespace book
