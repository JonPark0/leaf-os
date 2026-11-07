#!/usr/bin/env bash
# Assemble release assets from a CI firmware build.
#
#   tools/release/package.sh <version> <firmware-dir> <out-dir>
#
# <firmware-dir> holds the "leaf-os-firmware" CI artifact (the build/ layout).
set -euo pipefail

version="$1"
fw="$2"
out="$3"
root="$(cd "$(dirname "$0")/../.." && pwd)"
name="leaf-os-${version}"

mkdir -p "$out"
out="$(cd "$out" && pwd)"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

# Fresh install: one image written at 0x0. Erases settings (NVS).
cp "$fw/leaf_os-merged.bin" "$out/${name}-full.bin"

# Update: the separate images at their offsets. Keeps settings.
mkdir -p "$stage/parts/${name}"
cp "$fw/flash_args" "$fw/leaf_os.bin" "$fw/ota_data_initial.bin" "$stage/parts/${name}/"
mkdir -p "$stage/parts/${name}/bootloader" "$stage/parts/${name}/partition_table"
cp "$fw/bootloader/bootloader.bin" "$stage/parts/${name}/bootloader/"
cp "$fw/partition_table/partition-table.bin" "$stage/parts/${name}/partition_table/"
(cd "$stage/parts" && zip -qr "$out/${name}-update.zip" "${name}")

# Debug symbols for crash decoding.
(cd "$fw" && zip -q "$out/${name}-elf.zip" leaf_os.elf leaf_os.map)

# SD card contents, laid out as the card root.
mkdir -p "$stage/sd/apps" "$stage/sd/lang"
cp -r "$root/apps/." "$stage/sd/apps/"
cp "$root"/tools/langgen/samples/*.lang "$stage/sd/lang/"
(cd "$stage/sd" && zip -qr "$out/${name}-sd.zip" apps lang)

(cd "$out" && sha256sum -- * > SHA256SUMS)
ls -l "$out"
