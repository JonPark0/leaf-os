// C++ include wrapper for Lua.
//
// luaconf.h picks lua_Integer = 'long long' and then #errors unless LLONG_MAX
// is visible. Under the ESP-IDF C++ toolchain <climits> does not always expose
// it at the point luaconf.h is processed, so pin the C99 macros here (matching
// what the C-compiled Lua core got) before including the C headers.
#pragma once

#include <climits>
#include <cstdint>

#if !defined(LLONG_MAX)
#define LLONG_MAX   __LONG_LONG_MAX__
#define LLONG_MIN   (-__LONG_LONG_MAX__ - 1LL)
#define ULLONG_MAX  (__LONG_LONG_MAX__ * 2ULL + 1ULL)
#endif

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
