#!/usr/bin/env python3
"""icongen — bake a set of SVGs into a .m5i 1-bit icon sheet for PaperMono-Lite.

    python icongen.py --src "../../icons/Dazzle Line Icons" --size 24 \
        --out ../../main/assets/icons.m5i \
        --pick wifi=wifi-svgrepo-com wifi-weak=wifi-weak-svgrepo-com ...

    python icongen.py --verify ../../main/assets/icons.m5i --preview icons.png

Renders each SVG with resvg (`pip install resvg-py`), converts to grayscale,
thresholds to 1-bit MSB-first, and packs everything into one blob.

.m5i layout (all little-endian):
    Header 12 B : magic "M5I1" | version u8 | flags u8 (bit0: 1bpp) |
                  count u16 | cell_px u16
    Table      : count * 24 B { name[16] (NUL-padded) | w u8 | h u8 |
                  stride u8 | pad u8 | data_off u32 }  -- data_off is from
                  the start of the file
    Bitmaps    : 1-bit rows, MSB-first, `stride` bytes per row, `h` rows
"""
import argparse
import struct
import sys
from pathlib import Path

MAGIC = b"M5I1"
VERSION = 1
FLAG_1BPP = 0x01
HEADER_FMT = "<4sBBHH"          # 10 bytes... pad to 12
HEADER_SIZE = 12
NAME_LEN = 16
ENTRY_FMT = f"<{NAME_LEN}sBBBBI"   # 24 bytes
ENTRY_SIZE = struct.calcsize(ENTRY_FMT)
assert ENTRY_SIZE == 24, ENTRY_SIZE


def _render_png(svg_path: Path, size: int) -> bytes:
    import resvg_py
    # resvg_py.svg_to_bytes returns a list[int] of PNG bytes.
    out = resvg_py.svg_to_bytes(
        svg_string=svg_path.read_text(encoding="utf-8"),
        width=size, height=size,
    )
    return bytes(out)


def _to_1bpp(png_bytes: bytes, size: int, threshold: int, invert: bool):
    from PIL import Image
    import io
    im = Image.open(io.BytesIO(png_bytes)).convert("RGBA")
    if im.size != (size, size):
        im = im.resize((size, size), Image.LANCZOS)
    # Composite onto white; a stroke icon is dark-on-transparent.
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.alpha_composite(im)
    g = bg.convert("L")
    px = g.load()
    stride = (size + 7) // 8
    rows = bytearray(stride * size)
    ink = 0
    for y in range(size):
        for x in range(size):
            dark = px[x, y] < threshold
            if invert:
                dark = not dark
            if dark:
                rows[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
                ink += 1
    return bytes(rows), stride, ink


def build(src: Path, sizes: list[int], picks: dict[str, str], threshold: int,
          invert: bool, out: Path):
    # One entry per (icon, size). The first size keeps the bare name; extra
    # sizes get a "name@<size>" suffix so a single sheet can hold both.
    entries = []  # (entry_name, size, svg_stem)
    for name, stem in picks.items():
        for i, sz in enumerate(sizes):
            entries.append((name if i == 0 else f"{name}@{sz}", sz, stem))
    if len(entries) > 65535:
        raise SystemExit("too many entries")

    table = bytearray()
    blob = bytearray()
    data_base = HEADER_SIZE + ENTRY_SIZE * len(entries)

    for ent_name, sz, stem in entries:
        svg = src / (stem if stem.endswith(".svg") else stem + ".svg")
        if not svg.is_file():
            raise SystemExit(f"missing SVG for '{ent_name}': {svg}")
        bits, stride, ink = _to_1bpp(_render_png(svg, sz), sz, threshold, invert)
        if ink == 0:
            print(f"  warning: '{ent_name}' rendered blank (tune --threshold)",
                  file=sys.stderr)
        off = data_base + len(blob)
        nm = ent_name.encode("ascii")
        if len(nm) >= NAME_LEN:
            raise SystemExit(f"name too long (max {NAME_LEN-1}): {ent_name}")
        table += struct.pack(ENTRY_FMT, nm, sz, sz, stride, 0, off)
        blob += bits
        print(f"  {ent_name:18s} {sz}x{sz} stride={stride} ink={ink}")

    hdr = struct.pack(HEADER_FMT, MAGIC, VERSION, FLAG_1BPP,
                      len(entries), sizes[0])
    hdr += b"\x00" * (HEADER_SIZE - len(hdr))
    out.write_bytes(hdr + bytes(table) + bytes(blob))
    print(f"wrote {out} ({len(hdr) + len(table) + len(blob)} bytes, "
          f"{len(entries)} entries)")


def verify(path: Path, preview: Path | None):
    data = path.read_bytes()
    magic, ver, flags, count, cell = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != MAGIC:
        raise SystemExit("bad magic")
    print(f"{path}: v{ver} flags={flags:#04x} count={count} cell={cell}")
    entries = []
    for i in range(count):
        nm, w, h, stride, _pad, off = struct.unpack_from(
            ENTRY_FMT, data, HEADER_SIZE + i * ENTRY_SIZE)
        name = nm.split(b"\x00")[0].decode("ascii")
        entries.append((name, w, h, stride, off))
        print(f"  {name:16s} {w}x{h} stride={stride} off={off}")

    if not preview:
        return
    from PIL import Image
    box = max((w for _n, w, _h, _s, _o in entries), default=cell)
    cols = min(8, count)
    rows = (count + cols - 1) // cols
    pad = 4
    sheet = Image.new("L", (cols * (box + pad) + pad,
                            rows * (box + pad) + pad), 255)
    for idx, (name, w, h, stride, off) in enumerate(entries):
        cx = pad + (idx % cols) * (box + pad)
        cy = pad + (idx // cols) * (box + pad)
        for y in range(h):
            for x in range(w):
                bit = (data[off + y * stride + (x >> 3)] >> (7 - (x & 7))) & 1
                if bit:
                    sheet.putpixel((cx + x, cy + y), 0)
    sheet.save(preview)
    print(f"preview -> {preview}")


# Curated selection: name on device  ->  SVG file stem in the Dazzle set.
DEFAULT_PICKS = {
    "wifi":        "wifi-svgrepo-com",
    "wifi-weak":   "wifi-weak-svgrepo-com",
    "wifi-fair":   "wifi-fair-svgrepo-com",
    "wifi-good":   "wifi-good-svgrepo-com",
    "wifi-off":    "wifi-exclamation-svgrepo-com",
    "lock":        "lock-svgrepo-com",
    "gear":        "gear-svgrepo-com",
    "folder":      "folder-svgrepo-com",
    "book":        "book-svgrepo-com",
    "clock":       "clock-five-svgrepo-com",
    "font":        "text-size-svgrepo-com",
    "note":        "note-sticky-svgrepo-com",
    "image":       "image-square-svgrepo-com",
    "batt-full":   "battery-full-svgrepo-com",
    "batt-mid":    "battery-mid-svgrepo-com",
    "batt-low":    "battery-low-svgrepo-com",
    "batt-empty":  "battery-empty-svgrepo-com",
    "batt-chg":    "battery-bolt-alt-svgrepo-com",
    "plus":        "plus-svgrepo-com",
    "minus":       "minus-svgrepo-com",
    "trash":       "trash-svgrepo-com",
    "power":       "power-off-svgrepo-com",
    "dots":        "dots-horizontal-svgrepo-com",
    "cloud":       "cloud-plain",
    "download":    "cloud-arrow-down-svgrepo-com",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", type=Path, help="folder of source SVGs")
    ap.add_argument("--size", type=int, nargs="+", default=[24],
                    help="one or more sizes; first is the bare name, rest get @N")
    ap.add_argument("--threshold", type=int, default=235,
                    help="grey < threshold becomes ink (0..255)")
    ap.add_argument("--invert", action="store_true")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--pick", nargs="*", default=None,
                    help="name=svgstem pairs; default = built-in curated set")
    ap.add_argument("--verify", type=Path)
    ap.add_argument("--preview", type=Path)
    args = ap.parse_args()

    if args.verify:
        verify(args.verify, args.preview)
        return

    if not args.src or not args.out:
        ap.error("--src and --out are required to build")
    picks = DEFAULT_PICKS
    if args.pick:
        picks = {}
        for pair in args.pick:
            k, _, v = pair.partition("=")
            picks[k] = v
    build(args.src, list(args.size), picks, args.threshold, args.invert, args.out)


if __name__ == "__main__":
    main()
