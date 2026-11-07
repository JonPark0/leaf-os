/*
 * Install / update / delete an SD app from a catalog entry.
 *
 * Files land in /sd/apps/.inst-<id>/ and are checked against the catalog's
 * size and SHA-256; only then is the installed folder swapped out
 * (<id> -> .old-<id>, .inst-<id> -> <id>, delete .old-<id>). The installed
 * app's .state.txt moves across. A failure at any point leaves the installed
 * version in place; recoverApps() finishes or rolls back a swap that was cut
 * short (card pulled, power lost).
 */
#pragma once

#include <cstddef>
#include <string>

#include "catalog.h"

namespace appstore {

class Installer {
public:
    enum class State { Idle, Running, Done, Failed };

    // Prepare the staging folder. Failed (with error()) when that cannot be done.
    State begin(const std::string& base, const CatalogEntry& e);

    // Download the next file, or swap the folders once all are in. Call once
    // per tick while state() is Running.
    State step();

    State state() const { return state_; }
    int done() const { return static_cast<int>(next_); }
    int total() const { return static_cast<int>(entry_.files.size()); }
    const std::string& error() const { return error_; }

    // Drop the staging folder (e.g. the card went away or the screen closed).
    void cancel();

private:
    State fail(const std::string& why);
    State finish();

    State        state_ = State::Idle;
    std::string  base_, error_, staging_;
    CatalogEntry entry_;
    size_t       next_ = 0;
};

// Remove /sd/apps/<id> and everything in it, including saved state.
bool removeApp(const std::string& id);

// Clean up after an interrupted install: restore <id> from .old-<id> when <id>
// is missing, and delete leftover .inst-* / .old-* folders.
void recoverApps();

}  // namespace appstore
