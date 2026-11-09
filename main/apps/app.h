/*
 * App — a screen-owning unit the kernel runs on a stack.
 *
 * 1st milestone: apps are compiled into the firmware. The interface is the
 * boundary a future Lua runtime would implement, so keep it small and free of
 * engine internals. SD manifests attach data/icons to a builtin id (see
 * docs/APP_MANIFEST.md, later phase).
 */
#pragma once

#include "font.h"
#include "screen.h"

namespace apps {

class App;

// Services the kernel hands to every app.
struct AppContext {
    const gfx::Font* ui_font = nullptr;

    // Navigation (implemented by core::Kernel).
    void (*push)(App* child)  = nullptr;   // launch on top; kernel takes ownership
    void (*pop)()             = nullptr;   // close the current app
};

struct AppMeta {
    const char* id = "";   // stable identifier: logs, and the SD manifest key
};

class App {
public:
    virtual ~App() = default;

    virtual void onCreate(const AppContext& ctx) { ctx_ = ctx; }
    virtual void onResume() {}
    virtual void onPause()  {}
    virtual void onDestroy() {}
    virtual void onTick()   {}   // called each kernel tick while foregrounded

    // A system-wide state the app's contents may depend on has changed (e.g. an
    // SD card was inserted or removed). Only delivered to the foreground app.
    virtual void onSystemChanged() {}

    // The screen the kernel should paint / route touch to. Never null after
    // onCreate().
    virtual ui::Screen* screen() = 0;

    virtual const AppMeta& meta() const = 0;

    // Hardware Back / on-screen back. Return true if handled; otherwise the
    // kernel pops this app.
    virtual bool onBack() { return false; }

protected:
    AppContext ctx_{};
};

}  // namespace apps
