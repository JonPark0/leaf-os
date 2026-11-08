/*
 * Discovery of SD-card Lua apps under /sd/apps/<id>/.
 *
 * Each app is a folder with an app.txt manifest ("key = value" lines, parsed by
 * appstore::parseManifest; keys in docs/APP_KIT.md). The folder name is the app
 * id. A folder without a readable app.txt, or whose name is not a valid id, is
 * skipped; so are dot-folders (the installer's staging and backup folders).
 */
#pragma once

#include <string>

#include "manifest.h"

namespace apps {

struct SdAppInfo {
    appstore::AppManifest m;   // m.id = folder name, m.name defaults to the id
    std::string dir;           // "/sd/apps/<id>"
};

// "/sd/apps"
std::string sdAppsRoot();

// Read <dir>/app.txt. False when it is missing or unreadable.
bool readManifest(const std::string& dir, appstore::AppManifest& out);

// Scan /sd/apps/*/app.txt. Fills `out` with up to `max` entries, sorted by
// `order` then `id`. Returns the count. 0 when the card is not mounted.
int scanSdApps(SdAppInfo* out, int max);

}  // namespace apps
