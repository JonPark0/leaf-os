# Language packs & CJK fonts on the SD card

The firmware ships **한국어** and **English** built in. Any other language is an
SD-card *language pack* — a text file of translated strings — plus, for
non-Latin scripts, a matching font.

## 1. The pack file — `/sd/lang/<code>.lang`

UTF-8, `KEY = value` per line. `#`-prefixed lines are metadata. Blank lines and
unknown keys are ignored; any key you omit falls back to the built-in **English**
string.

```
# code = ja            ; must match the file name (ja.lang)
# name = 日本語          ; endonym, shown in 설정 → 언어
# name_en = Japanese    ; last-resort label if no /sd/fonts face can draw `name`
# font = ja             ; script for /sd/fonts/<script>-<family>-<px>.m5f (§2)
# ui_pad = 0            ; optional: 0/1 default for the 터치 여백 (list-row
                        ;   padding) toggle. Omit → on for ko/en, and 0 here for
                        ;   tall CJK fonts. A user choice in 설정 overrides it.

BACK = ← 戻る
SAVE = 保存
APP_SETTINGS = 設定
...
```

The 언어 list draws each pack's endonym in its own script: if the running UI
font can't draw it, `app_language` loads the pack's `# font` face from
`/sd/fonts` just for that row. `name_en` (or the file code) shows only when no
matching font is on the card.

Generate a fresh template (English values, all keys) from the current firmware:

```
python tools/langgen/langgen.py > ja.lang      # then translate the values
```

`tools/langgen/samples/ja.lang` is a complete Japanese example.

The key list is the firmware↔pack contract (the X-macro in `main/core/i18n.h`).
A pack made for an older firmware still loads — new keys just show in English.

## 2. The font — `/sd/fonts/<script>-<family>-<px>.m5f`

- `<script>` : `ko` · `ja` · `sc` (Simplified) · `tc` (Traditional) — matches a
  pack's `# font` tag.
- `<family>` : `ko` → `gothic` (default) · `myeongjo` · `pen`;
  `ja`/`sc`/`tc` → `sans` (default) · `serif`.
- `<px>` : confirmed from the file header; the name is just a hint.

Back-compat: `<script>-<px>.m5f` is the script's **default** family, and the
legacy `reader-<px>.m5f` is read as `ko-gothic-<px>`.

- The **system (UI) font** resolves at boot from the active language's `# font`
  tag plus 설정 → 화면/글꼴 → 시스템 글꼴/크기. With no SD match it falls back
  to the embedded 22 px NanumGothic (Korean + Latin only).
- The **reader body font** resolves independently from 읽기 글꼴/크기, filtered
  to the UI script. Changing 읽기 글꼴 applies on the next book open; changing
  the system font (or the language) needs a restart — a **재시작하여 적용** row
  then appears at the top of 설정.

Shipped in `tools/fontgen/out/` (also staged under `out/sd-bundle/`, ready to
copy onto the card): `ko-{gothic,myeongjo,pen}` at 20/22/24/28/32,
`ja/sc/tc-sans` at 22/24/28, `ja/sc/tc-serif` at 24 — all 1-bit. CJK tops out at
28 px on purpose (larger crowds the UI). Build more:

```
PY=tools/fontgen/.venv/Scripts/python
$PY tools/fontgen/fontgen.py --font fonts/Noto_Serif_JP/NotoSerifJP-VariableFont_wght.ttf \
    --size 28 --ranges "ui,cjk-symbols,kana,cjk-common,fullwidth" --bpp 1 \
    --out tools/fontgen/out/ja-serif-28.m5f
# Korean faces: --ranges "ui,korean"  (Simplified/Traditional drop "kana")
```

Keep `--bpp 1`: anti-aliased body text forces the slow 4-gray refresh on every
page turn.

## 3. Enable it

1. Copy `<code>.lang` to `/sd/lang/` and the font(s) to `/sd/fonts/`.
2. 설정 → 언어 → pick the pack.
3. Restart (language and system font apply at boot, like the system font size).

## Not supported

Right-to-left and complex scripts (Arabic, Hebrew, Hindi, Thai …). The text
layout is left-to-right with no glyph shaping or bidi.
