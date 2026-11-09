#include "kernel.h"

#include <esp_log.h>

#include "app.h"
#include "display_manager.h"
#include "hal_keys.h"
#include "hal_power.h"
#include "hal_storage.h"
#include "hal_sys.h"
#include "hal_touch.h"
#include "hal_wifi.h"

namespace core {
namespace {
constexpr char TAG[] = "kernel";
constexpr int64_t kSettleUs      = 220'000;    // quiet time before a refresh
constexpr int64_t kMaxDeferUs    = 2'000'000;  // refresh anyway under constant input
constexpr int     kInputPeriodMs = 16;
constexpr int     kRenderPeriodMs = 40;
constexpr int     kBrightStepPm  = 100;        // 10% per key event
}  // namespace

Kernel& Kernel::instance()
{
    static Kernel k;
    return k;
}

bool Kernel::init(const ssd1677::HwHooks& hooks)
{
    if (ready_) return true;
    if (!DisplayManager::instance().init(hooks)) {
        ESP_LOGE(TAG, "DisplayManager init failed");
        return false;
    }
    ready_ = true;
    return true;
}

void Kernel::setUiFont(const gfx::Font* f)
{
    ui_font_ = f;
    status_bar_.setFont(f);
    status_bar_.setBounds({0, 0, Framebuffer::kW, ui::StatusBar::kHeight});
}

// --- app stack --------------------------------------------------------------

void Kernel::attachOverlay()
{
    ui::Screen* s = activeScreen();
    if (s) s->setOverlay(&status_bar_);
}

apps::AppContext Kernel::makeContext()
{
    apps::AppContext c;
    c.ui_font = ui_font_;
    c.push = &Kernel::ctxPush;
    c.pop  = &Kernel::ctxPop;
    return c;
}

void Kernel::ctxPush(apps::App* child) { instance().pushApp(child); }
void Kernel::ctxPop()                  { instance().popApp(); }

void Kernel::setRootApp(apps::App* root)
{
    for (auto* a : stack_) { a->onPause(); a->onDestroy(); delete a; }
    stack_.clear();
    if (root == nullptr) return;
    root->onCreate(makeContext());
    root->onResume();
    stack_.push_back(root);
    attachOverlay();
    DisplayManager::instance().invalidateAll(RefreshHint::Quality);
}

// Public entry points only queue — the real work happens in processNav() at a
// point where no widget/app object is mid-call on the stack.
void Kernel::pushApp(apps::App* child)
{
    if (child == nullptr) return;
    // One push per tick: a second one (a double tap on a tile) is dropped. It
    // was never created, so it gets no lifecycle calls, just the delete.
    if (pending_push_) { delete child; return; }
    pending_push_ = child;
}

void Kernel::popApp()
{
    pending_pop_ = true;
}

void Kernel::goHome()
{
    pending_home_ = true;
}

void Kernel::processNav()
{
    if (pending_home_) {
        // Home wins over anything else queued this tick. A queued child was
        // never created, so it gets no lifecycle calls, just the delete.
        pending_home_ = false;
        pending_pop_  = false;
        delete pending_push_;
        pending_push_ = nullptr;
        doGoHome();
        return;
    }
    if (pending_pop_)  { pending_pop_ = false; doPop(); }
    if (pending_push_) { apps::App* c = pending_push_; pending_push_ = nullptr; doPush(c); }
}

void Kernel::doPush(apps::App* child)
{
    if (child == nullptr) return;
    if (!stack_.empty()) stack_.back()->onPause();
    child->onCreate(makeContext());
    child->onResume();
    stack_.push_back(child);
    attachOverlay();
    if (child->screen()) child->screen()->invalidateAll();
    DisplayManager::instance().invalidateAll(RefreshHint::Page);  // fast full-frame
    ESP_LOGI(TAG, "push '%s' (depth %d)", child->meta().id, appDepth());
}

void Kernel::doPop()
{
    if (stack_.size() <= 1) return;  // never pop the root
    apps::App* top = stack_.back();
    stack_.pop_back();
    top->onPause();
    top->onDestroy();
    delete top;
    stack_.back()->onResume();
    attachOverlay();
    if (stack_.back()->screen()) stack_.back()->screen()->invalidateAll();
    DisplayManager::instance().invalidateAll(RefreshHint::Page);  // fast full-frame
    ESP_LOGI(TAG, "pop -> '%s' (depth %d)", stack_.back()->meta().id, appDepth());
}

void Kernel::doGoHome()
{
    if (stack_.size() <= 1) return;  // already home
    // Only the top app is running; the ones below were paused when they were
    // covered, so they just get onDestroy.
    stack_.back()->onPause();
    while (stack_.size() > 1) {
        apps::App* a = stack_.back();
        stack_.pop_back();
        a->onDestroy();
        delete a;
    }
    stack_.back()->onResume();
    attachOverlay();
    if (stack_.back()->screen()) stack_.back()->screen()->invalidateAll();
    DisplayManager::instance().invalidateAll(RefreshHint::Page);  // one refresh for the lot
    ESP_LOGI(TAG, "home -> '%s'", stack_.back()->meta().id);
}

ui::Screen* Kernel::activeScreen() const
{
    if (!stack_.empty()) return stack_.back()->screen();
    return direct_screen_;
}

// --- render task ----------------------------------------------------------

void Kernel::renderTrampoline(void* self)
{
    static_cast<Kernel*>(self)->renderLoop();
}

void Kernel::stepRender()
{
    auto& dm = DisplayManager::instance();
    const int64_t t = hal::sys::nowUs();
    if (dm.hasPending()) {
        if (first_pending_us_ == 0) first_pending_us_ = t;
        const bool settled = !input_held_ && (t - last_input_us_) > kSettleUs;
        const bool capped  = (t - first_pending_us_) > kMaxDeferUs;
        if (settled || capped) {
            dm.flush();
            first_pending_us_ = 0;
        }
    } else {
        first_pending_us_ = 0;
    }
}

void Kernel::renderLoop()
{
    for (;;) {
        stepRender();
        hal::sys::delayMs(kRenderPeriodMs);
    }
}

// --- input loop ---------------------------------------------------------

void Kernel::stepInput()
{
    auto& dm = DisplayManager::instance();
    hal::keys::KeyEvent kevs[8];
    hal::touch::Point* tpts = touch_pts_;

    const int64_t t = hal::sys::nowUs();
    processNav();  // apply any queued push/pop before touching the stack
    ui::Screen* scr = activeScreen();
    apps::App* app = stack_.empty() ? nullptr : stack_.back();
    bool activity = false;

    // --- keys ---
    const int kn = hal::keys::poll(kevs, 8);
    for (int i = 0; i < kn; ++i) {
        const auto& e = kevs[i];
        const bool consumed = scr && scr->onKey(static_cast<int>(e.key),
                                                static_cast<int>(e.ev));
        const bool act = e.ev == hal::keys::Event::Click ||
                         e.ev == hal::keys::Event::Repeat;
        if (!consumed && act) {
            const int dir = (e.key == hal::keys::kA) ? +1 : -1;  // A up, B down
            hal::power::setBrightness(hal::power::brightness() + dir * kBrightStepPm);
            if (bright_cb_) bright_cb_(hal::power::brightness());
        }
        activity = true;
    }

    // --- touch -> Down / Move / Up ---
    const int tn = hal::touch::read(tpts, 2);
    const bool down = tn > 0;
    if (scr) {
        if (down && !was_down_) {
            ESP_LOGI(TAG, "touch down (%d,%d)", tpts[0].x, tpts[0].y);
            scr->handleTouch(tpts[0].x, tpts[0].y, ui::TouchPhase::Down);
        }
        else if (down && was_down_)  scr->handleTouch(tpts[0].x, tpts[0].y, ui::TouchPhase::Move);
        else if (!down && was_down_) scr->handleTouch(tpts[0].x, tpts[0].y, ui::TouchPhase::Up);
    }
    if (down != was_down_ || down) activity = true;
    was_down_ = down;

    // SD hot-plug: poll() rate-limits its own I2C reads. On a settled
    // insert/removal, let the foreground app rebuild (home re-filters the
    // file-manager tile).
    if (hal::storage::poll() && app) app->onSystemChanged();

    // Wi-Fi state -> status bar indicator.
    if (hal::wifi::poll()) status_bar_.setWifiConnected(hal::wifi::connected());

    if (app) app->onTick();
    if (tick_cb_) tick_cb_();
    status_bar_.tick();

    if (scr) scr->paint(dm.fb());

    input_held_ = down || hal::keys::isDown(hal::keys::kA) ||
                  hal::keys::isDown(hal::keys::kB);
    if (activity) last_input_us_ = t;
}

void Kernel::run()
{
    if (!hal::sys::spawnPinned(renderTrampoline, "render", 6144, this, 4, 1)) {
        ESP_LOGE(TAG, "render task not started");
    }
    for (;;) {
        stepInput();
        hal::sys::delayMs(kInputPeriodMs);
    }
}

}  // namespace core
