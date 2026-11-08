#include "app_language.h"

#include <cstdint>
#include <string>

#include <esp_log.h>

#include "font.h"
#include "font_scan.h"
#include "i18n.h"
#include "settings.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr int kMaxPacks = 16;

std::string mark(bool active, const std::string& label)
{
    return (active ? "* " : "   ") + label;
}

// True if `f` has a glyph for every non-ASCII scalar in `s` — i.e. the current
// UI font can actually draw this label. (Endonyms like "日本語" are blank until
// the matching font is active.)
bool renderable(const gfx::Font* f, const std::string& s)
{
    if (f == nullptr) return false;
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end) {
        const uint32_t cp = gfx::utf8_next(p, end);
        if (cp >= 0x80 && !f->hasGlyph(cp)) return false;
    }
    return true;
}
}  // namespace

void LanguageApp::onCreate(const AppContext& ctx)
{
    setupPage(ctx, tr(LANG_TITLE));

    list_.setFont(ctx.ui_font);
    list_.setBounds({0, bodyTop(), ui::metrics::kScreenW, ui::metrics::kScreenH - bodyTop() - 40});
    list_.onSelect([this](int i) { choose(i); });
    list_.fontFor([this](int row) -> const gfx::Font* {
        const int idx = row - 2;
        if (idx < 0 || idx >= static_cast<int>(pack_fonts_.size())) return nullptr;
        return pack_fonts_[idx].get();
    });
    screen_.add(&list_);

    note_.setFont(ctx.ui_font);
    note_.setText(tr(LANG_NOTE));
    note_.setBounds({16, ui::metrics::kScreenH - 36, 448, 30});
    note_.setAlign(ui::Label::Align::Left);
    screen_.add(&note_);

    core::LangPackInfo buf[kMaxPacks];
    const int n = core::listLangPacks(buf, kMaxPacks);
    packs_.assign(buf, buf + n);
    loadPackFonts();

    rebuild();
}

// For each pack whose endonym the running UI font can't draw, load a matching
// /sd/fonts face so the list can render "简体中文" etc. in its own script.
void LanguageApp::loadPackFonts()
{
    pack_fonts_.clear();
    pack_fonts_.resize(packs_.size());

    static gfx::SdFont sd[64];
    const int sdn = gfx::scanSdFonts("/sd/fonts", sd, 64);
    const int px = core::Settings::instance().uiFontSize()
                       ? core::Settings::instance().uiFontSize() : 22;

    for (size_t i = 0; i < packs_.size(); ++i) {
        const core::LangPackInfo& p = packs_[i];
        if (p.font.empty() || renderable(ctx_.ui_font, p.name)) continue;

        const gfx::SdFont* hit = gfx::pickSdFont(sd, sdn, p.font.c_str(), "", px);
        if (!hit) continue;
        auto fnt = std::make_unique<gfx::Font>();
        if (fnt->loadFromFile(hit->path.c_str()) && renderable(fnt.get(), p.name)) {
            ESP_LOGI("app_language", "endonym font for %s: %s", p.code.c_str(), hit->path.c_str());
            pack_fonts_[i] = std::move(fnt);
        }
    }
}

void LanguageApp::rebuild()
{
    core::Settings& st = core::Settings::instance();
    const std::string& code = st.langCode();
    const int lang = st.language();
    const gfx::Font* f = ctx_.ui_font;

    // Show the endonym when the active font can draw it, else an ASCII label so
    // the row is never blank.
    auto label = [f](const std::string& endonym, const char* ascii) {
        return renderable(f, endonym) ? endonym : std::string(ascii);
    };

    std::vector<std::string> rows;
    rows.push_back(mark(code.empty() && lang == 0, label(tr(LANG_KOREAN), "Korean")));
    rows.push_back(mark(code.empty() && lang == 1, label(tr(LANG_ENGLISH), "English")));
    for (size_t i = 0; i < packs_.size(); ++i) {
        const core::LangPackInfo& p = packs_[i];
        const char* ascii = p.name_ascii.empty() ? p.code.c_str() : p.name_ascii.c_str();
        // The row draws with pack_fonts_[i] when set (see list_.fontFor), so the
        // endonym is fine even if the UI font can't render it.
        const bool endonym_ok =
            (i < pack_fonts_.size() && pack_fonts_[i]) || renderable(f, p.name);
        rows.push_back(mark(code == p.code, endonym_ok ? p.name : std::string(ascii)));
    }
    list_.setItems(std::move(rows));
}

void LanguageApp::choose(int row)
{
    core::Settings& st = core::Settings::instance();
    if (row == 0)      { st.setLangCode(""); st.setLanguage(0); }
    else if (row == 1) { st.setLangCode(""); st.setLanguage(1); }
    else {
        const int idx = row - 2;
        if (idx < 0 || idx >= static_cast<int>(packs_.size())) return;
        st.setLangCode(packs_[idx].code);
    }
    rebuild();
}

}  // namespace apps
