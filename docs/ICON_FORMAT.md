# `.m5i` — 1-bit icon sheet

Baked by `tools/icongen/icongen.py` from SVGs, read on device by
`gfx::IconSet` (`main/gfx/iconset.{h,cpp}`). One sheet holds N square icons at a
single pixel size; lookup is by short name.

## Layout (little-endian)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `"M5I1"` |
| 4 | 1 | version (`1`) |
| 5 | 1 | flags — bit0 `1` = 1 bpp (only mode today) |
| 6 | 2 | `count` (entries, not distinct icons — see multi-size) |
| 8 | 2 | `cell_px` (nominal / first icon size) |
| 10 | 2 | reserved (0) |
| 12 | `count`×24 | entry table |
| … | — | bitmap data |

### Entry (24 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 16 | name, ASCII, NUL-padded (max 15 chars) |
| 16 | 1 | `w` |
| 17 | 1 | `h` |
| 18 | 1 | `stride` (bytes per row = `ceil(w/8)`) |
| 19 | 1 | pad (0) |
| 20 | 4 | `data_off` — byte offset **from the start of the file** |

### Bitmap

`stride × h` bytes, rows top to bottom, **MSB-first** within each byte
(bit 7 = leftmost pixel). A set bit is ink; a clear bit is transparent when the
icon is blitted (`Framebuffer::blit1bpp`, `bg = kTransparent`).

## Multi-size

`--size` takes a list. The **first** size keeps the bare name (`book`); each
extra size adds a `@<size>` suffix (`book@48`). One sheet holds them all — the
per-entry `w`/`h`/`stride` fields already allow mixed sizes. The device resolver
just asks for the name it wants: `IconSet::find("book")` (24 px) or
`find("book@48")` (48 px). `ui::AppTile` tries `name@48` then falls back to
`name`.

## Building

```
cd tools/icongen
python icongen.py --src "../../icons/Dazzle Line Icons" --size 24 48 \
    --out ../../main/assets/icons.m5i
python icongen.py --verify ../../main/assets/icons.m5i --preview icons.png
```

`--pick name=svgstem …` overrides the curated selection in `DEFAULT_PICKS`.
`--threshold N` (default 235): source grey `< N` becomes ink — raise it to
fatten thin anti-aliased strokes at small sizes. Needs `resvg-py` and `pillow`
(`pip install resvg-py pillow`).

The embedded sheet is `main/assets/icons.m5i` (`EMBED_FILES` in
`main/CMakeLists.txt`), loaded in `main.cpp` and published via
`gfx::setUiIcons()`. Non-app widgets read it through `gfx::uiIcons()`.

## Current contents (24 px)

`wifi` `wifi-weak` `wifi-fair` `wifi-good` `wifi-off` `lock` `gear` `folder`
`book` `clock` `font` `note` `image` `batt-full` `batt-mid` `batt-low`
`batt-empty` `batt-chg` `plus` `minus` `trash` `power` `dots`

(`wifi-weak` renders nearly blank at 24 px 1-bit — use procedural bars for very
low signal.)
