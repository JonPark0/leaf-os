# Firmware update over Wi-Fi (OTA)

Status: design, 2026-11-08.

## Goal

Update the firmware from the device over Wi-Fi: 설정 → 펌웨어 업데이트 shows the
running and the latest version, downloads the new image into the other app
slot, checks it, and restarts into it. A new image that fails to start goes
back to the previous one by itself. Settings, Wi-Fi and the SD card are not
touched.

Out of scope: background or automatic update checks, beta/prerelease channels,
delta updates, signed images (see §6).

## 1. Flash layout

Unchanged. `partitions.csv` already has `otadata`, `app0` and `app1`
(0x280000 each). The image is 2.35 MB, so each slot has about 10% headroom;
the build already reports this.

## 2. Feed (`firmware/v1/latest.json` on GitHub Pages)

`pages.yml` also publishes the latest firmware next to the app catalog, so the
device talks to one host (`jonpark0.github.io`) with no redirects:

```
https://jonpark0.github.io/leaf-os/firmware/v1/latest.json
https://jonpark0.github.io/leaf-os/firmware/v1/leaf-os-<tag>.bin
```

```json
{
  "schema": 1,
  "version": "v0.3.0",
  "size": 2346560,
  "sha256": "…64 hex…",
  "file": "leaf-os-v0.3.0.bin",
  "notes": "https://github.com/JonPark0/leaf-os/releases/tag/v0.3.0"
}
```

- `tools/release/build_feed.py` takes the latest non-prerelease GitHub release,
  pulls `leaf_os.bin` out of its `-update.zip`, and writes the two files. No
  new release asset is needed, and v0.2.0 already works as input.
- `pages.yml` gains a `release: [published]` trigger, and every run builds both
  the catalog and the feed, since a Pages deploy replaces the whole site.
- `/sd/firmware.txt` with `url = …` overrides the feed URL (for testing from a
  PC on the local network), like `/sd/apps/catalog.txt`.

## 3. Versions

The running version is `esp_app_get_description()->version`, which is
`git describe` at build time: `v0.2.0` on a tag, `v0.2.0-3-gabc1234[-dirty]`
on a dev build.

`parseFirmwareVersion` takes `vMAJOR.MINOR.PATCH` and an optional `-…`
suffix; a suffix marks a dev build. An update is offered only when the feed's
numbers are higher than the running ones. A dev build `v0.2.0-4-g…` is four
commits past v0.2.0, so v0.2.0 itself is not offered to it. An unreadable
running version always gets the offer.

## 4. Device side

**Screen.** 설정 gets a row 펌웨어 업데이트 → `app_update`:

- 현재 버전 / 최신 버전 lines, the release-notes URL as text.
- One button: 확인 (check) → 업데이트 (when newer) → disabled while running.
- Status line: checking, up to date, downloading NN%, verifying, restarting,
  and errors (same "can't reach the internet" wording as the app catalog).

**Preconditions** before the download starts: Wi-Fi connected, battery at
least 30% (the PM1 gives no charger state, so this also applies on USB), feed
`size` fits the target slot.

**Download** runs on a worker task (8 KiB stack, priority below the UI task),
because writing 2.3 MB takes about a minute and the UI must keep painting:

1. `esp_ota_get_next_update_partition`, `esp_ota_begin(OTA_WITH_SEQUENTIAL_WRITES)`.
2. Stream the GET in 4 KiB chunks into `esp_ota_write`, hashing with
   `mbedtls_sha256` (the same loop as `hal::http::download`, moved behind a
   sink callback so both share it).
3. Fail on non-200, short read, more bytes than `size`, size or SHA-256
   mismatch. `esp_ota_end` then validates the image header and chip.
4. `esp_ota_set_boot_partition`, show "restarting", `esp_restart()` after the
   screen has painted.

Progress is an atomic byte count the screen reads each tick; it repaints only
when the whole percent changes by 5 or more to limit e-paper refreshes. Back
cancels (`esp_ota_abort`), and the old slot stays the boot slot.

**Rollback.** Enable `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. A new image
boots in `ESP_OTA_IMG_PENDING_VERIFY`; the kernel calls
`esp_ota_mark_app_valid_cancel_rollback()` once the home screen has painted and
the loop has run 10 s. A reset or crash before that makes the bootloader go
back to the previous slot. On boot, if the last update was rolled back, the
update screen says so.

This changes the bootloader, which OTA cannot replace. Devices on v0.2.0 or
older install v0.3.0 once with `-update.zip` (keeps settings); OTA works from
then on.

## 5. Development flashing

`write_flash 0x10000 leaf_os.bin` only writes `app0`. After an OTA update the
device boots `app1`, so that command would no longer change what runs. The
documented dev command becomes `write_flash @flash_args`-style: app at
0x10000 plus `ota_data_initial.bin` at 0xe000, which resets the boot slot to
`app0`. NVS is still kept. `-update.zip` already does this.

## 6. Security

The feed and the image come over HTTPS from the same host, checked against
the certificate bundle, and the image is checked against the feed's SHA-256.
That protects against corruption and network tampering, not against someone
who controls the repository or the Pages site. Signed images need Secure Boot
v2, which burns eFuses and cannot be undone; it is left for later.

## 7. Tests

- Host tests: `parseFirmwareVersion` / update decision (tag, dev, dirty,
  garbage, equal), feed JSON validation (schema, size bounds, hex length,
  file name characters).
- CI: `build_feed.py` against the v0.2.0 release in a dry run.
- Hardware: update from a dev build to the feed version; pull power during
  download (old version boots); flash a build that crashes at start through a
  local feed (rolls back); battery-below-30% refusal; dev flash after OTA.
