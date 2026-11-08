/*
 * Fetch the app catalog (index.json) over HTTP(S). See docs/design/app-catalog.md.
 */
#pragma once

#include <string>
#include <vector>

#include "catalog.h"

namespace appstore {

// Catalog base URL, ending in '/'. /sd/apps/catalog.txt `url = ...` overrides
// the published one (for testing against a local server).
std::string catalogBase();

// Download and parse <base>index.json. Entries that fail validateEntry() are
// dropped and logged. False with `err` set when the index itself is unusable.
bool fetchCatalog(const std::string& base, std::vector<CatalogEntry>& out, std::string& err);

// <base>apps/<id>/<version>/<path>
std::string fileUrl(const std::string& base, const CatalogEntry& e, const CatalogFile& f);

}  // namespace appstore
