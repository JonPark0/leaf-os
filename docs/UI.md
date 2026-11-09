# UI structure

How screens are put together, and the rules that keep them consistent on an
e-paper panel. See `ARCHITECTURE.md` for the layers below (display manager,
kernel loop).

## Layers

| Layer | Holds | Knows about |
|---|---|---|
| `core/kernel` | the app stack, touch and key routing, the status bar | `ui::Screen`, `apps::App` |
| `ui/` | `Widget`, `Screen`, `StatusBar`, `metrics.h`, text helpers | `core/` framebuffer and geometry, `gfx/` fonts |
| `ui/widgets/` | reusable widgets (below) | `ui/` |
| `apps/` | one folder per screen, `PageApp`, `app_common.h` | everything above |

`ui/text_wrap` takes a measure callback instead of a font, so it builds on the
host and `tests/test_ui` covers it.

## A screen

An ordinary screen derives from `apps::PageApp`. It owns the `Screen`, the
`AppMeta`, a back button and a title; the subclass adds its own widgets below
`ui::metrics::bodyTop()`.

```cpp
class FooApp : public PageApp {
public:
    FooApp() : PageApp("foo") {}
    void onCreate(const AppContext& ctx) override
    {
        setupPage(ctx, tr(FOO_TITLE));          // first: runs App::onCreate too
        list_.setFont(ctx.ui_font);
        list_.setBounds(ui::metrics::body());
        screen_.add(&list_);
    }
private:
    ui::ListView list_;
};
```

- Title styles: `Centered` (220 px, most pages), `CenteredWide` (240 px),
  `BesideBack` (left-aligned up to the right edge, e.g. a folder path), `None`.
- A different back action: set `back_.onClick(...)` after `setupPage`.
- A screen that swaps its widget set calls `screen_.clearWidgets()` and then
  `addHeader()`.
- Home, the reader and Lua apps draw their own chrome and derive from `App`.

## Geometry

Use `ui/metrics.h`, not numbers: `kScreenW`/`kScreenH` (480 x 800 portrait),
`kMargin`, `backButton()`, `titleCentered()`, `titleBesideBack()`,
`bodyTop()`, `body()`, `content()`. The status bar height comes from the UI
font at runtime, so these are read in `onCreate`, never cached in statics.

The status bar strip belongs to the system: `Screen` keeps every touch there
from the app, even though the bar takes none itself. Full-screen views (Lua
apps, the reader) rely on that, and the pull-down gesture will claim it.

## Widgets

| Widget | Use |
|---|---|
| `Label` | one line; text wider than the box ends in `...` (or starts with it, `setElideHead`, for paths) |
| `TextView` | read-only paragraphs: wraps, pages with a pager column and vertical swipes |
| `ListView` | tappable rows, paged, optional icon / detail / per-row font |
| `Button`, `Checkbox`, `TextField`, `Keyboard` | input |
| `AppTile` | home grid cell |

`ListView` and `TextView` share `ui/widgets/pager` (the right-hand page column)
and `ui/text_draw` (ellipsis), so both look the same.

## Rules

- **1-bit only.** Any gray pixel forces the slow 4-gray refresh. Draw in
  `kBlack` / `kWhite`.
- **Repaint through invalidation.** Change state, call the widget's setter;
  setters call `invalidate()`, and `setBounds` repaints both the old and the
  new area. `Screen::paint` clears the dirty area and redraws what intersects
  it. Do not draw outside your bounds.
- **Page, don't scroll.** One partial refresh per page turn.
- **Navigation is queued.** `ctx_.push` / `ctx_.pop` take effect at the top
  of the next tick, so they are safe inside a click handler. One push per tick;
  a second is dropped. A pop and a push in the same tick replace the top app;
  the app beneath stays paused.
- **Glyphs.** The embedded font lacks some symbols (U+2026 `…`, math minus,
  misc technical). Use ASCII (`...`, `-`), arrows, or an icon.
- **Text comes from `core::i18n::tr`.** Format with `formatArg` from
  `app_common.h` for a single `%s`.
