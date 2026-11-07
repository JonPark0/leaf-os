/*
 * SD font discovery for /sd/fonts/<script>-<family>-<px>.m5f.
 *
 * `script` is a language script tag: ko | ja | sc | tc (matches a language
 * pack's "# font" value). `family` is a face within that script:
 *   ko : gothic (default) | myeongjo | pen
 *   ja/sc/tc : sans (default) | serif
 *
 * Back-compat: "<script>-<px>.m5f" is the script's default family, and the
 * legacy "reader-<px>.m5f" is "ko-gothic-<px>". FATFS hands short names back
 * upper-cased, so all matching is case-insensitive.
 *
 * The system (UI) font and the reader body font each resolve independently
 * through pickSdFont(); main.cpp and app_reader share this module instead of
 * carrying their own filename parsers.
 */
#pragma once

#include <string>

namespace gfx {

struct SdFont {
    char script[4] = "";     // "ko" / "ja" / "sc" / "tc"
    char family[12] = "";     // canonical face, e.g. "gothic"; never empty here
    int  px = 0;              // confirmed from the .m5f header
    std::string path;
};

// Default face for a script ("gothic" for ko, "sans" otherwise).
const char* defaultFamilyFor(const char* script);

// Read a .m5f header's pixel size (u16 LE at offset 14); 0 on any failure.
int m5fHeaderPx(const char* path);

// Enumerate `dir` (e.g. "/sd/fonts"); fills `out` with up to `max` entries.
// Returns the count. Non-matching / unreadable files are skipped.
int scanSdFonts(const char* dir, SdFont* out, int max);

// Best match for (script, family, px). `family` may be "" (script default).
// Fallback order: exact family + nearest px -> default family + nearest px ->
// any family of the script + nearest px -> nullptr.
const SdFont* pickSdFont(const SdFont* list, int n,
                         const char* script, const char* family, int px);

// Canonical family names present for `script`, in display order
// (gothic, myeongjo, pen, sans, serif). Always lists the script default first
// when any file of that script exists. Returns the count written to `out`.
int familiesFor(const SdFont* list, int n, const char* script,
                const char** out, int max);

// Distinct px sizes present for (script, family), ascending. `family` "" =
// script default. Returns the count written to `out`.
int sizesFor(const SdFont* list, int n, const char* script, const char* family,
             int* out, int max);

}  // namespace gfx
