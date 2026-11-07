/*
 * LuaApp — hosts one SD-card Lua app (/sd/apps/<id>/<entry>).
 *
 * Immediate-mode: the app defines optional globals that the host calls —
 *   on_show()            entered / resumed
 *   on_hide()            paused
 *   on_draw()            paint (use the `g` table; only valid here)
 *   on_touch(x,y,phase)  phase = "down" | "move" | "up"
 *   on_tick()            once per kernel tick while foregrounded
 * Any callback returning a truthy value asks the host to repaint.
 *
 * The Lua state gets base/table/string/math/utf8/coroutine plus the host
 * libraries g / sys / store / http / json (see lua_bindings). io / os / package
 * / debug and dofile/loadfile/load are removed — the SD app is sandboxed.
 * http.get only reaches the hosts listed in the manifest's `net`, and an app
 * whose `api` is above kLuaApiLevel is not run.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "iconset.h"
#include "manifest.h"
#include "widget.h"
#include "widgets/button.h"

struct lua_State;

namespace apps {

class LuaApp : public App {
public:
    LuaApp(std::string dir, appstore::AppManifest manifest);
    ~LuaApp() override;

    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onPause() override;
    void onDestroy() override;
    void onTick() override;
    void onSystemChanged() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }
    bool onBack() override { return false; }

    // --- accessors for lua_bindings -----------------------------------------
    static LuaApp* active() { return s_active_; }
    core::Framebuffer* activeFb() const { return cur_fb_; }
    const gfx::Font*   uiFont()  const { return ctx_.ui_font; }
    const gfx::IconSet* appIcons() const { return app_icons_.valid() ? &app_icons_ : nullptr; }
    const std::string& appDir()  const { return dir_; }
    const appstore::AppManifest& manifest() const { return manifest_; }
    bool  timedOut() const;           // deadline hook helper
    void  armDeadline();              // reset the per-callback watchdog
    void  showBusy();                 // corner "loading…" hint before a blocking call

private:
    class LuaView : public ui::Widget {
    public:
        explicit LuaView(LuaApp* a) : app_(a) {}
        void draw(core::Framebuffer& fb) override { app_->paint(fb); }
        bool onTouch(int x, int y, ui::TouchPhase p) override { return app_->touch(x, y, p); }
    private:
        LuaApp* app_;
    };

    void paint(core::Framebuffer& fb);
    bool touch(int x, int y, ui::TouchPhase phase);
    bool callCb(const char* name, int nargs, int nres = 0);  // pcall global; true => repaint
    void fail(const std::string& where);

    static LuaApp* s_active_;

    appstore::AppManifest manifest_;     // backs AppMeta's const char*
    AppMeta     meta_;
    std::string dir_;
    ui::Screen  screen_;
    LuaView     view_{this};
    ui::Button  back_;
    lua_State*  L_ = nullptr;
    gfx::IconSet app_icons_;
    std::vector<uint8_t> icon_blob_;   // backs app_icons_
    std::string error_;
    core::Framebuffer* cur_fb_ = nullptr;   // set only for the duration of paint()
    int64_t deadline_us_ = 0;
    bool    busy_shown_ = false;            // one loading hint per callback
};

}  // namespace apps
