#include "lua_registry.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

#include <esp_log.h>

#include "hal_storage.h"
#include "installer.h"

namespace apps {
namespace {
constexpr char TAG[] = "lua_registry";
constexpr long kMaxManifestBytes = 4096;
}  // namespace

std::string sdAppsRoot() { return std::string(hal::storage::mountPoint()) + "/apps"; }

bool readManifest(const std::string& dir, appstore::AppManifest& out)
{
    FILE* f = std::fopen((dir + "/app.txt").c_str(), "r");
    if (!f) return false;
    std::string text;
    char buf[256];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 &&
           text.size() < static_cast<size_t>(kMaxManifestBytes))
        text.append(buf, n);
    std::fclose(f);

    const size_t slash = dir.find_last_of('/');
    out = appstore::parseManifest(text);
    out.id = slash == std::string::npos ? dir : dir.substr(slash + 1);
    if (out.name.empty()) out.name = out.id;
    return true;
}

int scanSdApps(SdAppInfo* out, int max)
{
    if (hal::storage::state() != hal::storage::State::Mounted) return 0;

    appstore::recoverApps();   // finish or roll back an interrupted install

    const std::string root = sdAppsRoot();
    DIR* d = ::opendir(root.c_str());
    if (!d) return 0;

    int n = 0;
    struct dirent* ent;
    while (n < max && (ent = ::readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        if (!appstore::validId(ent->d_name)) {
            ESP_LOGW(TAG, "skipping /apps/%s: not a valid app id", ent->d_name);
            continue;
        }
        const std::string dir = root + "/" + ent->d_name;
        struct stat st{};
        if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        SdAppInfo info;
        info.dir = dir;
        if (!readManifest(dir, info.m)) { ESP_LOGD(TAG, "no app.txt in %s", dir.c_str()); continue; }
        out[n++] = std::move(info);
    }
    ::closedir(d);

    std::stable_sort(out, out + n, [](const SdAppInfo& a, const SdAppInfo& b) {
        return a.m.order != b.m.order ? a.m.order < b.m.order : a.m.id < b.m.id;
    });
    ESP_LOGI(TAG, "%d SD app(s)", n);
    return n;
}

}  // namespace apps
