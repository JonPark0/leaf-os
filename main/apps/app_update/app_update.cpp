#include "app_update.h"

#include <cstdio>
#include <vector>

#include <esp_log.h>

#include "hal_ota.h"
#include "hal_power.h"
#include "hal_sys.h"
#include "hal_wifi.h"
#include "i18n.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr char TAG[] = "app_update";
constexpr int kPaintTicks = 3;                 // let "checking…" paint before blocking
constexpr int kMinBatteryPct = 30;
constexpr int kPctStep = 5;                    // repaint progress every 5%
constexpr int64_t kRestartDelayUs = 2'500'000; // time to paint "restarting…"

// Outlives any one screen: its task may still be winding down after Back.
ota::Updater s_updater;

std::string fmt(const char* f, const std::string& a)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf), f, a.c_str());
    return buf;
}
}  // namespace

void UpdateApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);
    screen_.add(&back_);

    title_.setFont(ctx.ui_font);
    title_.setText(tr(SET_UPDATE));
    title_.setBounds({120, ui::StatusBar::kHeight + 10, 240, 40});
    title_.setAlign(ui::Label::Align::Center);
    screen_.add(&title_);

    const int lh = ctx.ui_font ? ctx.ui_font->lineHeight() : 30;
    const int btn_y = 800 - 64 - 3 * lh - 40;
    body_.setFont(ctx.ui_font);
    body_.setBounds({0, bodyTop(), 480, btn_y - bodyTop() - 12});
    screen_.add(&body_);

    button_.setFont(ctx.ui_font);
    button_.setBounds({140, btn_y, 200, 64});
    button_.onClick([this] { tapButton(); });
    screen_.add(&button_);

    status_.setFont(ctx.ui_font);
    status_.setBounds({16, btn_y + 64 + 16, 448, lh + 8});
    status_.setAlign(ui::Label::Align::Center);
    screen_.add(&status_);

    refresh();
}

void UpdateApp::onDestroy()
{
    if (s_updater.running()) {
        ESP_LOGW(TAG, "left the screen, cancelling the update");
        s_updater.cancel();
    }
}

void UpdateApp::refresh()
{
    std::vector<std::string> lines;
    lines.push_back(fmt(tr(UPD_CURRENT), hal::sys::firmwareVersion()));
    lines.push_back(fmt(tr(UPD_LATEST), has_feed_ ? feed_.version : std::string(tr(UPD_UNKNOWN))));
    if (has_feed_ && newer_ && !feed_.notes.empty()) {
        lines.emplace_back("");
        lines.emplace_back(tr(UPD_NOTES));
        lines.push_back(feed_.notes);
    }
    const std::string rolled = hal::ota::rolledBackVersion();
    if (!rolled.empty()) {
        lines.emplace_back("");
        lines.push_back(fmt(tr(UPD_ROLLED_BACK), rolled));
    }
    if (phase_ == Phase::Updating) {
        lines.emplace_back("");
        lines.emplace_back(tr(UPD_KEEP_ON));
    }
    body_.setLines(std::move(lines));

    switch (phase_) {
        case Phase::Idle:
        case Phase::Checking:   button_.setLabel(tr(UPD_CHECK)); break;
        case Phase::Checked:    button_.setLabel(newer_ ? tr(UPD_INSTALL) : tr(UPD_CHECK)); break;
        case Phase::Updating:   button_.setLabel(tr(UPD_CANCEL)); break;
        case Phase::Restarting: break;
    }
    button_.setVisible(phase_ != Phase::Checking && phase_ != Phase::Restarting);
}

void UpdateApp::tapButton()
{
    switch (phase_) {
        case Phase::Idle:
            check();
            return;
        case Phase::Checked:
            if (newer_) startUpdate(); else check();
            return;
        case Phase::Updating:
            s_updater.cancel();
            return;
        default:
            return;
    }
}

void UpdateApp::check()
{
    if (!hal::wifi::connected()) { status_.setText(tr(STORE_NO_WIFI)); return; }
    status_.setText(tr(UPD_CHECKING));
    phase_ = Phase::Checking;
    check_in_ticks_ = kPaintTicks;
    refresh();
}

void UpdateApp::startUpdate()
{
    if (!hal::wifi::connected()) { status_.setText(tr(STORE_NO_WIFI)); return; }
    if (hal::power::batteryPercent() < kMinBatteryPct) { status_.setText(tr(UPD_LOW_BATTERY)); return; }
    if (!s_updater.start(base_, feed_)) {
        status_.setText(fmt(tr(UPD_FAIL), s_updater.running() ? "busy" : s_updater.error()));
        return;
    }
    phase_ = Phase::Updating;
    last_pct_ = -1;
    char buf[48];
    std::snprintf(buf, sizeof(buf), tr(UPD_DOWNLOADING), 0);
    status_.setText(buf);
    refresh();
}

void UpdateApp::onTick()
{
    if (check_in_ticks_ > 0 && --check_in_ticks_ == 0) {
        check_in_ticks_ = -1;
        base_ = ota::feedBase();
        std::string err;
        has_feed_ = ota::fetchFeed(base_, feed_, err);
        if (has_feed_) {
            newer_ = ota::updateAvailable(hal::sys::firmwareVersion(), feed_.version);
            status_.setText(newer_ ? tr(UPD_AVAILABLE) : tr(UPD_UP_TO_DATE));
        } else {
            newer_ = false;
            ESP_LOGW(TAG, "feed: %s", err.c_str());
            status_.setText(netFailText(tr(UPD_CHECK_FAIL), err));
        }
        phase_ = has_feed_ ? Phase::Checked : Phase::Idle;
        refresh();
        return;
    }

    if (phase_ == Phase::Updating) {
        using S = ota::Updater::State;
        const S st = s_updater.state();
        if (st == S::Running) {
            const size_t total = s_updater.total();
            const int pct = total ? static_cast<int>(s_updater.received() * 100 / total) : 0;
            if (last_pct_ < 0 || pct - last_pct_ >= kPctStep) {
                last_pct_ = pct - pct % kPctStep;
                char buf[48];
                std::snprintf(buf, sizeof(buf), tr(UPD_DOWNLOADING), last_pct_);
                status_.setText(buf);
            }
        } else if (st == S::Done) {
            phase_ = Phase::Restarting;
            status_.setText(tr(UPD_RESTARTING));
            restart_at_us_ = hal::sys::nowUs() + kRestartDelayUs;
            refresh();
        } else {
            phase_ = Phase::Checked;
            status_.setText(st == S::Cancelled ? std::string(tr(UPD_CANCELLED))
                                               : netFailText(tr(UPD_FAIL), s_updater.error()));
            refresh();
        }
        return;
    }

    if (phase_ == Phase::Restarting && hal::sys::nowUs() >= restart_at_us_) {
        ESP_LOGI(TAG, "restarting into %s", feed_.version.c_str());
        hal::sys::restart();
    }
}

}  // namespace apps
