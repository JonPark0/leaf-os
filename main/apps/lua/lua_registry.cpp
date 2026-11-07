#include "lua_registry.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include <esp_log.h>

#include "hal_storage.h"
#include "kv.h"

namespace apps {
namespace {
constexpr char TAG[] = "lua_registry";

bool loadManifest(const std::string& dir, SdAppInfo& info)
{
    const std::string path = dir + "/app.txt";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    char buf[256];
    while (std::fgets(buf, sizeof(buf), f)) {
        std::string line = core::kv::trim(buf);
        if (line.empty() || line[0] == '#') continue;
        std::string k, v;
        if (!core::kv::split(line, k, v)) continue;
        if      (k == "name")    info.name = v;
        else if (k == "name_en") info.name_en = v;
        else if (k == "icon")    info.icon = v;
        else if (k == "entry")   info.entry = v;
        else if (k == "order")   info.order = std::atoi(v.c_str());
    }
    std::fclose(f);
    if (info.entry.empty()) info.entry = "main.lua";
    return true;
}

}  // namespace

int scanSdApps(SdAppInfo* out, int max)
{
    if (hal::storage::state() != hal::storage::State::Mounted) return 0;

    const std::string root = std::string(hal::storage::mountPoint()) + "/apps";
    DIR* d = ::opendir(root.c_str());
    if (!d) return 0;

    int n = 0;
    struct dirent* ent;
    while (n < max && (ent = ::readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;
        const std::string dir = root + "/" + ent->d_name;
        struct stat st{};
        if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        SdAppInfo info;
        info.id = ent->d_name;
        info.dir = dir;
        if (!loadManifest(dir, info)) { ESP_LOGD(TAG, "no app.txt in %s", dir.c_str()); continue; }
        if (info.name.empty()) info.name = info.id;
        out[n++] = std::move(info);
    }
    ::closedir(d);

    std::stable_sort(out, out + n, [](const SdAppInfo& a, const SdAppInfo& b) {
        return a.order != b.order ? a.order < b.order : a.id < b.id;
    });
    ESP_LOGI(TAG, "%d SD app(s)", n);
    return n;
}

}  // namespace apps
