# Roadmap — deferred / planned work

This file tracks planned work beyond the core firmware phases.

## Done since the original plan

- Reader font sizing: per-size `.m5f` from `/sd/fonts`, `ReaderSettingsPopup`
  (글자 크기 / 여백 / 줄 간격), re-paginate keeping scalar offset. See
  `docs`/`README` + memory `reader-fonts`.
- `app_books` bookshelf (home "책" tile); most-recently-read book pinned to top
  (`.pos` mtime; C clock seeded from RTC at boot / on every RTC write).
- Settings sub-pages: `app_display` (밝기 + 시스템/읽기 글꼴 family·크기 5
  steppers + 기본값 복원), `app_about` (버전 · OSS 고지 — `docs/THIRD_PARTY.md`),
  `app_power` (재시작 / 전원 끄기, 2-tap confirm).
- **Fonts overhaul:** embedded face is now NanumGothic 22 px only; every other
  face + all CJK on SD as `<script>-<family>-<px>.m5f` (`gfx/font_scan`).
  System font and reader font are configured independently (family + size).
  `core::Settings::restartPending()` → a 재시작하여 적용 row appears atop 설정
  when a boot-time setting (language / system font) changed.

## SD app kit — Lua (milestone 1 landed)

Reference:
`docs/APP_KIT.md`.

**Done:** `components/lua` (Lua 5.4.8 core, sandboxed) + `main/apps/lua/`
(`LuaApp` host, `g`/`sys`/`store`/`http`/`json` bindings, `app.txt` manifest
scan). `app_home` shows SD apps below a divider; `ui::AppTile` takes a per-tile
`icon.m5i`. Sample **weather** app (`apps/weather/`,
Open-Meteo dashboard with condition icons + 7-seg temperature). `http.get`
percent-encodes the URL; `store.*` file is now hidden `.state.txt`. The Clock /
Draw / Memo `PlaceholderApp` tiles were **removed from firmware** (registry) —
they return as SD Lua apps. `설정 → Wi-Fi` has a Wi-Fi on/off checkbox
(`setWifiAutoConnect` + `hal::wifi::disconnect`/`connectSaved`). Flash after:
~2.31 MB / ~300 KB free — headroom is thin.

**Milestone 2:**
1. **Ship clock / sketch / memo as bundled Lua apps** — the firmware tiles are
   gone; the SD replacements still need writing (`/sd/apps/{clock,draw,memo}/`).
2. **Peripheral permission model** — per-app grants for rarely-used interfaces
   (status LED, IMU, RTC alarm, front-light, raw I²C, SD write, Wi-Fi). Built-in
   apps get everything; a `설정 → 권한` page lists apps × capabilities. The
   binding layer is the enforcement point.
3. **On-screen keyboard binding** so an app can take text input (e.g. an
   arbitrary weather city) instead of a preset list / `state.txt` edit.
4. **`http.get` on a worker task** — today it blocks the input task up to 8 s.
5. **Flash budget** — Lua added ~360 KB in the `-Og` debug build; check the
   `-Os` release size, and consider trimming `lstrlib`/`ltablib` or moving the
   cert bundle to a smaller root set.

## Localization

5. **System language: 한국어 / English toggle — DONE.**
   - `core/i18n.{h,cpp}`: an X-macro list is the single source of truth for the
     `Msg` enum and the built-in `kKo` / `kEn` tables. `tr(Msg)` returns the
     active-language string (SD pack first, then the built-in).
   - `core::Settings::language` (0=ko / 1=en); `main.cpp` calls `i18n::setLang`
     before any app. Applied at boot — a `설정 → 언어` (`app_language`) page
     switches it; the note says "재시작 후 적용".
   - All ~70 visible UI literals across `apps/` + `ui/` now go through `tr()`.
     No font work — the embedded `.m5f` covers Latin-1 + Hangul.

6. **SD language packs — DONE for Latin + CJK; complex / RTL still out.**
   - `core/lang_pack.{h,cpp}`: `/sd/lang/<code>.lang` (`KEY = value`, `# code /
     # name / # font` metadata) parsed into an owned table and installed via
     `i18n::setPack`. English is the fall-back for omitted keys.
   - `core::Settings::langCode` (NVS `langc`); `app_language` lists 한국어 /
     English + every `/sd/lang/*.lang`. Applied at boot.
   - `# font = <script>` is the script for `/sd/fonts/<script>-<family>-<px>.m5f`
     (`gfx/font_scan`, `main.cpp pickUiFont()`).
   - `tools/langgen/langgen.py` dumps a template from `i18n.h`; samples
     `tools/langgen/samples/{ja,zh-Hans,zh-Hant}.lang`.
   - See `docs/LANGUAGE_PACKS.md`.
   - **Still out:** complex / RTL scripts (ar/hi/th) — `gfx/text_layout` is
     LTR-only with no glyph shaping / bidi; a HarfBuzz-class engine is a
     separate large effort. Per-*book* font memory (vs. the current global
     reader family/size) is a smaller follow-up.

7. **Per-language font family + system≠reader font — DONE.** Family axis added to
   the SD filename (`<script>-<family>-<px>.m5f`); 나눔 고딕/명조/펜 for ko,
   Noto sans/serif for CJK. `app_display` picks system and reader family/size
   separately. `restartPending()` drives the 설정 재시작 row.

## Power-off / sleep screen

**Done (minimal):** `app_power.cpp::drawPowerOffScreen()` — on a confirmed
전원 끄기 it paints white + a ×6-scaled centred "book" glyph straight into
`DisplayManager::fb()`, forces one synchronous full-mono `flush()`, then calls
`hal::power::shutdown()`. E-ink latches it after the rail is cut. If shutdown is
refused (external 5V, no battery) the page repaints over it.

**Still a sketch below** — bigger art, battery line, book context, and reusing
the same composer for a future deep-sleep.

E-ink keeps its last image with the rail cut, so "shutting down" = *compose a
full-screen image, do one full-mono flush, then cut power*. The same screen
serves a future deep-sleep.

**Mechanism**
1. `PowerApp` (confirmed 전원 끄기) or a sleep manager builds the framebuffer.
2. `core::DisplayManager` full-mono flush, **synchronously** — must finish the
   ~4 s update before power is cut. Add a `flushAndWait()` / `forceFullRefresh()`
   that blocks on BUSY, or poll `ssd1677` done.
3. Then `hal::power::shutdown()` (battery) / `esp_deep_sleep_start()`.
   Restart (`esp_restart`) needs none of this — boot repaints in ~1.5 s.
4. Skip the whole thing when `shutdown()` would refuse (external 5V, no battery)
   — the device won't actually go off.

**Where the code lives** — a free composer, not tied to an app, so deep-sleep
(no foreground app) can reuse it:

```cpp
namespace ui {
struct ShutdownInfo { const char* line1; const char* line2;
                      int battery_pct; const char* book_title; int page, pages; };
void drawShutdownScreen(core::Framebuffer&, const gfx::Font*, const ShutdownInfo&);
}
```

`PowerApp` fills it and calls it; later `hal`/sleep does the same. Needs the UI
font + `gfx::uiIcons()`; both are globals already.

**Content tiers**
- **v1 (text only, no new assets):** white ground, centred "전원이 꺼졌습니다"
  / "Power off" (i18n `PWR_OFF_SCREEN`), a power/leaf icon from the `.m5i`
  sheet, a small battery line. ~30 lines.
- **v2 (image):** embed `assets/sleep.m5i` (or a raw 1-bit 480×800 blob, 48 KB)
  via `EMBED_FILES` + a `blitFullscreen1bpp()` helper. Keep `--bpp 1`.
- **v3 (context):** if a book is the most-recent (`app_books` already finds it
  via `.pos` mtime — factor out `book::mostRecent()`), show its title + "읽던
  곳 p.42 / 310". Cover art waits on an image decoder (already deferred).

**Edge cases**
- Full flush counts against the DC-balance counter — fine, it *is* a full.
- If `holdFullRefresh(true)` is active (a modal), release it first.
- Deep-sleep wake redraws from scratch; the sleep image is just a placeholder
  until the kernel's first `flush()`.

## Fonts on the SD card

`/sd/fonts/<script>-<family>-<px>.m5f` — see `docs/LANGUAGE_PACKS.md` §2 and
memory `reader-fonts`. `<script>-<px>.m5f` = default family; `reader-<px>.m5f` =
`ko-gothic-<px>`. Resolver: `gfx/font_scan`. Shipped set staged in
`tools/fontgen/out/sd-bundle/`. Always `--bpp 1` (2-bpp forces the 4-gray
refresh).
