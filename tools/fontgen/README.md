# fontgen — TTF/OTF → `.m5f`

Bakes a single typeface at a single pixel size into the device font format
(`docs/FONT_FORMAT.md`). One file per size.

```bash
# The `python` on PATH is the Windows Store stub; use the project venv.
py -m venv .venv && ./.venv/Scripts/python -m pip install -r requirements.txt
PY=./.venv/Scripts/python                # freetype-py (+ pillow for --preview)

# UI font: Latin + punctuation + all Hangul syllables, 16 px, 1-bit
python fontgen.py --font ../../fonts/Nanum_Gothic/NanumGothic-Regular.ttf \
    --size 16 --ranges korean --bpp 1 --out ui16.m5f

# Reader font: anti-aliased, 24 px, 2-bit coverage
python fontgen.py --font ../../fonts/Nanum_Myeongjo/NanumMyeongjo-Regular.ttf \
    --size 24 --ranges korean --bpp 2 --out read24.m5f

# Only the glyphs a given book actually uses (tiny CJK files):
python fontgen.py --font Noto.ttf --size 22 --from-text book.txt --out book22.m5f

$PY fontgen.py --verify ui16.m5f --preview ui16.png   # re-parse + proof sheet
$PY fontgen.py --list-ranges                          # named presets
```

## Reader body fonts (`/sd/fonts/`)

The reader loads a per-size `.m5f` from the SD card's `/fonts/` folder and keeps
one resident in PSRAM. Regenerate the shipped set into `out/` and copy to SD:

```bash
for sz in 16 20 24 28 32; do
  $PY fontgen.py --font ../../fonts/Nanum_Gothic/NanumGothic-Regular.ttf \
      --size $sz --ranges ui,korean --bpp 1 --out out/reader-$sz.m5f
done
```

The device enumerates every `*.m5f` in `/sd/fonts/` and reads its header size, so
the filename is only a convention — add a `reader-22.m5f` and the size stepper
picks it up. Keep `--bpp 1`: 2-bpp body text forces the slow 4-gray refresh on
every page turn.

## `--ranges`

Comma-separated preset names and/or explicit ranges, freely mixed:

```
latin,punctuation,hangul-syllables
U+2600-U+26FF
0x20-0x7E
```

Presets include `latin`, `latin-1`, `punctuation`, `currency`, `arrows`,
`box-drawing`, `hangul-syllables`, `hangul-compat-jamo`, `hiragana`, `katakana`,
`cjk-common`, `cjk-ext-a`, `fullwidth`, plus the bundles `ui`, `korean`,
`japanese`. `--from-text FILE` overrides `--ranges` and subsets to exactly the
characters present in FILE (plus space).

## Sizes / bpp

- `--bpp 1` — crisp 1-bit, smallest. Good for UI at <= 20 px.
- `--bpp 2` — 4-level coverage, anti-aliased. Better for the reader body text;
  ~3x the bitmap size. The device maps coverage 0..3 straight onto its gray
  levels, so 2-bpp text needs a full 4-gray refresh, not a partial.

Rough sizes (Nanum Gothic, `korean` preset ≈ 11.4k glyphs): 16 px bpp1 ≈ 440 KB,
20 px bpp2 ≈ 1.1 MB, 24 px bpp2 ≈ 1.6 MB. UI-only (`latin,latin-1,punctuation`)
16 px bpp1 ≈ 4 KB.
