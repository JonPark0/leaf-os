# SD app kit (Lua)

Install an app from the home screen's **앱 / Apps** catalog, or drop it under
`/sd/apps/<id>/` by hand; either way it appears on the home screen below a
divider, no firmware rebuild. Apps are **Lua 5.4** scripts run by the built-in
`LuaApp` host; they reach the device only through the bindings below (no `io`,
`os`, `package`, `debug`, `require`, `dofile`, `load`).

## Layout

```
/sd/apps/<id>/
    app.txt      manifest (required)
    main.lua     entry script (or whatever `entry` names)
    icon.m5i     optional icon sheet — tile glyph + any g.icon() art
    .state.txt   written by the app via store.* (hidden; created on first set)
```

The folder name is the app id: `[a-z0-9-]`, 1 to 32 characters, not starting
with `-`. Folders with other names, and dot-folders, are ignored.

### app.txt  (`key = value`, same format as language packs)

```
name       = 날씨
name_en    = Weather
summary    = 현재 날씨와 오늘의 최고·최저 기온 (Open-Meteo)
summary_en = Current weather with today's high and low (Open-Meteo)
author     = JonPark0
version    = 1.0.0
api        = 1
net        = api.open-meteo.com, geocoding-api.open-meteo.com
icon       = cloud
entry      = main.lua
order      = 20
```

| key | default | meaning |
|---|---|---|
| `name` | the id | label on the tile (endonym) |
| `name_en` | | label when the UI language is English |
| `summary`, `summary_en` | | one line for the catalog |
| `author` | | shown in the catalog |
| `version` | | `MAJOR.MINOR.PATCH`; raise it whenever a file changes |
| `api` | 1 | lowest Lua API level the app needs (see below) |
| `net` | none | hosts `http.get` may reach, comma separated |
| `icon` | `note` | a name in `icon.m5i`, or a built-in UI icon name |
| `entry` | `main.lua` | the script to run |
| `order` | 100 | home-grid sort, ascending |

Lines starting with `#` are comments. A value runs to the end of the line.

**API level.** This firmware runs Lua API level **1**: the bindings below. The
level goes up when a binding is added, and existing bindings keep their
behaviour within a level. An app whose `api` is higher opens to a "needs a
firmware update" message instead of running. 설정 → 정보 shows the level.

**Network permission.** `http.get` only reaches hosts listed in `net` (exact
match, no wildcards); any other URL returns `nil, "permission"`. Without `net`
the app has no network access. The catalog shows these hosts before install,
and marks hosts an update adds.

## Lifecycle — optional globals the host calls

| global | when | note |
|---|---|---|
| `on_show()`  | app opened / resumed | |
| `on_hide()`  | app paused | |
| `on_draw()`  | repaint | the only place `g.*` is valid |
| `on_touch(x, y, phase)` | tap | `phase` = `"down"` / `"move"` / `"up"` |
| `on_tick()`  | once per kernel tick while foregrounded | keep it cheap |

Return a truthy value from any of them to ask the host to repaint. Coordinates
are portrait 480 × 800. A back button sits at the top-left (~`0..112, 42..92`) —
leave that corner clear.

## Bindings

### g — drawing (only inside `on_draw`)
`ink`: 0 = white, 3 = black (1-bit panel; only those two).

- `g.clear([ink])`  fill the screen
- `g.rect(x, y, w, h [, ink])`
- `g.text(x, y, s [, ink])`  top-left at (x, y), system UI font
- `g.textw(s) -> px`, `g.line_h() -> px`
- `g.icon(x, y, name [, ink])`  from this app's `icon.m5i`, else the UI sheet
- `g.w()`, `g.h()`

### sys
- `sys.now() -> unix`, `sys.clock() -> {y, mo, d, h, mi}` (local time)
- `sys.battery() -> 0..100`, `sys.wifi() -> bool`
- `sys.lang() -> "ko" | "en"`, `sys.log(...)`  (also `print`)

### store — per-app `.state.txt`
- `store.get(key [, default]) -> string`
- `store.set(key, value) -> bool`  (value coerced to a string)
- Backed by `/sd/apps/<id>/.state.txt` (`key = value` lines). Editable on a PC.

### http
- `http.get(url [, timeout_ms=8000]) -> body, status`  or  `nil, err`
- Blocks the UI while it runs (≤ 8 s). HTTPS uses the ESP-IDF cert bundle.
  Returns `nil, "offline"` with no Wi-Fi, and `nil, "permission"` when the
  URL's host is not in the manifest's `net`.
- The host shows a bottom-right "불러오는 중…" chip for the duration, so an app
  entry or a screen change that fetches doesn't look frozen — no work needed in
  the app.
- The URL is percent-encoded for you — pass a plain query value (`name=New York`,
  a UTF-8 city); an already-encoded URL is left as-is.

### json
- `json.decode(str) -> value`  (objects → tables, arrays → 1-based tables,
  `null` → absent). `nil, "parse error"` on bad input.

## Limits

- Call `http.get` and `store.set` from `on_show` / `on_touch` / `on_tick`, never
  from `on_draw` — a network round-trip or an SD write in the middle of a repaint
  stalls the frame and fights the display refresh.
- A callback that runs longer than ~9 s is aborted with `script timeout`.
- A Lua error shows on screen; the back button still works.
- One app runs at a time (the app stack). Removing the SD card pops it.

## Sample: weather

`apps/weather/` (copy to `/sd/apps/weather/`):
a dashboard — condition icon, big current temperature (drawn as 7-segment
digits), then the day's high / low. On Wi-Fi it pulls **Open-Meteo**
(`geocoding-api.open-meteo.com` + `api.open-meteo.com`, no key); the WMO
`weather_code` + `is_day` pick an icon from the app's own `icon.m5i`
(`sun`/`moon`/`cloud`/`cloud-sun`/`cloud-moon`/`cloud-rain`/`cloud-fog`/`snow`/
`bolt`, baked from `icons/Dazzle Line Icons/` by `tools/icongen`). Tap the bottom
row to cycle preset cities, "새로고침" to refetch; for any other city edit
`.state.txt`: `location = <city>`. Last reading is cached so it shows offline.

If flash ever gets tight, an app can skip HTTPS/JSON and hit
`http://wttr.in/<city>?format=j1` over plain HTTP instead.

## Publishing to the catalog

Apps under `apps/` in this repository are published to the catalog at
`https://jonpark0.github.io/leaf-os/catalog/v1/index.json` when they land on
`main`. `tools/apps/build_catalog.py` checks each app first: the id and keys
above, file paths (`[A-Za-z0-9._-]`, no dot-files, at most two folder levels),
at most 32 files, 256 KiB per file and 1 MiB per app. On a pull request it also
fails when an app's files changed but its `version` did not go up, since the
device would not offer the update otherwise.

On the device, 앱 / Apps lists the catalog. Install and update download every
file into `/sd/apps/.inst-<id>/`, check sizes and SHA-256, then swap the folder
in, keeping `.state.txt`. If anything fails, the installed version stays.
Delete removes the folder, saved state included.

To try a catalog before publishing it, build it and serve it on the local
network, then point the device at it with `/sd/apps/catalog.txt`:

```
python tools/apps/build_catalog.py --out site
python -m http.server -d site/catalog/v1 8000
```
```
url = http://<PC address>:8000/
```

## Not yet

A permission settings page; an on-screen keyboard binding for text entry;
`http.get` on a worker task instead of blocking; clock / draw / memo
reimplemented as bundled Lua apps; signed catalogs for apps from other
authors.
