#include "installer.h"

#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <esp_log.h>

#include "catalog_client.h"
#include "hal_http.h"
#include "hal_storage.h"

namespace appstore {
namespace {

constexpr char TAG[] = "installer";
constexpr int kTimeoutMs = 10000;
constexpr char kStagePrefix[] = ".inst-";
constexpr char kOldPrefix[]   = ".old-";

std::string appsRoot() { return std::string(hal::storage::mountPoint()) + "/apps"; }

bool exists(const std::string& p)
{
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0;
}

bool isDir(const std::string& p)
{
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Depth-limited: app folders are at most three levels deep (validRelPath).
bool rmTree(const std::string& p, int depth = 0)
{
    if (!isDir(p)) return !exists(p) || ::unlink(p.c_str()) == 0;
    if (depth > 4) return false;
    if (DIR* d = ::opendir(p.c_str())) {
        struct dirent* ent;
        while ((ent = ::readdir(d)) != nullptr) {
            const std::string name = ent->d_name;
            if (name == "." || name == "..") continue;
            rmTree(p + "/" + name, depth + 1);
        }
        ::closedir(d);
    }
    return ::rmdir(p.c_str()) == 0;
}

bool mkdirs(const std::string& root, const std::string& rel_dir)
{
    std::string cur = root;
    size_t start = 0;
    while (start < rel_dir.size()) {
        size_t slash = rel_dir.find('/', start);
        if (slash == std::string::npos) slash = rel_dir.size();
        cur += "/" + rel_dir.substr(start, slash - start);
        if (!isDir(cur) && ::mkdir(cur.c_str(), 0775) != 0) return false;
        start = slash + 1;
    }
    return true;
}

bool copyFile(const std::string& from, const std::string& to)
{
    FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) return false;
    FILE* out = std::fopen(to.c_str(), "wb");
    if (!out) { std::fclose(in); return false; }
    char buf[512];
    size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0)
        if (std::fwrite(buf, 1, n, out) != n) { ok = false; break; }
    std::fclose(in);
    ok = std::fclose(out) == 0 && ok;
    return ok;
}

}  // namespace

Installer::State Installer::fail(const std::string& why, const std::string& file)
{
    error_ = why;
    if (file.empty()) ESP_LOGE(TAG, "%s: %s", entry_.m.id.c_str(), why.c_str());
    else ESP_LOGE(TAG, "%s/%s: %s", entry_.m.id.c_str(), file.c_str(), why.c_str());
    if (!staging_.empty()) rmTree(staging_);
    return state_ = State::Failed;
}

Installer::State Installer::begin(const std::string& base, const CatalogEntry& e)
{
    base_ = base;
    entry_ = e;
    next_ = 0;
    error_.clear();
    staging_.clear();
    state_ = State::Running;

    if (hal::storage::state() != hal::storage::State::Mounted) return fail("no SD card");
    const std::string root = appsRoot();
    if (!isDir(root) && ::mkdir(root.c_str(), 0775) != 0) return fail("cannot create /apps");

    const std::string stage = root + "/" + kStagePrefix + e.m.id;
    if (exists(stage) && !rmTree(stage)) return fail("cannot clear staging");
    if (::mkdir(stage.c_str(), 0775) != 0) return fail("cannot create staging");
    staging_ = stage;
    ESP_LOGI(TAG, "installing %s %s (%u files)", e.m.id.c_str(), e.m.version_str.c_str(),
             static_cast<unsigned>(e.files.size()));
    return state_;
}

Installer::State Installer::step()
{
    if (state_ != State::Running) return state_;
    if (hal::storage::state() != hal::storage::State::Mounted) return fail("no SD card");
    if (next_ >= entry_.files.size()) return finish();

    const CatalogFile& f = entry_.files[next_];
    const size_t slash = f.path.rfind('/');
    if (slash != std::string::npos && !mkdirs(staging_, f.path.substr(0, slash)))
        return fail("cannot create folder");

    const hal::http::Download d = hal::http::download(
        fileUrl(base_, entry_, f), staging_ + "/" + f.path, f.size, kTimeoutMs);
    if (!d.ok)                 return fail(d.error, f.path);
    if (d.size != f.size)      return fail("size mismatch", f.path);
    if (d.sha256 != f.sha256)  return fail("checksum mismatch", f.path);
    ++next_;
    return state_;
}

Installer::State Installer::finish()
{
    const std::string root = appsRoot();
    const std::string final_dir = root + "/" + entry_.m.id;
    const std::string old_dir = root + "/" + kOldPrefix + entry_.m.id;

    if (isDir(final_dir)) {
        const std::string state_file = final_dir + "/.state.txt";
        if (exists(state_file) && !copyFile(state_file, staging_ + "/.state.txt"))
            return fail("cannot keep saved state");
        if (exists(old_dir)) rmTree(old_dir);
        if (::rename(final_dir.c_str(), old_dir.c_str()) != 0) return fail("cannot replace app");
    }
    if (::rename(staging_.c_str(), final_dir.c_str()) != 0) {
        ::rename(old_dir.c_str(), final_dir.c_str());
        return fail("cannot replace app");
    }
    staging_.clear();
    if (exists(old_dir)) rmTree(old_dir);
    ESP_LOGI(TAG, "installed %s %s", entry_.m.id.c_str(), entry_.m.version_str.c_str());
    return state_ = State::Done;
}

void Installer::cancel()
{
    if (state_ == State::Running && !staging_.empty()) rmTree(staging_);
    staging_.clear();
    state_ = State::Idle;
}

bool removeApp(const std::string& id)
{
    if (!validId(id) || hal::storage::state() != hal::storage::State::Mounted) return false;
    const bool ok = rmTree(appsRoot() + "/" + id);
    ESP_LOGI(TAG, "removed %s: %s", id.c_str(), ok ? "ok" : "failed");
    return ok;
}

void recoverApps()
{
    if (hal::storage::state() != hal::storage::State::Mounted) return;
    const std::string root = appsRoot();
    DIR* d = ::opendir(root.c_str());
    if (!d) return;
    std::vector<std::string> stage, old;
    struct dirent* ent;
    while ((ent = ::readdir(d)) != nullptr) {
        const std::string name = ent->d_name;
        if (name.rfind(kStagePrefix, 0) == 0) stage.push_back(name);
        else if (name.rfind(kOldPrefix, 0) == 0) old.push_back(name);
    }
    ::closedir(d);

    for (const std::string& name : stage) {
        ESP_LOGW(TAG, "removing unfinished install %s", name.c_str());
        rmTree(root + "/" + name);
    }
    for (const std::string& name : old) {
        const std::string id = name.substr(sizeof(kOldPrefix) - 1);
        const std::string final_dir = root + "/" + id;
        if (validId(id) && !exists(final_dir)) {
            ESP_LOGW(TAG, "restoring %s from an interrupted update", id.c_str());
            if (::rename((root + "/" + name).c_str(), final_dir.c_str()) == 0) continue;
        }
        rmTree(root + "/" + name);
    }
}

}  // namespace appstore
