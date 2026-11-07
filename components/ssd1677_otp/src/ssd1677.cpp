/*
 * SSD1677 panel driver for PaperMono-Lite.  See ssd1677.h.
 *
 * Command sequences ported from m5stack/M5PaperMono-OTP-Demo
 * (components/EDP_OTP_LUT_demo/src/EDP_OTP_LUT_demo.cpp), MIT licensed,
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD.
 */
#include "ssd1677.h"

#include "edp_spi.h"

#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ssd1677 {
namespace {

constexpr char TAG[] = "ssd1677";

// SSD1677 commands.
constexpr uint8_t CMD_SOFT_RESET        = 0x12;
constexpr uint8_t CMD_DEEP_SLEEP        = 0x10;
constexpr uint8_t CMD_MASTER_ACTIVATION = 0x20;
constexpr uint8_t CMD_UPDATE_CONTROL    = 0x22;
constexpr uint8_t CMD_WRITE_RAM_1       = 0x24;
constexpr uint8_t CMD_WRITE_RAM_2       = 0x26;
constexpr uint8_t CMD_DATA_ENTRY_MODE   = 0x11;
constexpr uint8_t CMD_RAM_X_RANGE       = 0x44;
constexpr uint8_t CMD_RAM_Y_RANGE       = 0x45;
constexpr uint8_t CMD_RAM_X_COUNTER     = 0x4E;
constexpr uint8_t CMD_RAM_Y_COUNTER     = 0x4F;

bool s_baseline_ready = false;

Rect full_frame()
{
    return {0, 0, RAM_W, RAM_H};
}

// Snap x/width outward to a byte boundary; clamp to the frame. Returns false if
// the rect is empty or entirely off-frame. Kept for the future windowed-partial
// path (see refreshPartialMono).
[[maybe_unused]] bool align_rect(Rect& r)
{
    if (r.w == 0 || r.h == 0 || r.x >= RAM_W || r.y >= RAM_H) {
        return false;
    }
    const uint32_t right  = std::min<uint32_t>(RAM_W, static_cast<uint32_t>(r.x) + r.w);
    const uint32_t bottom = std::min<uint32_t>(RAM_H, static_cast<uint32_t>(r.y) + r.h);

    r.x = static_cast<uint16_t>(r.x & ~7u);
    r.w = static_cast<uint16_t>(((right + 7u) & ~7u) - r.x);
    r.h = static_cast<uint16_t>(bottom - r.y);
    return r.w != 0 && r.h != 0;
}

// All refresh modes must scan RAM the same way or the image flips on the
// physical vertical axis (RAM-x) when one waveform is used instead of another.
// The 4-gray path is fixed by silicon to stream RAM X decreasing / Y increasing
// (data-entry 0x02), so mono and partial match it here. The plane buffers are
// streamed in forward memory order; the pack step in display_manager already
// maps the framebuffer to that layout.
//
// NOTE: the windowed (row_len != RAM_STRIDE) path in write_ram() would need its
// byte order reversed to work with X-decreasing — it is currently dormant
// (partial always writes the full frame).
void set_ram_window(Rect r)
{
    const uint16_t x_start = static_cast<uint16_t>(r.x + r.w - 1);
    const uint16_t x_end   = static_cast<uint16_t>(r.x);
    const uint16_t y_start = r.y;
    const uint16_t y_end   = static_cast<uint16_t>(r.y + r.h - 1);

    edp_spi::write_register(CMD_DATA_ENTRY_MODE, {0x02});  // X--, Y++

    edp_spi::write_command(CMD_RAM_X_RANGE);
    edp_spi::write_u16(x_start);
    edp_spi::write_u16(x_end);

    edp_spi::write_command(CMD_RAM_Y_RANGE);
    edp_spi::write_u16(y_start);
    edp_spi::write_u16(y_end);

    edp_spi::write_command(CMD_RAM_X_COUNTER);
    edp_spi::write_u16(x_start);

    edp_spi::write_command(CMD_RAM_Y_COUNTER);
    edp_spi::write_u16(y_start);
}

// Stream `plane` (a full RAM_W x RAM_H MSB-packed buffer) into `command` RAM,
// windowed to `r`. `r` must already be byte-aligned.
void write_ram(uint8_t command, const uint8_t* plane, Rect r, bool invert = false)
{
    set_ram_window(r);
    edp_spi::write_command(command);

    const size_t x_off      = r.x / 8;
    const size_t row_len    = r.w / 8;
    const uint8_t* first_row = plane + static_cast<size_t>(r.y) * RAM_STRIDE + x_off;

    if (row_len == RAM_STRIDE) {
        edp_spi::write_data(first_row, row_len * r.h, invert);
        return;
    }
    for (uint16_t y = 0; y < r.h; ++y) {
        edp_spi::write_data(first_row + static_cast<size_t>(y) * RAM_STRIDE, row_len, invert);
    }
}

bool software_reset()
{
    // Full updates only. Never before a partial update or the RAM baseline dies.
    if (!edp_spi::wait_ready() || !edp_spi::begin_write()) {
        return false;
    }
    edp_spi::write_command(CMD_SOFT_RESET);
    if (!edp_spi::end_write()) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return edp_spi::wait_ready();
}

bool init_mono_mode()
{
    if (!software_reset() || !edp_spi::begin_write()) {
        return false;
    }
    edp_spi::write_register(0x18, {0x80});                          // internal temp sensor
    edp_spi::write_register(0x0C, {0xAE, 0xC7, 0xC3, 0xC0, 0x80});  // booster soft-start
    edp_spi::write_register(0x01, {0xDF, 0x01, 0x02});              // 480 gate outputs
    edp_spi::write_register(0x3C, {0x01});                          // border waveform
    edp_spi::write_register(0x21, {0x00});                          // normal RAM display
    set_ram_window(full_frame());
    return edp_spi::end_write();
}

bool init_gray_mode()
{
    edp_spi::hardware_reset();
    if (!software_reset() || !edp_spi::begin_write()) {
        return false;
    }
    edp_spi::write_register(0x0C, {0xAE, 0xC7, 0xC3, 0xC0, 0x80});
    edp_spi::write_register(0x01, {0xDF, 0x01, 0x02});

    // 4-gray streams RAM X decreasing, RAM Y increasing, full frame only.
    // set_ram_window() uses this same direction for mono/partial.
    set_ram_window(full_frame());

    edp_spi::write_register(0x3C, {0x01});  // border waveform
    edp_spi::write_register(0x18, {0x80});  // internal temp sensor
    edp_spi::write_register(0x1A, {0x5A});  // gray-mode temp value
    return edp_spi::end_write();
}

bool wake_for_partial()
{
    edp_spi::hardware_reset();  // exits deep sleep; no software reset here
    if (!edp_spi::wait_ready() || !edp_spi::begin_write()) {
        return false;
    }
    edp_spi::write_register(0x3C, {0x80});  // float the border during partial
    return edp_spi::end_write();
}

}  // namespace

bool baselineValid()
{
    return s_baseline_ready;
}

void deepSleep()
{
    if (!edp_spi::begin_write()) {
        return;
    }
    edp_spi::write_register(CMD_DEEP_SLEEP, {0x01});  // Mode 1: RAM retained
    edp_spi::end_write();
    vTaskDelay(pdMS_TO_TICKS(100));
}

bool init(const HwHooks& hooks)
{
    s_baseline_ready = false;
    if (!edp_spi::begin(hooks)) {
        ESP_LOGE(TAG, "transport init failed");
        return false;
    }

    // Load both planes white but issue no master activation: nothing flashes,
    // yet a valid mono baseline now exists for the first partial update.
    auto* white_plane = static_cast<uint8_t*>(
        heap_caps_malloc(PLANE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (white_plane == nullptr) {
        ESP_LOGE(TAG, "baseline plane alloc failed");
        return false;
    }
    std::fill(white_plane, white_plane + PLANE_BYTES, 0xFF);

    edp_spi::hardware_reset();
    bool ok = init_mono_mode() && edp_spi::begin_write();
    if (ok) {
        write_ram(CMD_WRITE_RAM_2, white_plane, full_frame());
        write_ram(CMD_WRITE_RAM_1, white_plane, full_frame());
        ok = edp_spi::end_write();
    }
    heap_caps_free(white_plane);
    if (!ok) {
        return false;
    }
    s_baseline_ready = true;
    deepSleep();
    ESP_LOGI(TAG, "init ok, white baseline loaded");
    return true;
}

bool refreshPartialMono(const uint8_t* plane, Rect /*rect*/)
{
    if (plane == nullptr || !s_baseline_ready) {
        ESP_LOGW(TAG, "partial refused: %s", plane ? "no baseline" : "null plane");
        return false;
    }
    if (!wake_for_partial() || !edp_spi::begin_write()) {
        s_baseline_ready = false;  // controller state unknown; force a full next
        deepSleep();
        return false;
    }
    // The 0xFF partial sequence updates from a full RAM1-vs-RAM2 differential
    // and swaps the planes on activation, so RAM1 must hold the COMPLETE new
    // frame every time. Writing only a sub-window leaves stale pixels outside
    // it (observed as alternating markers appearing/disappearing). `rect` is
    // kept in the signature for a future windowed path once the swap semantics
    // are pinned down.
    write_ram(CMD_WRITE_RAM_1, plane, full_frame());
    edp_spi::write_register(0x21, {0x00});
    edp_spi::write_register(CMD_UPDATE_CONTROL, {0xFF});  // built-in partial seq
    edp_spi::write_command(CMD_MASTER_ACTIVATION);
    if (!edp_spi::end_write() || !edp_spi::wait_ready()) {
        s_baseline_ready = false;  // RAM1/RAM2 swap state unknown
        deepSleep();
        return false;
    }
    deepSleep();
    return true;
}

// TODO(refresh-speed, see docs/REFRESH_MODES.md):
//  A) ~4 s here is two OTP passes. A single short absolute mono waveform
//     (port M5GFX lut_text: ~0.45 s) would replace this — custom waveform,
//     DC-balance risk, keep behind a build flag until bench-tested.
//  B) refreshPartialMono does hardware_reset()+deepSleep() every call; keeping
//     the panel awake across a burst would take partials ~0.7 s -> ~0.3 s.
//     No waveform risk. Deferred.
bool refreshFullMono(const uint8_t* plane)
{
    if (plane == nullptr) {
        return false;
    }
    s_baseline_ready = false;  // only a completed full refresh re-arms partials
    edp_spi::hardware_reset();
    if (!init_mono_mode() || !edp_spi::begin_write()) {
        deepSleep();
        return false;
    }

    // Phase 1: sync the drive state with an inverted frame.
    edp_spi::write_register(CMD_UPDATE_CONTROL, {0xF8});
    write_ram(CMD_WRITE_RAM_1, plane, full_frame(), /*invert=*/true);
    edp_spi::write_command(CMD_MASTER_ACTIVATION);
    if (!edp_spi::end_write() || !edp_spi::wait_ready()) {
        deepSleep();  // don't leave the analog rails up after a failed update
        return false;
    }

    // Phase 2: OTP Mode 1 waveform; both planes get the image so the partial
    // baseline is valid afterwards.
    if (!edp_spi::begin_write()) {
        deepSleep();
        return false;
    }
    edp_spi::write_register(CMD_UPDATE_CONTROL, {0x14});
    write_ram(CMD_WRITE_RAM_2, plane, full_frame());
    write_ram(CMD_WRITE_RAM_1, plane, full_frame());
    edp_spi::write_command(CMD_MASTER_ACTIVATION);
    if (!edp_spi::end_write() || !edp_spi::wait_ready()) {
        deepSleep();  // don't leave the analog rails up after a failed update
        return false;
    }

    s_baseline_ready = true;
    deepSleep();
    return true;
}

bool refreshFullGray(const uint8_t* plane1, const uint8_t* plane2)
{
    if (plane1 == nullptr || plane2 == nullptr) {
        return false;
    }
    s_baseline_ready = false;  // gray planes overwrite the mono baseline
    if (!init_gray_mode() || !edp_spi::begin_write()) {
        deepSleep();
        return false;
    }
    edp_spi::write_command(CMD_WRITE_RAM_1);
    edp_spi::write_data(plane1, PLANE_BYTES);
    edp_spi::write_command(CMD_WRITE_RAM_2);
    edp_spi::write_data(plane2, PLANE_BYTES);
    edp_spi::write_register(CMD_UPDATE_CONTROL, {0xD7});  // OTP 4-gray waveform
    edp_spi::write_command(CMD_MASTER_ACTIVATION);
    if (!edp_spi::end_write() || !edp_spi::wait_ready()) {
        deepSleep();  // don't leave the analog rails up after a failed update
        return false;
    }

    deepSleep();
    return true;
}

}  // namespace ssd1677
