#include "catalog_client.h"

#include <cstdio>

#include <cJSON.h>
#include <esp_log.h>

#include "hal_http.h"
#include "hal_storage.h"
#include "kv.h"

namespace appstore {
namespace {

constexpr char TAG[] = "catalog";
constexpr char kDefaultBase[] = "https://jonpark0.github.io/leaf-os/catalog/v1/";
constexpr int kTimeoutMs = 10000;

std::string str(const cJSON* obj, const char* key)
{
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}

bool num(const cJSON* obj, const char* key, double& out)
{
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(v)) return false;
    out = v->valuedouble;
    return true;
}

bool parseEntry(const cJSON* a, CatalogEntry& e)
{
    AppManifest& m = e.m;
    m.id         = str(a, "id");
    m.name       = str(a, "name");
    m.name_en    = str(a, "name_en");
    m.summary    = str(a, "summary");
    m.summary_en = str(a, "summary_en");
    m.author     = str(a, "author");
    m.icon       = str(a, "icon");
    const std::string entry = str(a, "entry");
    if (!entry.empty()) m.entry = entry;
    m.version_str = str(a, "version");
    m.has_version = parseVersion(m.version_str, m.version);

    double d;
    if (!num(a, "api", d) || d < 1 || d > 1000) return false;
    m.api = static_cast<int>(d);

    const cJSON* net = cJSON_GetObjectItemCaseSensitive(a, "net");
    if (net && !cJSON_IsArray(net)) return false;
    const cJSON* h;
    cJSON_ArrayForEach(h, net) {
        if (!cJSON_IsString(h)) return false;
        m.net.emplace_back(h->valuestring);
    }

    const cJSON* files = cJSON_GetObjectItemCaseSensitive(a, "files");
    if (!cJSON_IsArray(files)) return false;
    const cJSON* f;
    cJSON_ArrayForEach(f, files) {
        CatalogFile cf;
        cf.path = str(f, "path");
        cf.sha256 = str(f, "sha256");
        if (!num(f, "size", d) || d < 0 || d > kMaxAppBytes) return false;
        cf.size = static_cast<size_t>(d);
        e.size += cf.size;
        e.files.push_back(std::move(cf));
        if (e.files.size() > kMaxFiles) return false;
    }
    return true;
}

}  // namespace

std::string catalogBase()
{
    std::string base = kDefaultBase;
    const std::string path = std::string(hal::storage::mountPoint()) + "/apps/catalog.txt";
    if (FILE* f = std::fopen(path.c_str(), "r")) {
        char buf[256];
        while (std::fgets(buf, sizeof(buf), f)) {
            std::string k, v;
            if (core::kv::split(core::kv::trim(buf), k, v) && k == "url" && !v.empty()) base = v;
        }
        std::fclose(f);
        ESP_LOGI(TAG, "catalog.txt overrides the catalog URL: %s", base.c_str());
    }
    if (base.back() != '/') base.push_back('/');
    return base;
}

bool fetchCatalog(const std::string& base, std::vector<CatalogEntry>& out, std::string& err)
{
    out.clear();
    const hal::http::Response r = hal::http::get(base + "index.json", kTimeoutMs, kMaxIndexBytes + 1);
    if (!r.ok)                          { err = r.error; return false; }
    if (r.status != 200)                { err = "HTTP " + std::to_string(r.status); return false; }
    if (r.body.size() > kMaxIndexBytes) { err = "index too large"; return false; }

    cJSON* root = cJSON_ParseWithLength(r.body.data(), r.body.size());
    if (!root) { err = "bad index"; return false; }

    double schema = 0;
    const cJSON* apps = cJSON_GetObjectItemCaseSensitive(root, "apps");
    if (!num(root, "schema", schema) || schema != 1 || !cJSON_IsArray(apps)) {
        cJSON_Delete(root);
        err = "unsupported index";
        return false;
    }

    const cJSON* a;
    cJSON_ArrayForEach(a, apps) {
        CatalogEntry e;
        std::string why = "malformed";
        if (!parseEntry(a, e) || !validateEntry(e, why)) {
            ESP_LOGW(TAG, "skipping catalog entry '%s': %s", e.m.id.c_str(), why.c_str());
            continue;
        }
        out.push_back(std::move(e));
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "%u app(s) in the catalog", static_cast<unsigned>(out.size()));
    return true;
}

std::string fileUrl(const std::string& base, const CatalogEntry& e, const CatalogFile& f)
{
    return base + "apps/" + e.m.id + "/" + e.m.version_str + "/" + f.path;
}

}  // namespace appstore
