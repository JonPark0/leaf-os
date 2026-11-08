#include "app_display.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "hal_power.h"
#include "hal_storage.h"
#include "i18n.h"
#include "lang_pack.h"
#include "settings.h"
#include "widgets/list_view.h"

namespace apps {
namespace {
using namespace core::i18n;

constexpr int kReadSizes[] = {16, 20, 22, 24, 28, 32};
constexpr int kDefaultBright  = 60;
constexpr int kDefaultUiSize   = 22;
constexpr int kDefaultReadSize = 22;

// The UI script: a language pack's "# font" tag, else "ko".
const char* activeScript()
{
    const core::Settings& st = core::Settings::instance();
    if (!st.langCode().empty() && core::langPackFont()[0] != '\0')
        return core::langPackFont();
    return "ko";
}

const char* familyLabel(const std::string& canonical)
{
    if (canonical.empty())                      return tr(FAMILY_DEFAULT);
    if (canonical == "gothic")                  return tr(FAMILY_GOTHIC);
    if (canonical == "myeongjo")                return tr(FAMILY_MYEONGJO);
    if (canonical == "pen")                     return tr(FAMILY_PEN);
    if (canonical == "sans")                    return tr(FAMILY_SANS);
    if (canonical == "serif")                   return tr(FAMILY_SERIF);
    return canonical.c_str();
}

// Effective list-row padding on/off right now: explicit user choice, else the
// active pack's "# ui_pad", else on for built-in ko/en. Mirrors main.cpp.
bool currentRowPad()
{
    const core::Settings& st = core::Settings::instance();
    if (st.uiRowPad() >= 0) return st.uiRowPad() != 0;
    if (core::langPackUiPad() >= 0) return core::langPackUiPad() != 0;
    return st.langCode().empty();
}

// Step to the neighbouring preset. If `cur` isn't a preset, snap to the nearest
// one first so the first tap always moves in the asked direction.
int stepPreset(const int* presets, int n, int cur, int dir)
{
    if (n == 0) return cur;
    int idx = 0, bestd = 1 << 30;
    for (int i = 0; i < n; ++i) {
        const int d = std::abs(presets[i] - cur);
        if (d < bestd) { bestd = d; idx = i; }
    }
    if (bestd != 0) {                       // not an exact preset: snap, don't step
        // move toward the requested direction if the snap didn't already
        if (dir > 0 && presets[idx] <= cur && idx + 1 < n) ++idx;
        if (dir < 0 && presets[idx] >= cur && idx > 0)     --idx;
        return presets[idx];
    }
    return presets[std::clamp(idx + dir, 0, n - 1)];
}
}  // namespace

void DisplayApp::rescan()
{
    sd_fonts_.clear();
    families_.clear();
    script_ = activeScript();

    if (hal::storage::state() == hal::storage::State::Mounted) {
        const std::string dir = std::string(hal::storage::mountPoint()) + "/fonts";
        static gfx::SdFont buf[64];
        const int n = gfx::scanSdFonts(dir.c_str(), buf, 64);
        sd_fonts_.assign(buf, buf + n);

        const char* fam[8];
        const int fn = gfx::familiesFor(buf, n, script_, fam, 8);
        for (int i = 0; i < fn; ++i) families_.emplace_back(fam[i]);
    }
}

void DisplayApp::onCreate(const AppContext& ctx)
{
    rescan();
    setupPage(ctx, tr(SET_DISPLAY));

    static const Msg kRowMsg[kRowCount] = {
        DISP_BRIGHTNESS, DISP_UI_FAMILY, DISP_UI_SIZE, DISP_READ_FAMILY, DISP_READ_SIZE};

    const int top = bodyTop();
    const int rowH = 64, gap = 12;
    for (int r = 0; r < kRowCount; ++r) {
        const int y = top + r * (rowH + gap);

        name_[r].setFont(ctx.ui_font);
        name_[r].setText(tr(kRowMsg[r]));
        name_[r].setBounds({16, y, 130, rowH});
        name_[r].setAlign(ui::Label::Align::Left);
        screen_.add(&name_[r]);

        minus_[r].setFont(ctx.ui_font);
        minus_[r].setLabel("-");
        minus_[r].setBounds({150, y, 60, rowH});
        minus_[r].onClick([this, r] { bump(r, -1); });
        screen_.add(&minus_[r]);

        value_[r].setFont(ctx.ui_font);
        value_[r].setBounds({214, y, 130, rowH});
        value_[r].setAlign(ui::Label::Align::Center);
        screen_.add(&value_[r]);

        plus_[r].setFont(ctx.ui_font);
        plus_[r].setLabel("+");
        plus_[r].setBounds({348, y, 60, rowH});
        plus_[r].onClick([this, r] { bump(r, +1); });
        screen_.add(&plus_[r]);
    }

    const int py = top + kRowCount * (rowH + gap);
    pad_name_.setFont(ctx.ui_font);
    pad_name_.setText(tr(DISP_TOUCH_PAD));
    pad_name_.setBounds({16, py, 300, rowH});
    pad_name_.setAlign(ui::Label::Align::Left);
    screen_.add(&pad_name_);

    pad_.setFont(ctx.ui_font);
    pad_.setChecked(currentRowPad());
    pad_.setBounds({330, py, 130, rowH});
    pad_.onToggle([](bool on) {
        core::Settings::instance().setUiRowPad(on);
        ui::ListView::setGlobalRowPadding(on ? 12 : 0);
    });
    screen_.add(&pad_);

    const int by = py + (rowH + gap) + 4;
    defaults_.setFont(ctx.ui_font);
    defaults_.setLabel(tr(DISP_RESTORE));
    defaults_.setBounds({16, by, 200, 52});
    defaults_.onClick([this] { applyDefaults(); });
    screen_.add(&defaults_);

    note_.setFont(ctx.ui_font);
    note_.setText(tr(DISP_RESTART_NOTE));
    note_.setBounds({16, by + 64, 448, 30});
    note_.setAlign(ui::Label::Align::Left);
    screen_.add(&note_);

    refresh();
}

// Cycle a family setting through the faces present for the script. `get`/`set`
// read and write the relevant core::Settings field.
static std::string cycleFamily(const std::vector<std::string>& fams,
                               const std::string& cur, int dir)
{
    if (fams.empty()) return cur;
    int idx = 0;
    for (size_t i = 0; i < fams.size(); ++i) if (fams[i] == cur) { idx = static_cast<int>(i); break; }
    idx = (idx + dir + static_cast<int>(fams.size())) % static_cast<int>(fams.size());
    return fams[idx];
}

void DisplayApp::bump(int row, int delta)
{
    core::Settings& st = core::Settings::instance();
    switch (row) {
        case kBright: {
            int p = std::clamp(hal::power::brightnessPercent() + delta * 10, 0, 100);
            hal::power::setBrightnessPercent(p);
            st.setBrightnessPerMille(hal::power::brightness());
            break;
        }
        case kUiFamily:
            st.setUiFontFamily(cycleFamily(families_, st.uiFontFamily(), delta));
            break;
        case kUiSize: {
            int sizes[16];
            const int sn = gfx::sizesFor(sd_fonts_.data(), static_cast<int>(sd_fonts_.size()),
                                         script_, st.uiFontFamily().c_str(), sizes, 16);
            if (sn == 0) { st.setUiFontSize(kDefaultUiSize); break; }
            st.setUiFontSize(stepPreset(sizes, sn, st.uiFontSize(), delta));
            break;
        }
        case kReadFamily:
            st.setReadFontFamily(cycleFamily(families_, st.readFontFamily(), delta));
            break;
        case kReadSize:
            st.setReadFontSize(stepPreset(kReadSizes,
                static_cast<int>(sizeof(kReadSizes) / sizeof(int)), st.readFontSize(), delta));
            break;
    }
    refresh();
}

void DisplayApp::applyDefaults()
{
    core::Settings& st = core::Settings::instance();
    hal::power::setBrightnessPercent(kDefaultBright);
    st.setBrightnessPerMille(hal::power::brightness());
    st.setUiFontFamily("");
    st.setUiFontSize(kDefaultUiSize);
    st.setReadFontFamily("");
    st.setReadFontSize(kDefaultReadSize);

    const bool d = (core::langPackUiPad() >= 0) ? core::langPackUiPad() != 0
                                                : st.langCode().empty();
    st.setUiRowPad(d);
    ui::ListView::setGlobalRowPadding(d ? 12 : 0);
    pad_.setChecked(d);
    refresh();
}

void DisplayApp::refresh()
{
    core::Settings& st = core::Settings::instance();
    char b[24];

    std::snprintf(b, sizeof(b), "%d%%", hal::power::brightnessPercent());
    value_[kBright].setText(b);

    value_[kUiFamily].setText(familyLabel(st.uiFontFamily()));
    std::snprintf(b, sizeof(b), "%dpx", st.uiFontSize() ? st.uiFontSize() : kDefaultUiSize);
    value_[kUiSize].setText(b);

    value_[kReadFamily].setText(familyLabel(st.readFontFamily()));
    std::snprintf(b, sizeof(b), "%dpx", st.readFontSize());
    value_[kReadSize].setText(b);
}

}  // namespace apps
