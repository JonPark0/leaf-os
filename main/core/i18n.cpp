#include "i18n.h"

#include <cstring>

namespace core::i18n {
namespace {

const char* const kName[] = {
#define X(name, ko, en) #name,
    I18N_MESSAGES(X)
#undef X
};

const char* const kKo[] = {
#define X(name, ko, en) ko,
    I18N_MESSAGES(X)
#undef X
};
const char* const kEn[] = {
#define X(name, ko, en) en,
    I18N_MESSAGES(X)
#undef X
};

Lang s_lang = Ko;
const char* const* s_pack = nullptr;

}  // namespace

void setLang(Lang l) { s_lang = l; }
Lang lang() { return s_lang; }
void setPack(const char* const* table) { s_pack = table; }

const char* tr(Msg m)
{
    if (m < 0 || m >= MSG_COUNT) return "?";
    if (s_pack != nullptr && s_pack[m] != nullptr) return s_pack[m];
    return (s_lang == En ? kEn : kKo)[m];
}

Msg msgFromName(const char* name)
{
    if (name == nullptr) return MSG_COUNT;
    for (int i = 0; i < MSG_COUNT; ++i)
        if (std::strcmp(name, kName[i]) == 0) return static_cast<Msg>(i);
    return MSG_COUNT;
}

const char* msgName(Msg m)
{
    return (m >= 0 && m < MSG_COUNT) ? kName[m] : "";
}

const char* builtinEn(Msg m)
{
    return (m >= 0 && m < MSG_COUNT) ? kEn[m] : "";
}

}  // namespace core::i18n
