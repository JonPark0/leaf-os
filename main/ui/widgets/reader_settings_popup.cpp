#include "reader_settings_popup.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "i18n.h"
#include "screen.h"

namespace ui {
namespace {
using namespace core::i18n;

constexpr int kPanelW = 400;
constexpr int kPanelH = 320;
constexpr int kPad    = 24;
constexpr int kRowTop = 64;
constexpr int kRowH   = 58;
constexpr int kBtnW   = 44;
constexpr int kBtnH   = 40;
constexpr int kValGap = 72;
constexpr int kRows   = 3;

const char* rowLabel(int r)
{
    switch (r) {
        case 0:  return tr(READ_FONT_SIZE);
        case 1:  return tr(READ_MARGIN);
        default: return tr(READ_LINE_GAP);
    }
}

void box(core::Framebuffer& fb, core::Rect r, int t)
{
    fb.fillRect({r.x, r.y, r.w, t}, core::kBlack);
    fb.fillRect({r.x, r.bottom() - t, r.w, t}, core::kBlack);
    fb.fillRect({r.x, r.y, t, r.h}, core::kBlack);
    fb.fillRect({r.right() - t, r.y, t, r.h}, core::kBlack);
}

}  // namespace

void ReaderSettingsPopup::configure(std::vector<int> sizes, int font_px,
                                    int margin_px, int line_extra)
{
    sizes_ = std::move(sizes);
    std::sort(sizes_.begin(), sizes_.end());
    size_idx_ = -1;
    for (size_t i = 0; i < sizes_.size(); ++i) {
        if (sizes_[i] == font_px) { size_idx_ = static_cast<int>(i); break; }
    }
    if (size_idx_ < 0 && !sizes_.empty()) {
        // pick the closest available size
        int best = 0, bestd = 1 << 30;
        for (size_t i = 0; i < sizes_.size(); ++i) {
            const int d = std::abs(sizes_[i] - font_px);
            if (d < bestd) { bestd = d; best = static_cast<int>(i); }
        }
        size_idx_ = best;
    }
    margin_ = std::clamp(margin_px, 8, 64);
    line_extra_ = std::clamp(line_extra, 0, 16);
}

int ReaderSettingsPopup::fontPx() const
{
    if (size_idx_ < 0 || size_idx_ >= static_cast<int>(sizes_.size())) return -1;
    return sizes_[size_idx_];
}

core::Rect ReaderSettingsPopup::panel() const
{
    return {(core::Framebuffer::kW - kPanelW) / 2,
            (core::Framebuffer::kH - kPanelH) / 2, kPanelW, kPanelH};
}

void ReaderSettingsPopup::buildHits(std::vector<Hit>& out) const
{
    const core::Rect p = panel();
    for (int i = 0; i < kRows; ++i) {
        const int ry = p.y + kRowTop + i * kRowH;
        const int by = ry + (kRowH - kBtnH) / 2;
        const int plusX = p.right() - kPad - kBtnW;
        const int minusX = plusX - kBtnW - kValGap;
        out.push_back({{minusX, by, kBtnW, kBtnH}, i, -1});
        out.push_back({{plusX, by, kBtnW, kBtnH}, i, +1});
    }
    out.push_back({{p.x + kPad, p.bottom() - 56, p.w - 2 * kPad, 44}, -1, 0});
}

void ReaderSettingsPopup::bump(int row, int delta)
{
    bool changed = false;
    if (row == 0) {
        if (!sizes_.empty()) {
            const int ni = std::clamp(size_idx_ + delta, 0,
                                      static_cast<int>(sizes_.size()) - 1);
            changed = (ni != size_idx_);
            size_idx_ = ni;
        }
    } else if (row == 1) {
        const int nv = std::clamp(margin_ + delta * 4, 8, 64);
        changed = (nv != margin_);
        margin_ = nv;
    } else if (row == 2) {
        const int nv = std::clamp(line_extra_ + delta * 2, 0, 16);
        changed = (nv != line_extra_);
        line_extra_ = nv;
    }
    if (changed) {
        // Repaint only this row's value cell — a full-panel partial per tap is
        // needlessly slow on e-ink.
        if (screen_ != nullptr) {
            const core::Rect p = panel();
            const int ry = p.y + kRowTop + row * kRowH;
            const int vx = p.right() - kPad - kBtnW - kValGap;
            screen_->invalidateRect({vx, ry, kValGap, kRowH});
        } else {
            invalidate();
        }
        if (on_change_) on_change_();
    }
}

void ReaderSettingsPopup::draw(core::Framebuffer& fb)
{
    fb.fillRect(bounds_, core::kWhite);   // hide the page beneath
    if (font_ == nullptr || !font_->valid()) return;

    const core::Rect p = panel();
    fb.fillRect(p, core::kWhite);
    box(fb, p, 2);

    const int asc = font_->ascent();

    // title
    {
        const char* t = tr(READ_TITLE);
        const int tw = font_->measure(t);
        font_->draw(fb, p.x + (p.w - tw) / 2, p.y + 16 + asc, t, core::kBlack);
    }

    std::vector<Hit> hits;
    buildHits(hits);

    for (int i = 0; i < kRows; ++i) {
        const int ry = p.y + kRowTop + i * kRowH;
        font_->draw(fb, p.x + kPad, ry + (kRowH + asc) / 2 - 2, rowLabel(i), core::kBlack);

        char val[16];
        if (i == 0) {
            if (fontPx() < 0) std::snprintf(val, sizeof(val), "%s", tr(DEFAULT));
            else              std::snprintf(val, sizeof(val), "%d", fontPx());
        } else if (i == 1) {
            std::snprintf(val, sizeof(val), "%d", margin_);
        } else {
            std::snprintf(val, sizeof(val), "%d", line_extra_);
        }

        const core::Rect minusR = hits[i * 2].r;
        const core::Rect plusR  = hits[i * 2 + 1].r;
        box(fb, minusR, 1);
        box(fb, plusR, 1);
        const int mmw = font_->measure("-");
        const int ppw = font_->measure("+");
        font_->draw(fb, minusR.x + (minusR.w - mmw) / 2, minusR.y + (minusR.h + asc) / 2 - 2,
                    "-", core::kBlack);
        font_->draw(fb, plusR.x + (plusR.w - ppw) / 2, plusR.y + (plusR.h + asc) / 2 - 2,
                    "+", core::kBlack);
        const int vw = font_->measure(val);
        const int vcx = (minusR.right() + plusR.x) / 2;
        font_->draw(fb, vcx - vw / 2, minusR.y + (minusR.h + asc) / 2 - 2, val, core::kBlack);
    }

    // close
    const core::Rect cr = hits.back().r;
    box(fb, cr, 1);
    const char* cl = tr(CLOSE);
    const int clw = font_->measure(cl);
    font_->draw(fb, cr.x + (cr.w - clw) / 2, cr.y + (cr.h + asc) / 2 - 2, cl, core::kBlack);
}

bool ReaderSettingsPopup::onTouch(int x, int y, TouchPhase phase)
{
    std::vector<Hit> hits;
    buildHits(hits);

    if (phase == TouchPhase::Down) {
        down_row_ = -2;
        for (const Hit& h : hits) {
            if (h.r.contains(x, y)) { down_row_ = h.row; down_delta_ = h.delta; break; }
        }
        return true;   // modal: swallow everything
    }
    if (phase == TouchPhase::Move) return true;

    // Up
    bool onSame = false;
    for (const Hit& h : hits) {
        if (h.row == down_row_ && h.delta == down_delta_ && h.r.contains(x, y)) {
            onSame = true;
            break;
        }
    }
    const int row = down_row_;
    down_row_ = -2;

    if (onSame) {
        if (row == -1) { if (on_close_) on_close_(); }
        else if (row >= 0) { bump(row, down_delta_); }
        return true;
    }
    // A tap that hit nothing (and started on nothing) closes the panel.
    if (row == -2 && !panel().contains(x, y)) {
        if (on_close_) on_close_();
    }
    return true;
}

}  // namespace ui
