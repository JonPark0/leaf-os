#include "lua_app.h"
#include "hal_sys.h"

#include <cstdio>
#include <sys/stat.h>
#include <utility>

#include <esp_log.h>

#include "lua.hpp"

#include "display_manager.h"
#include "font.h"
#include "i18n.h"
#include "iconset.h"
#include "lua_bindings.h"

namespace apps {
namespace {
constexpr char TAG[] = "lua_app";
constexpr int64_t kCallbackBudgetUs = 9'000'000;   // per-callback watchdog
}  // namespace

LuaApp* LuaApp::s_active_ = nullptr;

LuaApp::LuaApp(std::string dir, appstore::AppManifest manifest)
    : manifest_(std::move(manifest)), meta_{"", ""}, dir_(std::move(dir))
{
    meta_ = AppMeta{manifest_.id.c_str(), manifest_.name.c_str()};
}

LuaApp::~LuaApp()
{
    if (L_) { lua_close(L_); L_ = nullptr; }
    if (s_active_ == this) s_active_ = nullptr;
}

void LuaApp::armDeadline() { deadline_us_ = hal::sys::nowUs() + kCallbackBudgetUs; }
bool LuaApp::timedOut() const { return hal::sys::nowUs() > deadline_us_; }

// A small bottom-right "loading…" chip, painted + flushed synchronously right
// before a call that blocks the UI task for seconds (http.get). The next normal
// repaint paints over it. Once per callback (busy_shown_ is cleared in callCb).
void LuaApp::showBusy()
{
    if (busy_shown_) return;
    busy_shown_ = true;

    auto& dm = core::DisplayManager::instance();
    core::Framebuffer& fb = dm.fb();

    constexpr int bw = 176, bh = 48;
    const core::Rect box{core::Framebuffer::kW - bw - 14,
                         core::Framebuffer::kH - bh - 14, bw, bh};
    fb.fillRect(box, core::kBlack);                                   // 2px frame
    fb.fillRect({box.x + 2, box.y + 2, box.w - 4, box.h - 4}, core::kWhite);

    int tx = box.x + 14;
    if (const gfx::IconSet* is = gfx::uiIcons()) {
        const gfx::Icon ic = is->find("clock");
        if (ic) { is->draw(fb, tx, box.y + (bh - ic.h) / 2, ic, core::kBlack); tx += ic.w + 10; }
    }
    const gfx::Font* f = ctx_.ui_font;
    if (f && f->valid())
        f->draw(fb, tx, box.y + (bh - f->lineHeight()) / 2 + f->ascent(),
                core::i18n::tr(core::i18n::LUA_LOADING), core::kBlack);

    dm.invalidate(box, core::RefreshHint::Feedback);
    dm.flush();
}

void LuaApp::fail(const std::string& where)
{
    const char* msg = L_ ? lua_tostring(L_, -1) : nullptr;
    error_ = where + ": " + (msg ? msg : "?");
    ESP_LOGE(TAG, "%s", error_.c_str());
    if (L_) lua_pop(L_, 1);
}

// pcall a global function `name` with `nargs` already on the stack (pushed by
// the caller). Returns true if the callback asked for a repaint. Missing
// callback: cleans the args and returns false.
bool LuaApp::callCb(const char* name, int nargs, int /*nres*/)
{
    if (!L_) return false;                                    // newstate failed
    if (!error_.empty()) { lua_pop(L_, nargs); return false; }
    lua_getglobal(L_, name);
    if (!lua_isfunction(L_, -1)) { lua_pop(L_, nargs + 1); return false; }
    if (nargs) lua_insert(L_, -(nargs + 1));   // move fn below the args

    s_active_ = this;
    busy_shown_ = false;
    armDeadline();
    if (lua_pcall(L_, nargs, 1, 0) != LUA_OK) { fail(name); return false; }
    const bool repaint = lua_toboolean(L_, -1);
    lua_pop(L_, 1);
    return repaint;
}

void LuaApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    view_.setBounds({0, 0, core::Framebuffer::kW, core::Framebuffer::kH});
    screen_.add(&view_);

    makeBackButton(back_, ctx);
    screen_.add(&back_);              // on top of the Lua canvas

    // Optional per-app icon sheet.
    FILE* f = std::fopen((dir_ + "/icon.m5i").c_str(), "rb");
    if (f) {
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (n > 0 && n < 64 * 1024) {
            icon_blob_.resize(static_cast<size_t>(n));
            if (std::fread(icon_blob_.data(), 1, icon_blob_.size(), f) == icon_blob_.size())
                app_icons_.load(icon_blob_.data(), icon_blob_.size());
        }
        std::fclose(f);
    }

    if (manifest_.api > kLuaApiLevel) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), core::i18n::tr(core::i18n::LUA_NEEDS_FIRMWARE),
                      manifest_.api, kLuaApiLevel);
        error_ = msg;
        ESP_LOGW(TAG, "%s needs Lua API %d, firmware has %d",
                 manifest_.id.c_str(), manifest_.api, kLuaApiLevel);
        return;
    }

    L_ = luaL_newstate();
    if (!L_) { error_ = "out of memory (luaL_newstate)"; return; }
    luaapp_open(L_);

    const std::string path = dir_ + "/" + manifest_.entry;
    s_active_ = this;
    armDeadline();
    if (luaL_loadfilex(L_, path.c_str(), "t") != LUA_OK) { fail("load"); return; }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) { fail("run"); return; }
}

void LuaApp::onResume()
{
    callCb("on_show", 0);
    screen_.invalidateAll();
}

void LuaApp::onPause() { callCb("on_hide", 0); }

void LuaApp::onDestroy()
{
    if (L_) { lua_close(L_); L_ = nullptr; }
}

void LuaApp::onTick()
{
    if (callCb("on_tick", 0)) screen_.invalidateAll();
}

void LuaApp::onSystemChanged()
{
    struct stat st{};
    if (::stat(dir_.c_str(), &st) != 0) {          // card / folder gone
        ESP_LOGI(TAG, "%s vanished — leaving", dir_.c_str());
        if (ctx_.pop) ctx_.pop();
    }
}

void LuaApp::paint(core::Framebuffer& fb)
{
    if (!error_.empty()) {
        fb.fillRect({0, 0, core::Framebuffer::kW, core::Framebuffer::kH}, core::kWhite);
        const gfx::Font* f = ctx_.ui_font;
        if (f && f->valid()) {
            int y = bodyTop();
            std::string s = error_;
            size_t pos;
            while ((pos = s.find('\n')) != std::string::npos || !s.empty()) {
                std::string ln = (pos == std::string::npos) ? s : s.substr(0, pos);
                f->draw(fb, 12, y + f->ascent(), ln.c_str(), core::kBlack);
                y += f->lineHeight();
                if (pos == std::string::npos) break;
                s.erase(0, pos + 1);
            }
        }
        return;
    }
    s_active_ = this;
    cur_fb_ = &fb;
    armDeadline();
    callCb("on_draw", 0);
    cur_fb_ = nullptr;
}

bool LuaApp::touch(int x, int y, ui::TouchPhase phase)
{
    if (!error_.empty() || !L_) return false;
    const char* ph = phase == ui::TouchPhase::Down ? "down"
                   : phase == ui::TouchPhase::Move ? "move" : "up";
    lua_pushinteger(L_, x);
    lua_pushinteger(L_, y);
    lua_pushstring(L_, ph);
    if (callCb("on_touch", 3)) screen_.invalidateAll();
    return true;   // the Lua canvas consumes the gesture
}

}  // namespace apps
