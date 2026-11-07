#include "catalog.h"

#include <algorithm>

namespace appstore {
namespace {

bool validSha(const std::string& s)
{
    if (s.size() != 64) return false;
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

}  // namespace

bool validateEntry(const CatalogEntry& e, std::string& why)
{
    const AppManifest& m = e.m;
    if (!validId(m.id))            { why = "bad id"; return false; }
    if (!m.has_version)            { why = "bad version"; return false; }
    if (m.api < 1)                 { why = "bad api"; return false; }
    if (m.name.empty())            { why = "no name"; return false; }
    for (const std::string& h : m.net)
        if (!validHost(h))         { why = "bad host " + h; return false; }
    if (e.files.empty())           { why = "no files"; return false; }
    if (e.files.size() > kMaxFiles) { why = "too many files"; return false; }

    size_t total = 0;
    bool has_manifest = false, has_entry = false;
    for (size_t i = 0; i < e.files.size(); ++i) {
        const CatalogFile& f = e.files[i];
        if (!validRelPath(f.path))     { why = "bad path " + f.path; return false; }
        if (f.size > kMaxFileBytes)    { why = "file too large " + f.path; return false; }
        if (!validSha(f.sha256))       { why = "bad sha256 " + f.path; return false; }
        for (size_t j = 0; j < i; ++j)
            if (e.files[j].path == f.path) { why = "duplicate " + f.path; return false; }
        total += f.size;
        has_manifest |= f.path == "app.txt";
        has_entry    |= f.path == m.entry;
    }
    if (total > kMaxAppBytes)      { why = "app too large"; return false; }
    if (!has_manifest)             { why = "no app.txt"; return false; }
    if (!has_entry)                { why = "no " + m.entry; return false; }
    return true;
}

Status status(const CatalogEntry& e, const AppManifest* installed, int api_level)
{
    const bool up_to_date = installed && installed->has_version &&
                            compareVersion(installed->version, e.m.version) >= 0;
    if (up_to_date) return Status::Installed;
    if (e.m.api > api_level) return Status::NeedsFirmware;
    return installed ? Status::UpdateAvailable : Status::NotInstalled;
}

std::vector<std::string> newHosts(const CatalogEntry& e, const AppManifest* installed)
{
    std::vector<std::string> out;
    for (const std::string& h : e.m.net) {
        const bool had = installed &&
            std::find(installed->net.begin(), installed->net.end(), h) != installed->net.end();
        if (!had) out.push_back(h);
    }
    return out;
}

}  // namespace appstore
