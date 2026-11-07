# `.m5f` font binary format (version 1)

A single `.m5f` file holds **one typeface at one pixel size**. The e-reader's
size menu (e.g. 16/20/24/28/32 px) is one file per size. Multi-byte fields are
**little-endian**. Glyph bitmaps are **row-major, MSB-first**.

Wire contract between `tools/fontgen/fontgen.py` (host) and
`main/gfx/font.{h,cpp}` (device). Bump `version` on any layout change.

```
offset  size  field
------  ----  -----------------------------------------------------------------
Header (32 bytes)
  0      4    magic            "M5F1"  (0x4D 0x35 0x46 0x31)
  4      2    version          = 1
  6      1    bpp              1 or 2  (bits per pixel in the bitmap section)
  7      1    flags            bit0 = fixed pitch (all advances equal)
  8      2    ascent           px above the baseline   (int16, >= 0)
 10      2    descent          px below the baseline   (int16, >= 0)
 12      2    line_gap         extra leading between lines (int16, >= 0)
 14      2    px_size          nominal pixel size the file was baked at
 16      2    range_count
 18      2    glyph_count
 20      4    range_table_off  byte offset from file start
 24      4    glyph_table_off  byte offset from file start
 28      4    bitmap_off       byte offset from file start

Range table (range_count entries, 12 bytes each, sorted ascending by first_cp,
non-overlapping). Codepoint c in [first_cp, last_cp] maps to
glyph index = glyph_base + (c - first_cp). Lookup is a binary search.
  +0     4    first_cp         (Unicode scalar value)
  +4     4    last_cp
  +8     4    glyph_base       (index into the glyph table)

Glyph table (glyph_count entries, 10 bytes each — struct "<IBBbbh")
  +0     4    bitmap_rel       byte offset from bitmap_off to this glyph's bits
  +4     1    w                bitmap width  in px  (0 for empty glyphs, e.g. space)
  +5     1    h                bitmap height in px
  +6     1    xoff             left bearing, px (int8; added to the pen x)
  +7     1    yoff             top of the bitmap relative to the baseline, px
                               (int8; negative = above the baseline)
  +8     2    advance          pen advance after this glyph, px (int16)

Bitmap section
  Concatenated glyph bitmaps. Each glyph occupies  row_bytes * h  bytes where
  row_bytes = ceil(w * bpp / 8). Rows are packed MSB-first:
    bpp = 1 : 1 = ink, 0 = background.
    bpp = 2 : 2-bit coverage per px, first px in the high bits of the byte.
              0 = none, 3 = full ink. The device maps 0..3 straight onto its
              gray levels (white/light/dark/black), giving anti-aliased text.
  A glyph with w == 0 or h == 0 (space, control) has no bitmap bytes; its
  bitmap_rel still points somewhere valid (usually 0) and is ignored.
```

## Notes

- **Sparse coverage.** CJK/Hangul use wide, sparse codepoint ranges; the range
  table keeps the index compact (a few entries) versus a flat 0x10000 table.
- **Endianness / packing** match the SSD1677 mono plane so a 1-bpp glyph can be
  blitted with shifts, no per-pixel division.
- The host tool validates its own output by re-parsing the file and (optionally)
  emitting a PNG proof sheet (`--verify --preview`).
- Baseline layout on device: for a line whose baseline is at `y`, draw each
  glyph's bitmap at `(pen_x + xoff, y + yoff)`, then `pen_x += advance`. Line
  advance is `ascent + descent + line_gap`.
