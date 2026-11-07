#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "book.h"
#include "font.h"
#include "paginator.h"
#include "widget.h"
#include "widgets/button.h"
#include "widgets/reader_settings_popup.h"

namespace apps {

// Full-screen reader (TXT / EPUB). Left third = previous page, right third =
// next, centre = toggle the status bar. Reading position (document scalar
// offset) is saved to a dot-prefixed ".<file>.pos" sidecar so it survives font
// size / margin changes.
//
// Body text renders with a per-size font from /sd/fonts (reader-<px>.m5f); the
// settings panel steps through whatever sizes are present. Falls back to the
// embedded UI font when the card has none.
class ReaderApp : public App {
public:
    explicit ReaderApp(std::string path) : path_(std::move(path)) {}

    void onCreate(const AppContext& ctx) override;
    void onResume() override;
    void onPause() override;
    void onDestroy() override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    class View : public ui::Widget {
    public:
        explicit View(ReaderApp* a) : app_(a) {}
        void draw(core::Framebuffer& fb) override { app_->drawPage(fb); }
        bool onTouch(int x, int y, ui::TouchPhase phase) override;
    private:
        ReaderApp* app_;
        int down_x_ = 0, down_y_ = 0;
    };

    void drawPage(core::Framebuffer& fb);
    void drawBody(core::Framebuffer& fb, int bx, int bw);   // may return early
    void gotoPage(int p);
    void toggleBar();
    void loadAndPaginate();
    void savePos(bool force);

    void scanFontSizes();          // enumerate /sd/fonts/*.m5f
    bool loadFontPx(int px);       // swap reader_font_ to that size; false => kept
    void openSettings();
    void closeSettings();
    void applySettings();          // pull popup_ values, reload font, repaginate
    void repaginate();             // rebuild the page table, keep reading position
    int  boxW() const { return core::Framebuffer::kW - 2 * margin_; }
    // Reading-position sidecar. Dot-prefixed on the file name so the file
    // manager (which hides ".*" entries) never shows it next to the book.
    std::string posPath() const
    {
        const size_t s = path_.find_last_of('/');
        return (s == std::string::npos)
                   ? "." + path_ + ".pos"
                   : path_.substr(0, s + 1) + "." + path_.substr(s + 1) + ".pos";
    }
    std::string legacyPosPath() const { return path_ + ".pos"; }

    AppMeta meta_{"reader-view", "책"};   // "reader" is the bookshelf (app_books)
    ui::Screen screen_;
    View view_{this};
    ui::Button back_;         // visible only while the status bar is shown
    ui::Button settings_btn_; //  "

    ui::ReaderSettingsPopup popup_;

    std::string path_;
    std::unique_ptr<book::Book> book_;
    gfx::Paginator pag_;

    gfx::Font   reader_font_;                       // owns an SD .m5f, or unused
    const gfx::Font* font_ = nullptr;               // -> reader_font_ or ctx.ui_font
    std::vector<std::pair<int, std::string>> sd_fonts_;  // (px, path), sorted
    int  font_px_    = 0;                           // 0 => embedded fallback
    int  margin_     = 24;
    int  line_extra_ = 0;

    int  page_ = 0;
    int  turns_since_save_ = 0;
    bool bar_hidden_ = true;
    bool ok_ = false;
    bool layout_dirty_ = false;   // a setting changed; repaginate on popup close
};

}  // namespace apps
