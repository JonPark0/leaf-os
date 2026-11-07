#include "lang_pack.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <string>

#include <esp_log.h>

#include "i18n.h"
#include "kv.h"

namespace core {
namespace {
constexpr char TAG[] = "lang_pack";

using core::kv::trim;
using core::kv::split;

// Owned storage for the active pack. s_ptr is what i18n::setPack() points at.
std::string s_val[i18n::MSG_COUNT];
const char* s_ptr[i18n::MSG_COUNT];
std::string s_font;
int s_ui_pad = -1;   // "# ui_pad" (0/1), -1 if the pack didn't set it

// The printf conversions in `s`, in order ("%04d %s %%" -> "ds"). Pack strings
// are fed to snprintf as the format, so one whose conversions differ from the
// built-in (a stray %s, or %n) would read/write the wrong varargs.
std::string fmtSignature(const char* s)
{
    std::string sig;
    for (const char* p = s; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        while (*p && std::strchr("-+ #0123456789.lhz", *p)) ++p;
        if (!*p) { sig.push_back('!'); break; }
        if (*p != '%') sig.push_back(*p);
    }
    return sig;
}

FILE* openPack(const char* code)
{
    char path[64];
    std::snprintf(path, sizeof(path), "/sd/lang/%s.lang", code);
    return std::fopen(path, "r");
}

}  // namespace

bool loadLangPack(const char* code)
{
    if (code == nullptr || code[0] == '\0') return false;
    FILE* f = openPack(code);
    if (f == nullptr) { ESP_LOGW(TAG, "no /sd/lang/%s.lang", code); return false; }

    for (auto& v : s_val) v.clear();
    s_font.clear();
    s_ui_pad = -1;

    char buf[512];
    int hits = 0;
    while (std::fgets(buf, sizeof(buf), f) != nullptr) {
        std::string line = trim(buf);
        if (line.empty()) continue;

        if (line[0] == '#') {
            std::string k, v;
            if (split(line.substr(1), k, v)) {
                if (k == "font") s_font = v;
                else if (k == "ui_pad") s_ui_pad = (v == "1" || v == "on") ? 1 : 0;
            }
            continue;
        }
        std::string k, v;
        if (!split(line, k, v)) continue;
        const i18n::Msg m = i18n::msgFromName(k.c_str());
        if (m == i18n::MSG_COUNT) { ESP_LOGW(TAG, "unknown key '%s'", k.c_str()); continue; }
        const bool isFmt = k.size() > 4 && k.compare(k.size() - 4, 4, "_FMT") == 0;
        if (isFmt && fmtSignature(v.c_str()) != fmtSignature(i18n::builtinEn(m))) {
            ESP_LOGW(TAG, "'%s': format specifiers differ from built-in, ignored", k.c_str());
            continue;
        }
        s_val[m] = v;
        ++hits;
    }
    std::fclose(f);

    if (hits == 0) { ESP_LOGW(TAG, "%s.lang has no usable keys", code); return false; }

    for (int i = 0; i < i18n::MSG_COUNT; ++i)
        s_ptr[i] = s_val[i].empty() ? nullptr : s_val[i].c_str();
    i18n::setPack(s_ptr);
    ESP_LOGI(TAG, "loaded %s.lang (%d strings, font='%s')", code, hits, s_font.c_str());
    return true;
}

const char* langPackFont() { return s_font.c_str(); }
int langPackUiPad() { return s_ui_pad; }

int listLangPacks(LangPackInfo* out, int max)
{
    DIR* d = ::opendir("/sd/lang");
    if (d == nullptr) {
        ESP_LOGW(TAG, "opendir(/sd/lang) failed (no folder / card?)");
        return 0;
    }

    int n = 0, seen = 0;
    struct dirent* ent;
    while ((ent = ::readdir(d)) != nullptr && n < max) {
        ++seen;
        const char* dot = std::strrchr(ent->d_name, '.');
        if (ent->d_name[0] == '.' || dot == nullptr || strcasecmp(dot, ".lang") != 0) {
            ESP_LOGD(TAG, "skip '%s'", ent->d_name);
            continue;
        }

        LangPackInfo& info = out[n];
        info.code.assign(ent->d_name, static_cast<size_t>(dot - ent->d_name));
        info.name = info.code;
        ESP_LOGI(TAG, "pack '%s'", info.code.c_str());

        FILE* f = openPack(info.code.c_str());
        if (f != nullptr) {
            char buf[256];
            while (std::fgets(buf, sizeof(buf), f) != nullptr) {
                std::string line = trim(buf);
                if (line.empty()) continue;
                if (line[0] != '#') break;   // metadata block ends at the first key
                std::string k, v;
                if (!split(line.substr(1), k, v) || v.empty()) continue;
                if (k == "name")         info.name = v;
                else if (k == "name_en") info.name_ascii = v;
                else if (k == "font")    info.font = v;
            }
            std::fclose(f);
        }
        ++n;
    }
    ::closedir(d);
    ESP_LOGI(TAG, "listLangPacks: %d entr(ies) scanned, %d pack(s)", seen, n);
    return n;
}

}  // namespace core
