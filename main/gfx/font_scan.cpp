#include "font_scan.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <strings.h>

namespace gfx {
namespace {

constexpr const char* kScripts[] = {"ko", "ja", "sc", "tc"};
// Display / cycle order; the script default sorts first via familiesFor().
constexpr const char* kFamilyOrder[] = {"gothic", "myeongjo", "pen", "sans", "serif"};

bool isScript(const char* s)
{
    for (const char* k : kScripts) if (strcasecmp(s, k) == 0) return true;
    return false;
}

bool allDigits(const char* s)
{
    if (!*s) return false;
    for (; *s; ++s) if (!std::isdigit(static_cast<unsigned char>(*s))) return false;
    return true;
}

// Parse "<script>-<family>-<px>", "<script>-<px>" or "reader-<px>" (stem, no
// extension). Returns false if it is not a recognised font name.
bool parseStem(const char* stem, char script[4], char family[12], int* px)
{
    char buf[64];
    std::strncpy(buf, stem, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char* p = buf; *p; ++p) *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));

    char* dash1 = std::strchr(buf, '-');
    if (!dash1) return false;
    *dash1 = '\0';
    const char* a = buf;
    char* rest = dash1 + 1;

    if (std::strcmp(a, "reader") == 0 && allDigits(rest)) {
        std::strcpy(script, "ko");
        std::strcpy(family, "gothic");
        *px = std::atoi(rest);
        return *px > 0;
    }
    if (!isScript(a)) return false;

    char* dash2 = std::strchr(rest, '-');
    if (dash2) {
        *dash2 = '\0';
        const char* fam = rest;
        const char* num = dash2 + 1;
        if (!allDigits(num) || !*fam || std::strlen(fam) >= 12) return false;
        std::strncpy(script, a, 3); script[3] = '\0';
        std::strcpy(family, fam);
        *px = std::atoi(num);
        return *px > 0;
    }
    if (!allDigits(rest)) return false;
    std::strncpy(script, a, 3); script[3] = '\0';
    std::strcpy(family, defaultFamilyFor(a));
    *px = std::atoi(rest);
    return *px > 0;
}

}  // namespace

const char* defaultFamilyFor(const char* script)
{
    return (script && strcasecmp(script, "ko") == 0) ? "gothic" : "sans";
}

int m5fHeaderPx(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (!f) return 0;
    uint8_t h[16];
    const size_t got = std::fread(h, 1, sizeof(h), f);
    std::fclose(f);
    if (got < 16 || std::memcmp(h, "M5F1", 4) != 0) return 0;
    return h[14] | (h[15] << 8);
}

int scanSdFonts(const char* dir, SdFont* out, int max)
{
    DIR* d = ::opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent* ent;
    while (n < max && (ent = ::readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        const char* dot = std::strrchr(ent->d_name, '.');
        if (!dot || strcasecmp(dot, ".m5f") != 0) continue;

        char stem[64];
        const size_t slen = static_cast<size_t>(dot - ent->d_name);
        if (slen >= sizeof(stem)) continue;
        std::memcpy(stem, ent->d_name, slen);
        stem[slen] = '\0';

        SdFont e;
        if (!parseStem(stem, e.script, e.family, &e.px)) continue;

        e.path = std::string(dir) + "/" + ent->d_name;
        const int hpx = m5fHeaderPx(e.path.c_str());
        if (hpx >= 8 && hpx <= 96) e.px = hpx;   // trust the header
        else if (e.px < 8 || e.px > 96) continue;

        out[n++] = std::move(e);
    }
    ::closedir(d);
    return n;
}

const SdFont* pickSdFont(const SdFont* list, int n,
                         const char* script, const char* family, int px)
{
    const char* want = (family && *family) ? family : defaultFamilyFor(script);
    const char* def  = defaultFamilyFor(script);

    const SdFont* best = nullptr;
    int best_rank = 99, best_dp = 1 << 30;
    for (int i = 0; i < n; ++i) {
        const SdFont& e = list[i];
        if (strcasecmp(e.script, script) != 0) continue;
        int rank;
        if (strcasecmp(e.family, want) == 0)      rank = 0;
        else if (strcasecmp(e.family, def) == 0)  rank = 1;
        else                                      rank = 2;
        const int dp = std::abs(e.px - px);
        if (rank < best_rank || (rank == best_rank && dp < best_dp)) {
            best_rank = rank; best_dp = dp; best = &e;
        }
    }
    return best;
}

int familiesFor(const SdFont* list, int n, const char* script,
                const char** out, int max)
{
    const char* def = defaultFamilyFor(script);
    bool seen_def = false;
    int count = 0;

    auto add = [&](const char* fam) {
        for (int i = 0; i < count; ++i) if (strcasecmp(out[i], fam) == 0) return;
        if (count < max) out[count++] = fam;
    };

    for (int i = 0; i < n; ++i)
        if (strcasecmp(list[i].script, script) == 0) { seen_def = true; break; }
    if (seen_def) add(def);

    for (const char* fam : kFamilyOrder) {
        if (strcasecmp(fam, def) == 0) continue;
        for (int i = 0; i < n; ++i) {
            if (strcasecmp(list[i].script, script) != 0) continue;
            if (strcasecmp(list[i].family, fam) == 0) { add(fam); break; }
        }
    }
    return count;
}

int sizesFor(const SdFont* list, int n, const char* script, const char* family,
             int* out, int max)
{
    const char* want = (family && *family) ? family : defaultFamilyFor(script);
    int count = 0;
    for (int i = 0; i < n; ++i) {
        const SdFont& e = list[i];
        if (strcasecmp(e.script, script) != 0) continue;
        if (strcasecmp(e.family, want) != 0) continue;
        bool dup = false;
        for (int j = 0; j < count; ++j) if (out[j] == e.px) { dup = true; break; }
        if (!dup && count < max) out[count++] = e.px;
    }
    for (int i = 0; i < count; ++i)          // insertion sort ascending
        for (int j = i + 1; j < count; ++j)
            if (out[j] < out[i]) { int t = out[i]; out[i] = out[j]; out[j] = t; }
    return count;
}

}  // namespace gfx
