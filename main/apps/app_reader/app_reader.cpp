#include "app_reader.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <strings.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "display_manager.h"
#include "epub_parser.h"
#include "font_scan.h"
#include "hal_storage.h"
#include "i18n.h"
#include "kernel.h"
#include "lang_pack.h"
#include "settings.h"
#include "text_layout.h"
#include "txt_parser.h"
#include "xhtml_parser.h"

namespace apps {
namespace {
using namespace core::i18n;
constexpr char TAG[] = "app_reader";

// Vertical content box is fixed (the status bar overlays the reserved strip);
// only the horizontal margin is user-adjustable.
constexpr int kBoxY = 44, kBoxH = 740;
constexpr int kSaveEvery = 8;   // page turns between sidecar writes

// Script tag for the body font — follows the active language pack's "# font"
// so a book renders in the same script as the UI.
const char* bodyFontScript()
{
    const core::Settings& st = core::Settings::instance();
    if (!st.langCode().empty() && core::langPackFont()[0] != '\0')
        return core::langPackFont();
    return "ko";
}
}  // namespace

void ReaderApp::onCreate(const AppContext& ctx)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);
    view_.setBounds({0, 0, core::Framebuffer::kW, core::Framebuffer::kH});
    screen_.add(&view_);

    core::Settings& s = core::Settings::instance();
    margin_     = s.readMarginPx();
    line_extra_ = s.readLineExtra();

    scanFontSizes();
    if (!loadFontPx(s.readFontSize())) {
        font_ = ctx.ui_font;                 // no SD sizes -> embedded fallback
        font_px_ = 0;
    }

    makeBackButton(back_, ctx);              // {8, kHeight+8, 104, 42}
    back_.setVisible(!bar_hidden_);
    screen_.add(&back_);

    settings_btn_.setFont(ctx.ui_font);
    settings_btn_.setLabel(tr(APP_SETTINGS));
    settings_btn_.setBounds({core::Framebuffer::kW - 8 - 104, ui::StatusBar::kHeight + 8, 104, 42});
    settings_btn_.onClick([this] { openSettings(); });
    settings_btn_.setVisible(!bar_hidden_);
    screen_.add(&settings_btn_);

    // Modal settings panel. Added last so it paints on top and, with
    // full-screen bounds, captures every tap while shown. (The Screen's single
    // overlay slot is reserved by the kernel for the status bar.)
    popup_.setFont(ctx.ui_font);
    popup_.setBounds({0, 0, core::Framebuffer::kW, core::Framebuffer::kH});
    popup_.onChange([this] { applySettings(); });
    popup_.onClose([this] { closeSettings(); });
    popup_.setVisible(false);
    screen_.add(&popup_);

    loadAndPaginate();
}

void ReaderApp::onResume()
{
    core::Kernel::instance().statusBar().setVisible(!bar_hidden_);
    screen_.invalidateAll();
}

void ReaderApp::onPause()
{
    core::DisplayManager::instance().holdFullRefresh(false);
    core::Kernel::instance().statusBar().setVisible(true);  // restore for others
    savePos(true);
}

void ReaderApp::onDestroy() { savePos(true); }

void ReaderApp::scanFontSizes()
{
    sd_fonts_.clear();
    if (hal::storage::state() != hal::storage::State::Mounted) return;

    const char* script = bodyFontScript();
    const std::string& fam = core::Settings::instance().readFontFamily();
    const char* want_fam = fam.empty() ? gfx::defaultFamilyFor(script) : fam.c_str();

    const std::string dir = std::string(hal::storage::mountPoint()) + "/fonts";
    static gfx::SdFont buf[64];
    const int n = gfx::scanSdFonts(dir.c_str(), buf, 64);
    for (int i = 0; i < n; ++i) {
        if (strcasecmp(buf[i].script, script) != 0) continue;
        if (strcasecmp(buf[i].family, want_fam) != 0) continue;
        sd_fonts_.emplace_back(buf[i].px, buf[i].path);
    }
    // Fall back to the script default face if the chosen family has no files.
    if (sd_fonts_.empty()) {
        const char* def = gfx::defaultFamilyFor(script);
        for (int i = 0; i < n; ++i)
            if (strcasecmp(buf[i].script, script) == 0 && strcasecmp(buf[i].family, def) == 0)
                sd_fonts_.emplace_back(buf[i].px, buf[i].path);
    }
    std::sort(sd_fonts_.begin(), sd_fonts_.end());
    ESP_LOGI(TAG, "%d SD font(s) for %s/%s in %s",
             static_cast<int>(sd_fonts_.size()), script, want_fam, dir.c_str());
}

bool ReaderApp::loadFontPx(int px)
{
    if (sd_fonts_.empty()) return false;

    // exact, else nearest available
    int best = 0, bestd = 1 << 30;
    for (size_t i = 0; i < sd_fonts_.size(); ++i) {
        const int dd = std::abs(sd_fonts_[i].first - px);
        if (dd < bestd) { bestd = dd; best = static_cast<int>(i); }
    }
    if (font_ == &reader_font_ && font_px_ == sd_fonts_[best].first) return true;  // already loaded

    if (!reader_font_.loadFromFile(sd_fonts_[best].second.c_str())) {
        ESP_LOGW(TAG, "font load failed: %s", sd_fonts_[best].second.c_str());
        return false;
    }
    font_ = &reader_font_;
    font_px_ = sd_fonts_[best].first;
    return true;
}

void ReaderApp::loadAndPaginate()
{
    ok_ = false;
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) { ESP_LOGE(TAG, "open failed: %s", path_.c_str()); return; }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 12 * 1024 * 1024) { std::fclose(f); ESP_LOGE(TAG, "bad size %ld", sz); return; }
    std::string buf(static_cast<size_t>(sz), '\0');
    const size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    buf.resize(got);

    const char* base = std::strrchr(path_.c_str(), '/');
    const std::string name = base ? std::string(base + 1) : path_;
    const char* dot = std::strrchr(name.c_str(), '.');
    const bool is_epub = dot && !strcasecmp(dot, ".epub");

    book::setImagePlaceholder(tr(READER_IMG));   // localised <img> placeholder
    book_ = is_epub
        ? book::parseEpub(reinterpret_cast<const uint8_t*>(buf.data()), buf.size())
        : book::parseTxt(buf, name);
    if (!book_) { ESP_LOGE(TAG, "parse failed (%s)", is_epub ? "epub" : "txt"); return; }

    if (!font_ || !font_->valid()) { ESP_LOGE(TAG, "no usable font"); return; }

    gfx::PageMetrics m;
    m.width = boxW();
    m.height = kBoxH;
    m.line_extra = line_extra_;
    const int64_t t0 = esp_timer_get_time();
    pag_.paginate(*book_, *font_, m);
    ESP_LOGI(TAG, "paginate: %d pages in %lld ms",
             pag_.pageCount(), (esp_timer_get_time() - t0) / 1000);

    // Restore saved reading position (migrating a pre-dot sidecar if present).
    uint32_t off = 0;
    bool legacy = false;
    FILE* pf = std::fopen(posPath().c_str(), "r");
    if (!pf) {
        pf = std::fopen(legacyPosPath().c_str(), "r");
        legacy = pf != nullptr;
    }
    if (pf) {
        long v = 0;
        if (std::fscanf(pf, "%ld", &v) == 1 && v >= 0) off = static_cast<uint32_t>(v);
        std::fclose(pf);
    }
    // FatFs can't safely unlink an open file; drop the old sidecar after close.
    if (legacy) std::remove(legacyPosPath().c_str());
    page_ = pag_.pageForOffset(off);
    ok_ = true;
    ESP_LOGI(TAG, "'%s': %u runs, %d pages, resume %d/%d, font %s%d",
             book_->title().c_str(), static_cast<unsigned>(book_->runCount()),
             pag_.pageCount(), page_ + 1, pag_.pageCount(),
             font_px_ ? "" : "embedded ", font_px_);
}

void ReaderApp::repaginate()
{
    if (!ok_ || !book_ || !font_ || !font_->valid()) return;
    const uint32_t saved = pag_.page(page_).offset;   // capture BEFORE re-layout

    gfx::PageMetrics m;
    m.width = boxW();
    m.height = kBoxH;
    m.line_extra = line_extra_;
    const int64_t t0 = esp_timer_get_time();
    pag_.paginate(*book_, *font_, m);
    ESP_LOGI(TAG, "repaginate: %d pages in %lld ms",
             pag_.pageCount(), (esp_timer_get_time() - t0) / 1000);

    page_ = pag_.pageForOffset(saved);
    screen_.invalidateAll();
}

void ReaderApp::openSettings()
{
    std::vector<int> sizes;
    sizes.reserve(sd_fonts_.size());
    for (const auto& e : sd_fonts_) sizes.push_back(e.first);
    popup_.configure(std::move(sizes), font_px_, margin_, line_extra_);
    core::DisplayManager::instance().holdFullRefresh(true);
    popup_.setVisible(true);
    screen_.invalidateAll();
}

void ReaderApp::closeSettings()
{
    popup_.setVisible(false);
    if (layout_dirty_) { layout_dirty_ = false; repaginate(); }  // repaginate() invalidates
    else               { screen_.invalidateAll(); }
    core::DisplayManager::instance().holdFullRefresh(false);
}

// Fired on every stepper tap. Load the font / stash the metrics and persist, but
// defer the (visually expensive) re-pagination + full-page redraw to
// closeSettings() so stepping through values stays responsive.
void ReaderApp::applySettings()
{
    const int new_px    = popup_.fontPx();       // -1 when only the embedded font
    const int new_margin = popup_.marginPx();
    const int new_extra  = popup_.lineExtra();

    if (new_px > 0 && new_px != font_px_ && loadFontPx(new_px)) layout_dirty_ = true;
    if (new_margin != margin_)   { margin_     = new_margin; layout_dirty_ = true; }
    if (new_extra  != line_extra_) { line_extra_ = new_extra;  layout_dirty_ = true; }

    core::Settings& s = core::Settings::instance();
    if (font_px_ > 0) s.setReadFontSize(font_px_);
    s.setReadMarginPx(margin_);
    s.setReadLineExtra(line_extra_);
}

void ReaderApp::drawPage(core::Framebuffer& fb)
{
    fb.fillRect({0, 0, core::Framebuffer::kW, core::Framebuffer::kH}, core::kWhite);
    const int bx = margin_;
    const int bw = boxW();
    if (!ok_ || !font_ || !font_->valid() || pag_.pageCount() == 0) {
        if (font_ && font_->valid())
            font_->draw(fb, bx, kBoxY + font_->ascent(), tr(READER_OPEN_FAIL), core::kBlack);
        return;
    }

    drawBody(fb, bx, bw);

    // Page number, centred in the bottom margin. Drawn here (not inside
    // drawBody) so it is not skipped by drawBody's early returns on interior
    // pages.
    char pg[40];
    std::snprintf(pg, sizeof(pg), "%d / %d", page_ + 1, pag_.pageCount());
    const int pw = font_->measure(pg);
    font_->draw(fb, (core::Framebuffer::kW - pw) / 2,
                core::Framebuffer::kH - 6 - font_->descent(), pg, core::kBlack);
}

void ReaderApp::drawBody(core::Framebuffer& fb, int bx, int bw)
{
    const gfx::PagePos& start = pag_.page(page_);
    const gfx::PagePos* nextp = (page_ + 1 < pag_.pageCount()) ? &pag_.page(page_ + 1) : nullptr;

    const int lh = font_->lineHeight() + line_extra_;
    const int para_gap = pag_.metrics().para_gap;
    const int rule_h = pag_.metrics().rule_gap * 2 + 2;
    int y = kBoxY;

    static gfx::Line lines[600];

    for (uint32_t ri = start.run; ri < book_->runCount(); ++ri) {
        const book::TextRun& r = book_->run(ri);
        if (nextp && ri > nextp->run) break;

        if (ri != start.run && y > kBoxY) y += para_gap;

        if (r.rule) {
            const int ry = y + pag_.metrics().rule_gap;
            fb.fillRect({bx + 40, ry, bw - 80, 2}, core::kBlack);
            y += rule_h;
            if (y > kBoxY + kBoxH) break;
            continue;
        }

        const int n = gfx::wrap_text(*font_, r.text.c_str(), bw, lines, 600);
        const int first = (ri == start.run) ? static_cast<int>(start.line) : 0;
        for (int li = first; li < n; ++li) {
            if (nextp && ri == nextp->run && li >= static_cast<int>(nextp->line)) {
                return;  // reached the next page's start
            }
            if (y + lh > kBoxY + kBoxH) return;
            const int len = static_cast<int>(lines[li].end - lines[li].begin);
            font_->draw(fb, bx, y + font_->ascent(), lines[li].begin, core::kBlack, len);
            y += lh;
        }
    }
}

void ReaderApp::gotoPage(int p)
{
    if (p < 0) p = 0;
    if (p >= pag_.pageCount()) p = pag_.pageCount() - 1;
    if (p == page_) return;
    page_ = p;
    if (++turns_since_save_ >= kSaveEvery) savePos(false);
    screen_.invalidateAll();
}

void ReaderApp::toggleBar()
{
    bar_hidden_ = !bar_hidden_;
    core::Kernel::instance().statusBar().setVisible(!bar_hidden_);
    back_.setVisible(!bar_hidden_);
    settings_btn_.setVisible(!bar_hidden_);
    screen_.invalidateAll();
}

void ReaderApp::savePos(bool force)
{
    if (!ok_) return;
    if (!force && turns_since_save_ < kSaveEvery) return;
    turns_since_save_ = 0;
    if (FILE* pf = std::fopen(posPath().c_str(), "w")) {
        std::fprintf(pf, "%u\n", static_cast<unsigned>(pag_.page(page_).offset));
        std::fclose(pf);
    }
}

bool ReaderApp::View::onTouch(int x, int y, ui::TouchPhase phase)
{
    if (phase == ui::TouchPhase::Down) { down_x_ = x; down_y_ = y; return true; }
    if (phase != ui::TouchPhase::Up) return true;
    if (std::abs(x - down_x_) > 40 || std::abs(y - down_y_) > 40) return true;  // drag, ignore

    const int w = core::Framebuffer::kW;
    if (x < w * 3 / 10)        app_->gotoPage(app_->page_ - 1);
    else if (x > w * 7 / 10)   app_->gotoPage(app_->page_ + 1);
    else                       app_->toggleBar();
    return true;
}

}  // namespace apps
