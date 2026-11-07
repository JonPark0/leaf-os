#!/usr/bin/env python3
"""fontgen — bake a TTF/OTF into a .m5f binary for PaperMono-Lite.

    python fontgen.py --font NanumGothic-Regular.ttf --size 20 \
        --ranges latin,punctuation,hangul-syllables --bpp 1 --out ui20.m5f

    python fontgen.py --verify ui20.m5f --preview ui20.png
    python fontgen.py --list-ranges

Format: see docs/FONT_FORMAT.md. Requires `freetype-py` (`pip install freetype-py`).
`--preview` additionally needs `pillow`.
"""
import argparse
import struct
import sys

import ranges as rangemod

MAGIC = b"M5F1"
VERSION = 1
HEADER_SIZE = 32
RANGE_FMT = "<III"
GLYPH_FMT = "<IBBbbh"
RANGE_ENTRY = struct.calcsize(RANGE_FMT)   # 12
GLYPH_ENTRY = struct.calcsize(GLYPH_FMT)   # 10

HEADER_FMT = "<4sHBBhhhHHHIII"


def _clamp8(v: int) -> int:
    return max(-128, min(127, v))


def _pack_bitmap(bm, bpp: int) -> bytes:
    w, h, pitch = bm.width, bm.rows, bm.pitch
    if w == 0 or h == 0:
        return b""
    if pitch < 0:
        raise SystemExit("bottom-up FreeType bitmap not supported")
    buf = bm.buffer
    row_bytes = (w * bpp + 7) // 8
    out = bytearray()
    if bpp == 1:
        for y in range(h):
            base = y * pitch
            row = bytearray(row_bytes)
            for x in range(w):
                if (buf[base + (x >> 3)] >> (7 - (x & 7))) & 1:
                    row[x >> 3] |= 0x80 >> (x & 7)
            out += row
    else:  # bpp == 2, from an 8-bpp gray bitmap
        for y in range(h):
            base = y * pitch
            row = bytearray(row_bytes)
            for x in range(w):
                q = (buf[base + x] * 3 + 127) // 255       # 0..3
                row[x >> 2] |= q << (6 - 2 * (x & 3))      # first px in high bits
            out += row
    return bytes(out)


def build(font_path, px_size, range_spec, bpp, out_path, from_text=None):
    import freetype

    face = freetype.Face(font_path)
    face.set_pixel_sizes(0, px_size)

    asc = (face.size.ascender + 63) >> 6
    desc = (-face.size.descender + 63) >> 6
    height = (face.size.height + 63) >> 6
    line_gap = max(0, height - asc - desc)

    if from_text:
        with open(from_text, encoding="utf-8") as fh:
            cps = {ord(c) for c in fh.read() if ord(c) >= 0x20}
        cps.add(0x20)
    else:
        cps = set()
        for lo, hi in rangemod.resolve(range_spec):
            cps.update(range(lo, hi + 1))

    load_flags = freetype.FT_LOAD_RENDER
    if bpp == 1:
        load_flags |= freetype.FT_LOAD_TARGET_MONO

    glyphs = {}
    for cp in sorted(cps):
        if face.get_char_index(cp) == 0:
            continue
        face.load_char(cp, load_flags)
        g = face.glyph
        glyphs[cp] = {
            "w": g.bitmap.width,
            "h": g.bitmap.rows,
            "xoff": _clamp8(g.bitmap_left),
            "yoff": _clamp8(-g.bitmap_top),
            "adv": max(-32768, min(32767, (g.advance.x + 32) >> 6)),
            "bits": _pack_bitmap(g.bitmap, bpp),
        }

    if not glyphs:
        raise SystemExit("no glyphs matched the requested ranges")

    # contiguous runs of covered codepoints -> range table
    covered = sorted(glyphs)
    range_rows, glyph_rows, blob = [], [], bytearray()
    base = 0
    i = 0
    while i < len(covered):
        first = last = covered[i]
        j = i + 1
        while j < len(covered) and covered[j] == last + 1:
            last = covered[j]
            j += 1
        range_rows.append((first, last, base))
        for cp in range(first, last + 1):
            gl = glyphs[cp]
            glyph_rows.append((len(blob), gl["w"], gl["h"], gl["xoff"], gl["yoff"], gl["adv"]))
            blob += gl["bits"]
        base += last - first + 1
        i = j

    glyph_count = base
    flags = 1 if len({r[5] for r in glyph_rows}) == 1 else 0

    range_table_off = HEADER_SIZE
    glyph_table_off = range_table_off + len(range_rows) * RANGE_ENTRY
    bitmap_off = glyph_table_off + glyph_count * GLYPH_ENTRY

    with open(out_path, "wb") as fh:
        fh.write(struct.pack(HEADER_FMT, MAGIC, VERSION, bpp, flags,
                             asc, desc, line_gap, px_size,
                             len(range_rows), glyph_count,
                             range_table_off, glyph_table_off, bitmap_off))
        for row in range_rows:
            fh.write(struct.pack(RANGE_FMT, *row))
        for row in glyph_rows:
            fh.write(struct.pack(GLYPH_FMT, *row))
        fh.write(blob)

    total = bitmap_off + len(blob)
    print(f"{out_path}: {glyph_count} glyphs, {len(range_rows)} ranges, "
          f"{px_size}px bpp{bpp}, asc{asc}/desc{desc}/gap{line_gap}, {total} bytes")


# --------------------------------------------------------------------------- #

def load(path):
    with open(path, "rb") as fh:
        data = fh.read()
    (magic, ver, bpp, flags, asc, desc, gap, px, nranges, nglyphs,
     roff, goff, boff) = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != MAGIC or ver != VERSION:
        raise SystemExit(f"bad magic/version: {magic!r} v{ver}")
    ranges = [struct.unpack_from(RANGE_FMT, data, roff + k * RANGE_ENTRY)
              for k in range(nranges)]
    glyphs = [struct.unpack_from(GLYPH_FMT, data, goff + k * GLYPH_ENTRY)
              for k in range(nglyphs)]
    return dict(bpp=bpp, flags=flags, asc=asc, desc=desc, gap=gap, px=px,
                ranges=ranges, glyphs=glyphs, bitmap=data[boff:], raw=data)


def glyph_index(font, cp):
    lo, hi = 0, len(font["ranges"]) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        first, last, base = font["ranges"][mid]
        if cp < first:
            hi = mid - 1
        elif cp > last:
            lo = mid + 1
        else:
            return base + (cp - first)
    return -1


def verify(path, preview=None, sample=None):
    font = load(path)
    print(f"{path}: bpp{font['bpp']} {font['px']}px "
          f"asc{font['asc']}/desc{font['desc']}/gap{font['gap']} "
          f"{len(font['glyphs'])} glyphs {len(font['ranges'])} ranges "
          f"bitmap {len(font['bitmap'])} B")
    for first, last, base in font["ranges"]:
        assert 0 <= base and base + (last - first) < len(font["glyphs"]), "range OOB"
    for gi, (rel, w, h, xo, yo, adv) in enumerate(font["glyphs"]):
        rb = (w * font["bpp"] + 7) // 8
        assert rel + rb * h <= len(font["bitmap"]), f"glyph {gi} bitmap OOB"
    print("structure OK")

    if preview:
        _preview(font, preview, sample or "Ag 한글 Ｍ5 — hello 안녕하세요 0123")


def _preview(font, out_png, text):
    try:
        from PIL import Image
    except ImportError:
        raise SystemExit("--preview needs pillow (pip install pillow)")
    bpp = font["bpp"]
    line_h = font["asc"] + font["desc"] + font["gap"]
    pen_x, base_y = 4, font["asc"] + 4
    W = 4
    for ch in text:
        gi = glyph_index(font, ord(ch))
        W += font["glyphs"][gi][5] if gi >= 0 else font["px"] // 2
    img = Image.new("L", (max(W + 4, 64), line_h + 8), 255)
    px = img.load()
    for ch in text:
        gi = glyph_index(font, ord(ch))
        if gi < 0:
            pen_x += font["px"] // 2
            continue
        rel, w, h, xo, yo, adv = font["glyphs"][gi]
        rb = (w * bpp + 7) // 8
        for y in range(h):
            for x in range(w):
                if bpp == 1:
                    on = (font["bitmap"][rel + y * rb + (x >> 3)] >> (7 - (x & 7))) & 1
                    v = 0 if on else 255
                else:
                    q = (font["bitmap"][rel + y * rb + (x >> 2)] >> (6 - 2 * (x & 3))) & 3
                    v = 255 - q * 85
                if v != 255:
                    ix, iy = pen_x + xo + x, base_y + yo + y
                    if 0 <= ix < img.width and 0 <= iy < img.height:
                        px[ix, iy] = v
        pen_x += adv
    img.save(out_png)
    print(f"wrote {out_png} ({img.width}x{img.height})")


def main():
    ap = argparse.ArgumentParser(description="TTF/OTF -> .m5f")
    ap.add_argument("--font")
    ap.add_argument("--size", type=int)
    ap.add_argument("--ranges", default="latin")
    ap.add_argument("--from-text", help="subset to the characters in this UTF-8 file")
    ap.add_argument("--bpp", type=int, choices=(1, 2), default=1)
    ap.add_argument("--out")
    ap.add_argument("--verify")
    ap.add_argument("--preview")
    ap.add_argument("--sample")
    ap.add_argument("--list-ranges", action="store_true")
    a = ap.parse_args()

    if a.list_ranges:
        print(rangemod.list_presets())
        return
    if a.verify:
        verify(a.verify, a.preview, a.sample)
        return
    if not (a.font and a.size and a.out):
        ap.error("need --font, --size and --out (or --verify / --list-ranges)")
    build(a.font, a.size, a.ranges, a.bpp, a.out, a.from_text)


if __name__ == "__main__":
    sys.exit(main())
