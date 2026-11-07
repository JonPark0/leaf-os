# App catalog: packaging, permissions, on-device install

Status: design, 2026-11-07.

## Goal

Install, update and remove Lua apps on the device over Wi-Fi, without taking
the SD card out. Apps live in this repository under `apps/`; CI publishes a
catalog to GitHub Pages; a built-in **Apps** screen lists the catalog and
installs into `/sd/apps/<id>/`.

The format is kept open for apps written by other people later, but this
design does not add a submission process, signing or per-permission toggles.
Copying a folder to the SD card by hand keeps working.

## 1. Manifest (`app.txt`)

Existing keys stay: `name`, `name_en`, `icon`, `entry`, `order`. New keys:

| key | required in catalog | meaning |
|---|---|---|
| `version` | yes | `MAJOR.MINOR.PATCH`, integers. Compared numerically to detect updates. |
| `api` | yes | Minimum Lua API level the app needs. Missing → 1. |
| `net` | no | Comma-separated host names the app may reach with `http.get`. Missing → no network. |
| `summary`, `summary_en` | yes (`summary`) | One-line description for the catalog. |
| `author` | no | Shown on the detail screen. |

The folder name is the app id: `[a-z0-9-]`, 1 to 32 characters, not starting
with `-`.

**API level.** The firmware defines `kLuaApiLevel = 1` (the bindings in
`docs/APP_KIT.md` today). The level goes up when a binding is added; existing
bindings do not change behaviour within a level. An app whose `api` is higher
than the firmware's shows on the home grid but opens to a "needs a firmware
update" message instead of running. About shows the level.

**Network permission.** `LuaApp` keeps the app's `net` list. `http.get`
extracts the URL's host and refuses with `nil, "permission"` unless it equals
(case-insensitive) one of the listed hosts. No wildcards. `g`, `sys`, `store`
and `json` touch only the app's own screen and state, so they need no
permission. New capabilities that reach outside the app add their own key in
the same way.

**Consent.** The catalog detail screen lists the hosts before install. On an
update that adds hosts, the new ones are marked. Pressing Install / Update is
the consent. Sideloaded apps are trusted as placed.

`apps/weather/app.txt` gains `version = 1.0.0`, `api = 1`, its two Open-Meteo
hosts and a summary.

## 2. Catalog and CI

**Location.** `https://jonpark0.github.io/leaf-os/catalog/v1/`

```
catalog/v1/index.json
catalog/v1/apps/<id>/<version>/<file>      e.g. apps/weather/1.0.0/main.lua
```

File URLs include the version so a CDN-cached old file can never mix with a new
one. Only the current version of each app is published.

**index.json**

```json
{
  "schema": 1,
  "apps": [
    {
      "id": "weather", "version": "1.0.0", "api": 1,
      "name": "날씨", "name_en": "Weather",
      "summary": "...", "summary_en": "...", "author": "JonPark0",
      "icon": "cloud", "net": ["api.open-meteo.com", "geocoding-api.open-meteo.com"],
      "size": 13221,
      "files": [ { "path": "main.lua", "size": 6012, "sha256": "..." } ]
    }
  ]
}
```

**`tools/apps/build_catalog.py`** reads `apps/*/` and writes `site/`. It fails
on:

- a bad id, a missing required key, a malformed `version`, `api` or host;
- a file path that is absolute, contains `..`, starts with `.`, has characters
  outside `[A-Za-z0-9._/-]`, or is nested more than two levels;
- more than 32 files, a file over 256 KiB or an app over 1 MiB;
- with `--base <ref>`: an app whose files changed since `<ref>` while its
  `version` did not go up. Without this, devices would never see the change.

**Workflows.**

- `ci.yml` gains a *Catalog* job: `build_catalog.py --base origin/main` on pull
  requests, plain build on pushes. It runs next to the existing `luac -p` job.
- New `pages.yml`: on pushes to `main` that touch `apps/**` or `tools/apps/**`
  (and by manual dispatch), build and deploy `site/` with
  `actions/deploy-pages`. Pages is enabled once with source "GitHub Actions".

The release `-sd.zip` keeps bundling `apps/` as today.

## 3. Device

### Units

| unit | role | platform-free |
|---|---|---|
| `main/appstore/manifest.{h,cpp}` | parse `app.txt` text into `AppManifest`; id, version, host, path validation; version compare | yes |
| `main/appstore/catalog.{h,cpp}` | `CatalogEntry`; validate an entry; `status(entry, installed, apiLevel)` → NotInstalled / Installed / UpdateAvailable / NeedsFirmware; new hosts on update | yes |
| `main/appstore/url.{h,cpp}` | host of a URL; host allow check | yes |
| `main/appstore/catalog_client.{h,cpp}` | fetch and parse `index.json` (cJSON) into `CatalogEntry` | no |
| `main/appstore/installer.{h,cpp}` | stage, verify, swap, delete, startup recovery | no |
| `hal::http::download(url, path, max, sha256_out)` | stream a GET to a file in 4 KiB chunks, hashing on the way | no |
| `main/apps/app_store/` | the Apps screen | no |

`lua_registry.cpp` uses `appstore::parseManifest` instead of its own parser, so
the home grid and the catalog read `app.txt` the same way. `SdAppInfo` gains
`version`, `api` and `net`; `LuaApp` takes them.

The catalog URL defaults to the Pages URL above. `/sd/apps/catalog.txt` with
`url = http://...` overrides it, for testing against a local server.

### Install / update

1. Check Wi-Fi and the SD card; otherwise show why and stop.
2. Remove any `/sd/apps/.inst-<id>/` and create it fresh.
3. Download each file with `hal::http::download` into the staging folder.
   Stop on a size or SHA-256 mismatch, an HTTP error or a write error, delete
   the staging folder and show the error. The installed app is untouched.
4. Copy `.state.txt` from the installed app, if any, into staging.
5. Rename `<id>` → `.old-<id>`, `.inst-<id>` → `<id>`, then delete `.old-<id>`.

One file per kernel tick, driven from `onTick`, so the screen can show
"받는 중 2/4" between files.

**Recovery** (at boot, after the card mounts): if `<id>` is missing and
`.old-<id>` exists, rename it back. Delete leftover `.inst-*` and `.old-*`
folders whose `<id>` exists. Dot-folders are already hidden from the home grid.

**Delete.** Two taps ("삭제" then "한 번 더 눌러 삭제"), as on the power page.
Removes the whole folder, including saved state.

### Screens

- Home grid: a built-in **앱 / Apps** tile.
- List: one row per catalog app plus installed apps not in the catalog. The
  right column shows `설치됨`, `업데이트`, `펌웨어 필요` or nothing. The list
  loads when the screen opens; a 새로고침 row reloads it. Offline shows
  installed apps only, which can still be deleted.
- Detail: name, version (`1.0.0 → 1.1.0` on update), author, size, summary,
  network hosts, then Install / Update / Delete. Leaving the screen after a
  change rebuilds the home grid.

## 4. Errors

| case | shown |
|---|---|
| No Wi-Fi | "Wi-Fi에 연결해 주세요" with installed apps listed |
| No SD card | "SD 카드를 넣어 주세요" |
| `index.json` unreachable or invalid | "앱 목록을 불러오지 못했습니다" + reason |
| Entry fails validation on device | skipped and logged |
| Download / hash / write failure | "설치하지 못했습니다: <reason>"; installed version kept |
| `api` too high | row shows 펌웨어 필요; Install disabled |

Device-side limits match CI: `index.json` ≤ 64 KiB, 32 files, 256 KiB per
file, 1 MiB per app.

## 5. Testing

- Host tests (`tests/`, CI): manifest parsing and validation, version compare,
  path rules, URL host extraction and allow check, catalog status and new-host
  diff.
- CI: `build_catalog.py` over `apps/`, and `--base` on pull requests.
- Hardware, against a local server through `catalog.txt`: install weather from
  scratch; update after a version bump and confirm `.state.txt` survives; break a
  hash and confirm the old version stays; pull the card mid-install and confirm
  recovery on next boot; delete; confirm weather's network calls still work and
  a host not in `net` is refused.

## Not in this design

Signing or a second trust root; outside submissions; a permission settings
page; automatic update checks; icons in the catalog list; Lua `require`
across files.
