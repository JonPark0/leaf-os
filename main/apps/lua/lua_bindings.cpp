#include "lua_bindings.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <esp_log.h>
#include <cJSON.h>

#include "lua.hpp"

#include "font.h"
#include "framebuffer.h"
#include "hal_http.h"
#include "hal_power.h"
#include "hal_wifi.h"
#include "i18n.h"
#include "iconset.h"
#include "kv.h"
#include "lua_app.h"
#include "url.h"

namespace apps {
namespace {
constexpr char TAG[] = "lua";
constexpr size_t kHttpMax = 48 * 1024;

// ---- g : drawing (valid only inside on_draw) --------------------------------

core::Framebuffer* fb_or_err(lua_State* L)
{
    LuaApp* a = LuaApp::active();
    core::Framebuffer* fb = a ? a->activeFb() : nullptr;
    if (!fb) luaL_error(L, "g.* is only valid inside on_draw()");
    return fb;
}

uint8_t ink_arg(lua_State* L, int idx)
{
    int v = static_cast<int>(luaL_optinteger(L, idx, core::kBlack));
    if (v < 0) v = 0;
    if (v > 3) v = 3;
    return static_cast<uint8_t>(v);
}

int g_w(lua_State* L)  { lua_pushinteger(L, core::Framebuffer::kW); return 1; }
int g_h(lua_State* L)  { lua_pushinteger(L, core::Framebuffer::kH); return 1; }

int g_clear(lua_State* L)
{
    core::Framebuffer* fb = fb_or_err(L);
    const uint8_t ink = (lua_gettop(L) >= 1) ? ink_arg(L, 1) : static_cast<uint8_t>(core::kWhite);
    fb->fillRect({0, 0, core::Framebuffer::kW, core::Framebuffer::kH}, ink);
    return 0;
}

int g_rect(lua_State* L)
{
    core::Framebuffer* fb = fb_or_err(L);
    const int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
    const int w = luaL_checkinteger(L, 3), h = luaL_checkinteger(L, 4);
    fb->fillRect({x, y, w, h}, ink_arg(L, 5));
    return 0;
}

int g_text(lua_State* L)
{
    core::Framebuffer* fb = fb_or_err(L);
    const int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
    const char* s = luaL_checkstring(L, 3);
    const gfx::Font* f = LuaApp::active()->uiFont();
    if (f && f->valid())
        f->draw(*fb, x, y + f->ascent(), s, ink_arg(L, 4));
    return 0;
}

int g_textw(lua_State* L)
{
    const char* s = luaL_checkstring(L, 1);
    const gfx::Font* f = LuaApp::active()->uiFont();
    lua_pushinteger(L, (f && f->valid()) ? f->measure(s) : 0);
    return 1;
}

int g_line_h(lua_State* L)
{
    const gfx::Font* f = LuaApp::active()->uiFont();
    lua_pushinteger(L, (f && f->valid()) ? f->lineHeight() : 0);
    return 1;
}

int g_icon(lua_State* L)
{
    core::Framebuffer* fb = fb_or_err(L);
    const int x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
    const char* name = luaL_checkstring(L, 3);
    const gfx::IconSet* is = LuaApp::active()->appIcons();
    if (!is || !is->find(name)) is = gfx::uiIcons();
    if (is) is->draw(*fb, x, y, name, ink_arg(L, 4));
    return 0;
}

// ---- sys -------------------------------------------------------------------

int sys_now(lua_State* L)     { lua_pushinteger(L, static_cast<lua_Integer>(::time(nullptr))); return 1; }
int sys_battery(lua_State* L) { lua_pushinteger(L, hal::power::batteryPercent()); return 1; }
int sys_wifi(lua_State* L)    { lua_pushboolean(L, hal::wifi::connected()); return 1; }
int sys_lang(lua_State* L)    { lua_pushstring(L, core::i18n::lang() == core::i18n::En ? "en" : "ko"); return 1; }

int sys_clock(lua_State* L)
{
    time_t t = ::time(nullptr);
    struct tm tmv{};
    localtime_r(&t, &tmv);
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, tmv.tm_year + 1900); lua_setfield(L, -2, "y");
    lua_pushinteger(L, tmv.tm_mon + 1);     lua_setfield(L, -2, "mo");
    lua_pushinteger(L, tmv.tm_mday);        lua_setfield(L, -2, "d");
    lua_pushinteger(L, tmv.tm_hour);        lua_setfield(L, -2, "h");
    lua_pushinteger(L, tmv.tm_min);         lua_setfield(L, -2, "mi");
    return 1;
}

int sys_log(lua_State* L)
{
    int n = lua_gettop(L);
    std::string line;
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);   // pushes a string
        if (i > 1) line += '\t';
        line.append(s, len);
        lua_pop(L, 1);
    }
    ESP_LOGI(TAG, "[%s] %s",
             LuaApp::active() ? LuaApp::active()->appDir().c_str() : "?", line.c_str());
    return 0;
}

// ---- store : /sd/apps/<id>/state.txt -------------------------------------

std::string statePath()
{
    // Leading dot: hidden on a PC file manager, matches the reader's .pos and
    // hal_wifi's .wifi convention.
    return LuaApp::active()->appDir() + "/.state.txt";
}

int store_get(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    FILE* f = std::fopen(statePath().c_str(), "r");
    if (f) {
        char buf[256];
        while (std::fgets(buf, sizeof(buf), f)) {
            std::string k, v;
            if (core::kv::split(core::kv::trim(buf), k, v) && k == key) {
                std::fclose(f);
                lua_pushlstring(L, v.data(), v.size());
                return 1;
            }
        }
        std::fclose(f);
    }
    if (lua_gettop(L) >= 2) lua_pushvalue(L, 2); else lua_pushnil(L);
    return 1;
}

int store_set(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    size_t vlen = 0;
    const char* val = luaL_tolstring(L, 2, &vlen);   // coerce anything to string
    std::string value(val, vlen);
    lua_pop(L, 1);

    std::vector<std::pair<std::string, std::string>> rows;
    FILE* f = std::fopen(statePath().c_str(), "r");
    bool replaced = false;
    if (f) {
        char buf[256];
        while (std::fgets(buf, sizeof(buf), f)) {
            std::string k, v;
            if (!core::kv::split(core::kv::trim(buf), k, v)) continue;
            if (k == key) { v = value; replaced = true; }
            rows.emplace_back(k, v);
        }
        std::fclose(f);
    }
    if (!replaced) rows.emplace_back(key, value);

    f = std::fopen(statePath().c_str(), "w");
    if (!f) { lua_pushboolean(L, 0); return 1; }
    for (auto& r : rows) std::fprintf(f, "%s = %s\n", r.first.c_str(), r.second.c_str());
    std::fclose(f);
    lua_pushboolean(L, 1);
    return 1;
}

// ---- http.get -----------------------------------------------------------

// Percent-encode the bytes esp_http_client's URL parser chokes on (a raw space
// in "New York", any UTF-8 city name). Structural URL characters and an already
// present '%' pass through, so a pre-encoded URL is left untouched.
std::string urlSanitize(const char* in)
{
    static const char* kSafe = "-_.~:/?#[]@!$&'()*+,;=%";
    std::string out;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(in); *p; ++p) {
        const unsigned char ch = *p;
        if (std::isalnum(ch) || std::strchr(kSafe, ch)) {
            out.push_back(static_cast<char>(ch));
        } else {
            char esc[4];
            std::snprintf(esc, sizeof(esc), "%%%02X", ch);
            out += esc;
        }
    }
    return out;
}

int http_get(lua_State* L)
{
    // Fetch every argument before building C++ objects: a luaL_* error
    // longjmps and would skip their destructors.
    const char* raw_url = luaL_checkstring(L, 1);
    const int timeout = static_cast<int>(luaL_optinteger(L, 2, 8000));
    const std::string url = urlSanitize(raw_url);

    LuaApp* app = LuaApp::active();
    if (!app || !appstore::hostAllowed(app->manifest().net, url)) {
        ESP_LOGW(TAG, "http.get to '%s' refused: host not in app.txt net",
                 appstore::urlHost(url).c_str());
        lua_pushnil(L);
        lua_pushstring(L, "permission");
        return 2;
    }

    if (!hal::wifi::connected()) { lua_pushnil(L); lua_pushstring(L, "offline"); return 2; }

    // This blocks the UI task for seconds — show a "loading…" hint first.
    if (LuaApp* a = LuaApp::active()) a->showBusy();

    const hal::http::Response r = hal::http::get(url, timeout, kHttpMax);

    // Time spent blocked in the network is not script runtime: restart the
    // watchdog so a slow server doesn't kill the app on its next instruction.
    if (LuaApp* a = LuaApp::active()) a->armDeadline();

    if (!r.ok) {
        lua_pushnil(L);
        lua_pushstring(L, r.error.c_str());
        return 2;
    }
    lua_pushlstring(L, r.body.data(), r.body.size());
    lua_pushinteger(L, r.status);
    return 2;
}

// ---- json.decode (cJSON -> Lua) --------------------------------------------

// Deeper documents are refused: each level costs a Lua stack slot and a C
// frame on the 16 KB main task stack (cJSON itself accepts up to 1000).
constexpr int kJsonMaxDepth = 32;

// Pushes exactly one value on success. On failure returns false having pushed
// nothing net (the caller still owns the cJSON tree, so no luaL_error here).
bool push_json(lua_State* L, const cJSON* n, int depth = 0)
{
    if (depth > kJsonMaxDepth || !lua_checkstack(L, 3)) return false;
    if (!n || cJSON_IsNull(n)) { lua_pushnil(L); return true; }
    if (cJSON_IsBool(n))   { lua_pushboolean(L, cJSON_IsTrue(n)); return true; }
    if (cJSON_IsNumber(n)) {
        double d = n->valuedouble;
        if (d == (double)(long long)d) lua_pushinteger(L, (lua_Integer)d);
        else lua_pushnumber(L, d);
        return true;
    }
    if (cJSON_IsString(n)) { lua_pushstring(L, n->valuestring ? n->valuestring : ""); return true; }
    if (cJSON_IsArray(n)) {
        lua_createtable(L, cJSON_GetArraySize(n), 0);
        int i = 1;
        for (const cJSON* c = n->child; c; c = c->next) {
            if (!push_json(L, c, depth + 1)) { lua_pop(L, 1); return false; }
            lua_rawseti(L, -2, i++);
        }
        return true;
    }
    if (cJSON_IsObject(n)) {
        lua_createtable(L, 0, cJSON_GetArraySize(n));
        for (const cJSON* c = n->child; c; c = c->next) {
            if (!c->string) continue;
            if (!push_json(L, c, depth + 1)) { lua_pop(L, 1); return false; }
            lua_setfield(L, -2, c->string);
        }
        return true;
    }
    lua_pushnil(L);
    return true;
}

int json_decode(lua_State* L)
{
    size_t len = 0;
    const char* s = luaL_checklstring(L, 1, &len);
    cJSON* root = cJSON_ParseWithLength(s, len);
    if (!root) { lua_pushnil(L); lua_pushstring(L, "parse error"); return 2; }
    const bool ok = push_json(L, root);
    cJSON_Delete(root);
    if (!ok) { lua_pushnil(L); lua_pushstring(L, "too deeply nested"); return 2; }
    return 1;
}

// ---- runaway-script watchdog -------------------------------------------

void timeout_hook(lua_State* L, lua_Debug*)
{
    LuaApp* a = LuaApp::active();
    if (a && a->timedOut()) luaL_error(L, "script timeout");
}

// ---- table registration ----------------------------------------------

void reg(lua_State* L, const char* tbl, const luaL_Reg* fns)
{
    lua_newtable(L);
    luaL_setfuncs(L, fns, 0);
    lua_setglobal(L, tbl);
}

const luaL_Reg kG[] = {
    {"w", g_w}, {"h", g_h}, {"clear", g_clear}, {"rect", g_rect},
    {"text", g_text}, {"textw", g_textw}, {"line_h", g_line_h}, {"icon", g_icon},
    {nullptr, nullptr}};
const luaL_Reg kSys[] = {
    {"now", sys_now}, {"clock", sys_clock}, {"battery", sys_battery},
    {"wifi", sys_wifi}, {"lang", sys_lang}, {"log", sys_log}, {nullptr, nullptr}};
const luaL_Reg kStore[] = {{"get", store_get}, {"set", store_set}, {nullptr, nullptr}};
const luaL_Reg kHttp[] = {{"get", http_get}, {nullptr, nullptr}};
const luaL_Reg kJson[] = {{"decode", json_decode}, {nullptr, nullptr}};

}  // namespace

void luaapp_open(lua_State* L)
{
    static const luaL_Reg safe[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {LUA_COLIBNAME, luaopen_coroutine},
        {nullptr, nullptr}};
    for (const luaL_Reg* lib = safe; lib->func; ++lib) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    // Drop the filesystem / code-loading escapes from _G.
    for (const char* g : {"dofile", "loadfile", "load", "require", "collectgarbage"}) {
        lua_pushnil(L);
        lua_setglobal(L, g);
    }

    reg(L, "g", kG);
    reg(L, "sys", kSys);
    reg(L, "store", kStore);
    reg(L, "http", kHttp);
    reg(L, "json", kJson);

    // `print` -> sys.log
    lua_pushcfunction(L, sys_log);
    lua_setglobal(L, "print");

    lua_sethook(L, timeout_hook, LUA_MASKCOUNT, 20000);
}

}  // namespace apps
