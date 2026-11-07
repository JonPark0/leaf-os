/*
 * Host libraries exposed to an SD Lua app: g / sys / store / http / json.
 * See docs/APP_KIT.md for the surface. luaapp_open() also opens the safe
 * standard libs and installs the runaway-script hook.
 */
#pragma once

struct lua_State;

namespace apps {

// Open safe stdlibs (base/table/string/math/utf8/coroutine, minus
// dofile/loadfile/load) + the host libs, and install the timeout hook.
void luaapp_open(lua_State* L);

}  // namespace apps
