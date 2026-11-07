/*
 * Optional UI translation loaded from the SD card, on top of the built-in
 * 한국어 / English tables (core/i18n). A pack is /sd/lang/<code>.lang:
 *
 *     # code = ja
 *     # name = 日本語
 *     # font = ja           ; /sd/fonts/<font>-<px>.m5f  (script tag; see main.cpp)
 *     BACK  = ← 戻る
 *     SAVE  = 保存
 *     ...
 *
 * "KEY = value" lines are matched by i18n::msgFromName; unknown or omitted keys
 * fall through to the built-in string. Order does not matter.
 */
#pragma once

#include <string>

namespace core {

// Load /sd/lang/<code>.lang and install it via i18n::setPack(). Returns false
// on any failure (i18n left untouched). The parsed strings are held for the
// process lifetime.
bool loadLangPack(const char* code);

// The active pack's "# font" tag, or "" when none is loaded / it declared none.
const char* langPackFont();

// The active pack's "# ui_pad" (0/1) — extra list-row padding preference — or
// -1 when no pack is loaded / it declared none.
int langPackUiPad();

struct LangPackInfo {
    std::string code;         // file stem
    std::string name;         // "# name" (endonym), or the code if absent
    std::string name_ascii;   // "# name_en" — shown when the active font can't
                              // draw `name`; empty => fall back to `code`
    std::string font;         // "# font" script tag (ko|ja|sc|tc), "" if absent
};

// Enumerate /sd/lang/*.lang (metadata only). Returns the count written to `out`.
int listLangPacks(LangPackInfo* out, int max);

}  // namespace core
