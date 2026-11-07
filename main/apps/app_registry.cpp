#include "app_registry.h"

#include "app_books/app_books.h"
#include "app_files/app_files.h"
#include "app_settings/app_settings.h"
#include "hal_storage.h"
#include "i18n.h"

namespace apps {
namespace {
using namespace core::i18n;

App* makeSettings() { return new SettingsApp(); }
App* makeReader()   { return new BooksApp(); }
App* makeFiles()    { return new FilesApp(); }

// Clock / Draw / Memo used to be built-in placeholder tiles; they are being
// reimplemented as SD Lua apps (/sd/apps/<id>/), so they no longer ship in the
// firmware. The APP_CLOCK / APP_SKETCH / APP_MEMO i18n strings are kept for
// those SD apps' manifests.

// Only offer the file manager once the card is actually mounted — a card that
// is present (DET=1) but failed to mount (cold-boot 0x108) would otherwise show
// a tile that pops straight back out. hal::storage::poll() keeps re-trying, so
// the tile appears a few seconds later once the mount succeeds.
bool sdMounted()    { return hal::storage::state() == hal::storage::State::Mounted; }

const BuiltinApp kApps[] = {
    {"reader",   APP_BOOKS,    "book",   makeReader,   nullptr},
    {"files",    APP_FILES,    "folder", makeFiles,    sdMounted},  // only when mounted
    {"settings", APP_SETTINGS, "gear",   makeSettings, nullptr},
};

}  // namespace

const BuiltinApp* builtinApps(int& count)
{
    count = sizeof(kApps) / sizeof(kApps[0]);
    return kApps;
}

}  // namespace apps
