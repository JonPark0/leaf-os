#include "app_power.h"
#include "hal_sys.h"


#include "display_manager.h"
#include "hal_power.h"
#include "i18n.h"
#include "iconset.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr int64_t kArmWindowUs = 5'000'000;   // 5 s to confirm

// Nearest-neighbour ×s blit of a 1-bit icon (MSB-first rows).
void blitIconScaled(core::Framebuffer& fb, const gfx::Icon& ic, int x, int y, int s)
{
    for (int r = 0; r < ic.h; ++r) {
        const uint8_t* line = ic.bits + r * ic.stride;
        for (int c = 0; c < ic.w; ++c)
            if (line[c >> 3] & (0x80 >> (c & 7)))
                fb.fillRect({x + c * s, y + r * s, s, s}, core::kBlack);
    }
}

// Full-screen white with a big centred book glyph, then one synchronous
// full-mono refresh — e-ink keeps it latched after the rail is cut.
void drawPowerOffScreen()
{
    auto& dm = core::DisplayManager::instance();
    core::Framebuffer& fb = dm.fb();
    fb.fillRect({0, 0, core::Framebuffer::kW, core::Framebuffer::kH}, core::kWhite);

    if (const gfx::IconSet* is = gfx::uiIcons()) {
        const gfx::Icon ic = is->find("book");
        if (ic) {
            const int s = 6;   // 24 px sheet -> 144 px
            blitIconScaled(fb, ic, (core::Framebuffer::kW - ic.w * s) / 2,
                           (core::Framebuffer::kH - ic.h * s) / 2, s);
        }
    }

    dm.holdFullRefresh(false);           // drop any modal hold
    dm.requestFullClear();
    dm.invalidateAll(core::RefreshHint::Quality);
    dm.flush();                          // blocks ~4 s; image is on the panel
}
}  // namespace

void PowerApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    title_.setFont(ctx.ui_font);
    title_.setText(tr(PWR_TITLE));
    title_.setBounds({130, ui::StatusBar::kHeight + 10, 220, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const int x = 90, w = 300, h = 64;
    int y = bodyTop() + 40;

    restart_.setFont(ctx.ui_font);
    restart_.setLabel(tr(PWR_RESTART));
    restart_.setBounds({x, y, w, h});
    restart_.onClick([this] { tap(kRestart); });
    screen_.add(&restart_);

    y += h + 28;
    shutdown_.setFont(ctx.ui_font);
    shutdown_.setLabel(tr(PWR_SHUTDOWN));
    shutdown_.setBounds({x, y, w, h});
    shutdown_.onClick([this] { tap(kShutdown); });
    screen_.add(&shutdown_);

    hint_.setFont(ctx.ui_font);
    hint_.setText("");
    hint_.setBounds({16, y + h + 24, 448, 30});
    hint_.setAlign(ui::Label::Align::Center);
    screen_.add(&hint_);
}

void PowerApp::disarm()
{
    armed_ = kNone;
    hint_.setText("");   // button labels are fixed; only the hint line changes
}

void PowerApp::tap(Action a)
{
    if (armed_ == a) {                       // confirmed
        if (a == kRestart) hal::power::reboot();     // never returns
        disarm();
        drawPowerOffScreen();                        // book glyph, latched on e-ink
        if (!hal::power::shutdown()) {               // returns only if still powered
            screen_.invalidateAll();                 // repaint this page over it
            hint_.setText(tr(PWR_NO_OFF));
        }
        return;
    }
    disarm();
    armed_ = a;
    armed_us_ = hal::sys::nowUs();
    hint_.setText(tr(PWR_CONFIRM));   // only the hint line, not the button
}

void PowerApp::onTick()
{
    if (armed_ != kNone && hal::sys::nowUs() - armed_us_ > kArmWindowUs)
        disarm();
}

}  // namespace apps
