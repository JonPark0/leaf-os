/*
 * Paged vertical list. Paging (not smooth scroll) suits e-ink: one partial
 * refresh per page turn instead of dozens during a drag. The far-right 44 px
 * column is the pager — top half = previous page, bottom half = next page.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "font.h"
#include "widget.h"

namespace ui {

class ListView : public Widget {
public:
    explicit ListView(const gfx::Font* font = nullptr) : font_(font) {}

    void setFont(const gfx::Font* f) { font_ = f; invalidate(); }

    // Extra px added to every row's height (above + below the text), applied to
    // all lists. Set once at boot from core::Settings::uiRowPad(); the display
    // settings page also flips it live. Default 0.
    static void setGlobalRowPadding(int px);
    static int  globalRowPadding();

    void setItems(std::vector<std::string> items);
    void setItem(int i, const std::string& s);  // in place, repaints if visible
    int  count() const { return static_cast<int>(items_.size()); }

    void onSelect(std::function<void(int)> cb) { on_select_ = std::move(cb); }

    // Optional per-row badge: return an icon name (from gfx::uiIcons()) to draw
    // just left of the chevron, or nullptr for none.
    void iconFor(std::function<const char*(int)> cb) { icon_for_ = std::move(cb); }

    // Optional right-aligned secondary text, drawn left of the badge/chevron in
    // a fixed column so its spacing never shifts with the primary text length.
    // The primary text is ellipsized to whatever space is left.
    void detailFor(std::function<std::string(int)> cb) { detail_for_ = std::move(cb); }

    // Optional per-row font override (return nullptr to use the list font). Used
    // by the language picker to draw an endonym the UI font can't render.
    void fontFor(std::function<const gfx::Font*(int)> cb) { font_for_ = std::move(cb); }

    int rowHeight() const;
    int rowsPerPage() const;
    int page() const { return page_; }
    void setPage(int p);

    void draw(core::Framebuffer& fb) override;
    bool onTouch(int x, int y, TouchPhase phase) override;

private:
    static constexpr int kPagerW = 44;
    int pageCount() const;

    const gfx::Font* font_;
    std::vector<std::string> items_;
    std::function<void(int)> on_select_;
    std::function<const char*(int)> icon_for_;
    std::function<std::string(int)> detail_for_;
    std::function<const gfx::Font*(int)> font_for_;
    int page_ = 0;
    int down_row_ = -1;  // row under the press, -1 if in the pager / outside
};

}  // namespace ui
