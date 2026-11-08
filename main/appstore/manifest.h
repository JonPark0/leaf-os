/*
 * SD app manifest (app.txt) parsing and validation. Platform-free: the home
 * grid, the Lua host and the catalog all read app.txt through here, and the
 * host tests cover it. See docs/APP_KIT.md for the keys.
 */
#pragma once

#include <string>
#include <vector>

namespace appstore {

struct Version {
    int major = 0, minor = 0, patch = 0;
};

// "MAJOR.MINOR.PATCH", each a decimal integer of 1..5 digits.
bool parseVersion(const std::string& s, Version& out);
// <0, 0, >0 like strcmp.
int compareVersion(const Version& a, const Version& b);

struct AppManifest {
    std::string id;            // folder name; not read from app.txt
    std::string name, name_en;
    std::string summary, summary_en;
    std::string author;
    std::string icon;
    std::string entry = "main.lua";
    std::string version_str;   // as written, "" when absent
    Version version;
    bool has_version = false;  // version_str parsed
    int api = 1;
    int order = 100;
    std::vector<std::string> net;   // lower-cased host names
};

// Parse app.txt text. Unknown keys are ignored; malformed values fall back to
// the defaults above (validate separately where that matters).
AppManifest parseManifest(const std::string& text);

// [a-z0-9-]{1,32}, not starting with '-'.
bool validId(const std::string& id);
// A DNS host name: labels of [a-z0-9-] (lower case), dot separated, <= 253.
bool validHost(const std::string& host);
// A file path inside an app folder: [A-Za-z0-9._/-], no empty / "." / ".."
// segment, no segment starting with '.', at most two directory levels.
bool validRelPath(const std::string& path);

}  // namespace appstore
