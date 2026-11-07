/*
 * Catalog entries and the decisions made from them (validate, install status,
 * hosts added by an update). Platform-free; catalog_client fills the structs
 * from index.json.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "manifest.h"

namespace appstore {

// Limits shared with tools/apps/build_catalog.py.
constexpr size_t kMaxIndexBytes = 64 * 1024;
constexpr size_t kMaxFiles      = 32;
constexpr size_t kMaxFileBytes  = 256 * 1024;
constexpr size_t kMaxAppBytes   = 1024 * 1024;

struct CatalogFile {
    std::string path;     // relative to the app folder
    size_t      size = 0;
    std::string sha256;   // 64 lower-case hex digits
};

struct CatalogEntry {
    AppManifest m;        // id, names, version, api, net, ...
    size_t      size = 0; // sum of file sizes
    std::vector<CatalogFile> files;
};

// Checks id, version, hosts, paths, hashes and the limits above; also that
// app.txt and the entry script are among the files. `why` names the first
// problem.
bool validateEntry(const CatalogEntry& e, std::string& why);

enum class Status { NotInstalled, Installed, UpdateAvailable, NeedsFirmware };

// `installed` is the app.txt on the card, or nullptr when the app is absent.
Status status(const CatalogEntry& e, const AppManifest* installed, int api_level);

// Hosts in the entry's `net` that the installed version does not have (all of
// them when nothing is installed).
std::vector<std::string> newHosts(const CatalogEntry& e, const AppManifest* installed);

}  // namespace appstore
