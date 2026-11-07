"""Named Unicode range presets for fontgen.

A range spec is a comma-separated list of preset names and/or explicit ranges:
    latin,punctuation,hangul-syllables
    U+2600-U+26FF
    0x20-0x7E
Presets and explicit ranges may be freely mixed.
"""

# name -> list of (first_cp, last_cp) inclusive
PRESETS: dict[str, list[tuple[int, int]]] = {
    # --- Latin / European ---
    "ascii":            [(0x0020, 0x007E)],
    "latin":            [(0x0020, 0x007E)],
    "latin-1":          [(0x00A0, 0x00FF)],
    "latin-ext-a":      [(0x0100, 0x017F)],
    "latin-ext-b":      [(0x0180, 0x024F)],
    "cyrillic":         [(0x0400, 0x04FF)],
    "greek":            [(0x0370, 0x03FF)],
    # --- Punctuation / symbols ---
    "punctuation":      [(0x2000, 0x206F)],
    "currency":         [(0x20A0, 0x20BF)],
    "arrows":           [(0x2190, 0x21FF)],
    "math":             [(0x2200, 0x22FF)],
    "box-drawing":      [(0x2500, 0x257F)],
    "block-elements":   [(0x2580, 0x259F)],
    "geometric":        [(0x25A0, 0x25FF)],
    "misc-symbols":     [(0x2600, 0x26FF)],
    "dingbats":         [(0x2700, 0x27BF)],
    # --- CJK / Korean / Japanese ---
    "cjk-symbols":      [(0x3000, 0x303F)],
    "hiragana":         [(0x3040, 0x309F)],
    "katakana":         [(0x30A0, 0x30FF)],
    "kana":             [(0x3040, 0x30FF)],
    "hangul-jamo":      [(0x1100, 0x11FF)],
    "hangul-compat-jamo": [(0x3130, 0x318F)],
    "hangul-syllables": [(0xAC00, 0xD7A3)],
    "cjk-common":       [(0x4E00, 0x9FFF)],
    "cjk-ext-a":        [(0x3400, 0x4DBF)],
    "fullwidth":        [(0xFF00, 0xFFEF)],
    # --- convenience bundles ---
    "ui":               [(0x0020, 0x007E), (0x00A0, 0x00FF), (0x2000, 0x206F),
                         (0x20A0, 0x20BF), (0x2190, 0x21FF), (0x2500, 0x25FF)],
    "korean":           [(0x0020, 0x007E), (0x2000, 0x206F),
                         (0x3130, 0x318F), (0xAC00, 0xD7A3)],
    "japanese":         [(0x0020, 0x007E), (0x2000, 0x206F), (0x3000, 0x30FF),
                         (0x4E00, 0x9FFF), (0xFF00, 0xFFEF)],
}


def _parse_cp(token: str) -> int:
    token = token.strip().lower()
    if token.startswith("u+"):
        return int(token[2:], 16)
    if token.startswith("0x"):
        return int(token, 16)
    return int(token)


def resolve(spec: str) -> list[tuple[int, int]]:
    """Expand a range spec string to a merged, sorted list of (first, last)."""
    out: list[tuple[int, int]] = []
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if part.lower() in PRESETS:
            out.extend(PRESETS[part.lower()])
        elif "-" in part.replace("u+", "@").replace("U+", "@"):
            # explicit "A-B" where A,B may be U+.. / 0x.. / decimal
            lo_s, hi_s = part.replace("U+", "u+").split("-", 1) if part.count("-") == 1 \
                else _split_uplus(part)
            out.append((_parse_cp(lo_s), _parse_cp(hi_s)))
        else:
            cp = _parse_cp(part)
            out.append((cp, cp))
    return merge(out)


def _split_uplus(part: str):
    # handle "U+2600-U+26FF" where '-' also appears after the first U+ token
    a, _, b = part.lower().partition("-u+")
    return a, "u+" + b


def merge(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    if not ranges:
        return []
    ranges = sorted((min(a, b), max(a, b)) for a, b in ranges)
    merged = [ranges[0]]
    for lo, hi in ranges[1:]:
        plo, phi = merged[-1]
        if lo <= phi + 1:
            merged[-1] = (plo, max(phi, hi))
        else:
            merged.append((lo, hi))
    return merged


def list_presets() -> str:
    return "\n".join(
        f"  {name:20s} {' '.join(f'U+{a:04X}-U+{b:04X}' for a, b in rs)}"
        for name, rs in sorted(PRESETS.items())
    )
