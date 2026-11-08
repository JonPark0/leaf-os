/*
 * Fetches the firmware feed and runs an update on a worker task.
 * See docs/design/ota-update.md.
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <string>

#include "firmware_feed.h"

namespace ota {

// Feed folder URL, ending in '/'. /sd/firmware.txt `url = …` overrides it.
std::string feedBase();

// GETs <base>latest.json and validates it against the update slot.
bool fetchFeed(const std::string& base, Feed& out, std::string& err);

// Downloads <base><feed.file> into the other app slot on its own task, checks
// size and SHA-256, and makes it the next boot. One update at a time; the
// object must outlive the task (a static, or wait for !running()).
class Updater {
public:
    enum class State { Idle, Running, Done, Failed, Cancelled };

    // False if one is already running or the task could not start.
    bool start(const std::string& base, const Feed& feed);
    // Asks a running update to stop; it ends in Cancelled.
    void cancel() { cancel_.store(true); }

    State  state() const { return state_.load(); }
    bool   running() const { return state() == State::Running; }
    size_t received() const { return received_.load(); }
    size_t total() const { return feed_.size; }
    // Valid once state() is Failed.
    const std::string& error() const { return error_; }

private:
    static void taskMain(void* self);
    void run();

    std::string base_;
    Feed feed_;
    std::string error_;
    std::atomic<State>  state_{State::Idle};
    std::atomic<size_t> received_{0};
    std::atomic<bool>   cancel_{false};
};

}  // namespace ota
