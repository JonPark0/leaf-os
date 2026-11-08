#!/usr/bin/env python3
"""Write the firmware update feed from a release's -update.zip.

    python tools/release/build_feed.py --tag v0.3.0 --zip leaf-os-v0.3.0-update.zip \
        [--notes URL] [--out site]

Writes <out>/firmware/v1/latest.json and <out>/firmware/v1/leaf-os-<tag>.bin
(the app image, leaf_os.bin, from the zip). The rules match main/ota/
firmware_feed.cpp; see docs/design/ota-update.md.
"""
import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = 1
TAG_RE = re.compile(r"^v\d{1,5}\.\d{1,5}\.\d{1,5}$")
SLOT_BYTES = 0x280000          # app0 / app1 in partitions.csv
MIN_IMAGE_BYTES = 64 * 1024
IMAGE_MAGIC = 0xE9             # ESP image header


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True)
    ap.add_argument("--zip", type=Path, required=True)
    ap.add_argument("--notes", default="")
    ap.add_argument("--out", type=Path, default=ROOT / "site")
    args = ap.parse_args()

    if not TAG_RE.match(args.tag):
        print(f"error: {args.tag} is not a release tag (vMAJOR.MINOR.PATCH)", file=sys.stderr)
        return 1
    with zipfile.ZipFile(args.zip) as z:
        names = [n for n in z.namelist() if n.endswith("/leaf_os.bin") and n.count("/") == 1]
        if len(names) != 1:
            print(f"error: no single <dir>/leaf_os.bin in {args.zip}", file=sys.stderr)
            return 1
        image = z.read(names[0])

    if image[0] != IMAGE_MAGIC:
        print("error: leaf_os.bin is not an ESP app image", file=sys.stderr)
        return 1
    if not MIN_IMAGE_BYTES <= len(image) <= SLOT_BYTES:
        print(f"error: image is {len(image)} bytes; the slot holds {SLOT_BYTES}", file=sys.stderr)
        return 1

    out = args.out / "firmware" / "v1"
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("leaf-os-*.bin"):
        old.unlink()
    file = f"leaf-os-{args.tag}.bin"
    (out / file).write_bytes(image)
    feed = {
        "schema": SCHEMA,
        "version": args.tag,
        "size": len(image),
        "sha256": hashlib.sha256(image).hexdigest(),
        "file": file,
        "notes": args.notes,
    }
    (out / "latest.json").write_text(json.dumps(feed, indent=1) + "\n", encoding="utf-8")
    print(f"{args.tag}: {len(image)} bytes, {feed['sha256']}")
    print(f"wrote {out / 'latest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
