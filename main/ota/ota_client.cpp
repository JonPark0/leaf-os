#include "ota_client.h"

#include <cstdio>

#include <cJSON.h>
#include <esp_log.h>

#include "hal_http.h"
#include "hal_ota.h"
#include "hal_storage.h"
#include "hal_sys.h"
#include "kv.h"

namespace ota {
namespace {

constexpr char TAG[] = "ota";
constexpr char kDefaultBase[] = "https://jonpark0.github.io/leaf-os/firmware/v1/";
constexpr int kFeedTimeoutMs = 10000;
constexpr int kImageTimeoutMs = 15000;   // per network operation
constexpr uint32_t kTaskStack = 10 * 1024;
constexpr int kTaskPriority = 2;         // below the UI loop and render task
constexpr int kTaskCore = 0;

std::string str(const cJSON* obj, const char* key)
{
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}

}  // namespace

std::string feedBase()
{
    std::string base = kDefaultBase;
    const std::string path = std::string(hal::storage::mountPoint()) + "/firmware.txt";
    if (hal::storage::state() == hal::storage::State::Mounted) {
        if (FILE* f = std::fopen(path.c_str(), "r")) {
            char buf[256];
            while (std::fgets(buf, sizeof(buf), f)) {
                std::string k, v;
                if (core::kv::split(core::kv::trim(buf), k, v) && k == "url" && !v.empty()) base = v;
            }
            std::fclose(f);
            ESP_LOGI(TAG, "firmware.txt overrides the feed URL: %s", base.c_str());
        }
    }
    if (base.back() != '/') base.push_back('/');
    return base;
}

bool fetchFeed(const std::string& base, Feed& out, std::string& err)
{
    const hal::http::Response r = hal::http::get(base + "latest.json", kFeedTimeoutMs, kMaxFeedBytes + 1);
    if (!r.ok)                         { err = r.error; return false; }
    if (r.status != 200)               { err = "HTTP " + std::to_string(r.status); return false; }
    if (r.body.size() > kMaxFeedBytes) { err = "feed too large"; return false; }

    cJSON* root = cJSON_ParseWithLength(r.body.data(), r.body.size());
    if (!root) { err = "bad feed"; return false; }
    const cJSON* schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON* size = cJSON_GetObjectItemCaseSensitive(root, "size");
    if (!cJSON_IsNumber(schema) || schema->valuedouble != 1 || !cJSON_IsNumber(size) ||
        size->valuedouble < 0 || size->valuedouble > 64.0 * 1024 * 1024) {
        cJSON_Delete(root);
        err = "unsupported feed";
        return false;
    }
    Feed f;
    f.version = str(root, "version");
    f.size = static_cast<size_t>(size->valuedouble);
    f.sha256 = str(root, "sha256");
    f.file = str(root, "file");
    f.notes = str(root, "notes");
    cJSON_Delete(root);

    if (!validateFeed(f, hal::ota::nextSlotBytes(), err)) return false;
    ESP_LOGI(TAG, "feed: %s, %u bytes", f.version.c_str(), static_cast<unsigned>(f.size));
    out = std::move(f);
    return true;
}

bool Updater::start(const std::string& base, const Feed& feed)
{
    State expected = state_.load();
    if (expected == State::Running) return false;
    base_ = base;
    feed_ = feed;
    error_.clear();
    received_.store(0);
    cancel_.store(false);
    state_.store(State::Running);
    if (!hal::sys::spawnPinned(taskMain, "ota", kTaskStack, this, kTaskPriority, kTaskCore)) {
        error_ = "task";
        state_.store(State::Failed);
        return false;
    }
    return true;
}

void Updater::taskMain(void* self)
{
    static_cast<Updater*>(self)->run();
    hal::sys::endTask();
}

void Updater::run()
{
    hal::ota::Writer w;
    auto fail = [&](const std::string& why) {
        w.abort();
        ESP_LOGE(TAG, "update to %s failed: %s", feed_.version.c_str(), why.c_str());
        error_ = why;
        state_.store(State::Failed);
    };

    if (!w.begin(feed_.size)) return fail(w.error());
    ESP_LOGI(TAG, "downloading %s%s", base_.c_str(), feed_.file.c_str());
    const hal::http::Download d = hal::http::stream(
        base_ + feed_.file, feed_.size, kImageTimeoutMs, [&](const char* data, size_t n) {
            if (cancel_.load()) return false;
            if (!w.write(data, n)) return false;
            received_.fetch_add(n);
            return true;
        });

    if (cancel_.load()) {
        w.abort();
        ESP_LOGW(TAG, "update cancelled");
        state_.store(State::Cancelled);
        return;
    }
    if (!d.ok)                     return fail(d.error == "sink" ? w.error() : d.error);
    if (d.size != feed_.size)      return fail("size mismatch");
    if (d.sha256 != feed_.sha256)  return fail("checksum mismatch");
    if (!w.end())                  return fail(w.error());
    if (!w.setBoot())              return fail(w.error());
    ESP_LOGI(TAG, "%s written and verified", feed_.version.c_str());
    state_.store(State::Done);
}

}  // namespace ota
