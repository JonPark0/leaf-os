/*
 * Kernel — the tick loop.
 *
 * Owns two FreeRTOS tasks:
 *   input  (this thread) — polls hal::touch / hal::keys at ~60 Hz, turns touch
 *                          into Down/Move/Up phases, routes to the active
 *                          Screen, applies the default side-key action
 *                          (front-light brightness) when the Screen does not
 *                          consume the key, then calls Screen::paint().
 *   render (spawned)     — performs the blocking panel refresh once input has
 *                          settled, so input latency is unaffected by the
 *                          ~0.7 s e-ink update.
 *
 * Phase 2 layers an app stack on top of this; for now a single Screen is set.
 */
#pragma once

#include <atomic>
#include <functional>
#include <vector>

#include "screen.h"
#include "ssd1677.h"
#include "status_bar.h"

namespace apps { class App; struct AppContext; }
namespace gfx  { class Font; }

namespace core {

class Kernel {
public:
    static Kernel& instance();

    // hal::i2c/board/keys/touch/power must already be initialised.
    bool init(const ssd1677::HwHooks& display_hooks);

    // --- app stack -------------------------------------------------------
    // The UI font handed to every app via AppContext and to the status bar.
    void setUiFont(const gfx::Font* f);

    ui::StatusBar& statusBar() { return status_bar_; }

    // Install the root app (home). Ownership transfers to the kernel.
    void setRootApp(apps::App* root);

    // Navigation requests are queued and applied at the top of the next tick,
    // so it is safe to call these from inside a widget callback (the callback's
    // owning app/widget may be the one being destroyed).
    void pushApp(apps::App* child);   // launch on top
    void popApp();                    // close the top app (no-op on root)
    int  appDepth() const { return static_cast<int>(stack_.size()); }

    // Direct-screen mode (no app stack) — used by bring-up tests only.
    void setScreen(ui::Screen* s) { direct_screen_ = s; }

    // Invoked after the default side-key handler changes brightness (per-mille).
    void onBrightnessChanged(std::function<void(int)> cb) { bright_cb_ = std::move(cb); }

    // Optional: called once per input tick, after touch/keys, before paint.
    void onTick(std::function<void()> cb) { tick_cb_ = std::move(cb); }

    [[noreturn]] void run();  // never returns

private:
    Kernel() = default;
    void renderLoop();
    static void renderTrampoline(void* self);
    ui::Screen* activeScreen() const;
    apps::AppContext makeContext();
    static void ctxPush(apps::App* child);
    static void ctxPop();

    // Queued navigation, applied by processNav() at the top of the tick loop —
    // never from inside a widget callback, whose owning app/screen may be the
    // one being destroyed (use-after-free otherwise).
    void processNav();
    void doPush(apps::App* child);
    void doPop();

    void attachOverlay();  // point the status bar at the active screen

    std::vector<apps::App*> stack_;
    apps::App* pending_push_ = nullptr;
    bool       pending_pop_  = false;
    const gfx::Font* ui_font_ = nullptr;
    ui::Screen* direct_screen_ = nullptr;
    ui::StatusBar status_bar_;

    std::function<void(int)> bright_cb_;
    std::function<void()> tick_cb_;

    // Written by the input loop, read by the render task on the other core;
    // a plain 64-bit volatile can tear on Xtensa.
    std::atomic<int64_t> last_input_us_{0};
    std::atomic<bool>    input_held_{false};
    bool ready_ = false;
};

}  // namespace core
