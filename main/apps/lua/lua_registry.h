/*
 * Discovery of SD-card Lua apps under /sd/apps/<id>/.
 *
 * Each app is a folder with an app.txt manifest ("key = value" lines, same
 * format as the language packs — see core/kv):
 *
 *     name    = 날씨
 *     name_en = Weather
 *     icon    = cloud          ; a gfx::uiIcons() name, or omit and ship icon.m5i
 *     entry   = main.lua       ; default main.lua
 *     order   = 20             ; home-grid sort key, ascending (default 100)
 *
 * The folder name is the app id. A folder without a readable app.txt is skipped.
 */
#pragma once

#include <string>

namespace apps {

struct SdAppInfo {
    std::string id;       // folder name
    std::string name;     // "name" (endonym)
    std::string name_en;  // "name_en" (fallback for the English UI)
    std::string icon;     // builtin icon name, or "" -> <dir>/icon.m5i
    std::string entry;    // Lua entry file, default "main.lua"
    std::string dir;      // "/sd/apps/<id>"
    int order = 100;
};

// Scan /sd/apps/*/app.txt. Fills `out` with up to `max` entries, sorted by
// `order` then `id`. Returns the count. 0 when the card is not mounted.
int scanSdApps(SdAppInfo* out, int max);

}  // namespace apps
