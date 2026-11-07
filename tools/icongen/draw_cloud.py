#!/usr/bin/env python3
"""Bake a filled "cloud" glyph into a one-icon .m5i sheet (name "cloud" +
"cloud@48"). Used for the sample weather SD app; needs only Pillow.

    python draw_cloud.py <out.m5i>
"""
import struct
import sys
from PIL import Image, ImageDraw


def cloud_bitmap(px: int) -> Image.Image:
    im = Image.new("1", (px, px), 1)  # 1 = white
    d = ImageDraw.Draw(im)
    s = px / 24.0
    # three bumps + a slab, all filled ink (0)
    d.ellipse([2 * s, 9 * s, 12 * s, 19 * s], fill=0)
    d.ellipse([7 * s, 5 * s, 17 * s, 17 * s], fill=0)
    d.ellipse([12 * s, 9 * s, 21 * s, 18 * s], fill=0)
    d.rectangle([5 * s, 14 * s, 19 * s, 19 * s], fill=0)
    return im


def pack_rows(im: Image.Image):
    w, h = im.size
    stride = (w + 7) // 8
    data = bytearray()
    px = im.load()
    for y in range(h):
        row = bytearray(stride)
        for x in range(w):
            if px[x, y] == 0:  # ink
                row[x >> 3] |= 0x80 >> (x & 7)
        data += row
    return w, h, stride, bytes(data)


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "icon.m5i"
    icons = [("cloud", cloud_bitmap(24)), ("cloud@48", cloud_bitmap(48))]

    table = bytearray()
    blobs = bytearray()
    header_len = 12
    table_len = 24 * len(icons)
    off = header_len + table_len
    for name, im in icons:
        w, h, stride, data = pack_rows(im)
        nm = name.encode("ascii")[:16].ljust(16, b"\0")
        table += struct.pack("<16sBBBBI", nm, w, h, stride, 0, off)
        blobs += data
        off += len(data)

    with open(out, "wb") as f:
        # 12-byte header: magic | ver u8 | flags u8 | count u16 | cell u16 | pad u16
        f.write(struct.pack("<4sBBHHH", b"M5I1", 1, 0x01, len(icons), 24, 0))
        f.write(table)
        f.write(blobs)
    print(f"{out}: {len(icons)} icons, {off} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
