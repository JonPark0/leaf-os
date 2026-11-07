#!/usr/bin/env python3
"""langgen — dump a .lang translation template from main/core/i18n.h.

    python langgen.py > ja.lang        # English values, ready to translate
    python langgen.py --ko > xx.lang   # Korean values instead

A .lang file is loaded by core/lang_pack.cpp:

    # code = ja
    # name = 日本語
    # font = ja            ; script for /sd/fonts/<script>-<family>-<px>.m5f
    BACK = <translation>
    ...

Only "KEY = value" lines matter; unknown/omitted keys fall back to the built-in
English string. Copy the result to the SD card as /sd/lang/<code>.lang.
"""
import argparse
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
I18N_H = HERE.parent.parent / "main" / "core" / "i18n.h"

# X(NAME, "korean", "english")  — tolerant of whitespace / line continuations
ROW = re.compile(
    r'X\(\s*([A-Z0-9_]+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)'
)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ko", action="store_true", help="seed with the Korean values")
    ap.add_argument("--i18n", type=pathlib.Path, default=I18N_H)
    args = ap.parse_args()

    text = args.i18n.read_text(encoding="utf-8")
    rows = ROW.findall(text)
    if not rows:
        print(f"no X(...) rows found in {args.i18n}", file=sys.stderr)
        return 1

    print("# code = xx")
    print("# name = <endonym>              ; e.g. 日本語")
    print("# name_en = <name in English>   ; shown until the matching font loads")
    print("# font = ja                     ; script (ko|ja|sc|tc) for /sd/fonts/<script>-<family>-<px>.m5f")
    print("# ui_pad = 0                     ; optional 0/1 default for the 터치 여백 (list-row padding) toggle")
    print()
    for name, ko, en in rows:
        print(f"{name} = {ko if args.ko else en}")
    print(f"\n# {len(rows)} keys", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
