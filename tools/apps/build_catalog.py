#!/usr/bin/env python3
"""Validate apps/*/ and build the app catalog site.

    python tools/apps/build_catalog.py [--out site] [--base origin/main]

Writes <out>/catalog/v1/index.json and <out>/catalog/v1/apps/<id>/<version>/<file>.
The rules match main/appstore/ (manifest.cpp, catalog.cpp); see
docs/design/app-catalog.md. With --base, an app whose files differ from <base>
must also have a higher version than it had there.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = 1
MAX_FILES = 32
MAX_FILE_BYTES = 256 * 1024
MAX_APP_BYTES = 1024 * 1024
MAX_INDEX_BYTES = 64 * 1024

ID_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,31}$")
VERSION_RE = re.compile(r"^(\d{1,5})\.(\d{1,5})\.(\d{1,5})$")
LABEL_RE = re.compile(r"^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$")
PATH_SEG_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def parse_manifest(text):
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def parse_version(s):
    m = VERSION_RE.match(s or "")
    return tuple(int(x) for x in m.groups()) if m else None


def valid_host(h):
    return 0 < len(h) <= 253 and all(LABEL_RE.match(p) for p in h.split("."))


def valid_rel_path(p):
    if not p or len(p) > 96:
        return False
    segs = p.split("/")
    return len(segs) <= 3 and all(PATH_SEG_RE.match(s) and not s.startswith(".") for s in segs)


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True,
                          encoding="utf-8")


def check_version_bump(app_id, version, base, errors):
    if git("diff", "--quiet", base, "--", f"apps/{app_id}").returncode == 0:
        return  # unchanged since base
    old = git("show", f"{base}:apps/{app_id}/app.txt")
    if old.returncode != 0:
        return  # new app
    old_version = parse_version(parse_manifest(old.stdout).get("version"))
    if old_version and version <= old_version:
        errors.append(f"{app_id}: files changed since {base} but version "
                      f"{'.'.join(map(str, version))} is not above "
                      f"{'.'.join(map(str, old_version))}")


def build_entry(app_dir, errors):
    app_id = app_dir.name
    err = lambda msg: errors.append(f"{app_id}: {msg}")
    if not ID_RE.match(app_id):
        err("folder name must match [a-z0-9-], 1-32 chars, not starting with '-'")
        return None
    manifest_path = app_dir / "app.txt"
    if not manifest_path.is_file():
        err("no app.txt")
        return None
    m = parse_manifest(manifest_path.read_text(encoding="utf-8"))

    version = parse_version(m.get("version"))
    if not version:
        err("version must be MAJOR.MINOR.PATCH")
    api = m.get("api", "")
    if not api.isdigit() or int(api) < 1:
        err("api must be a positive integer")
    for key in ("name", "summary"):
        if not m.get(key):
            err(f"missing {key}")
    net = [h.strip().lower() for h in m.get("net", "").split(",") if h.strip()]
    for h in net:
        if not valid_host(h):
            err(f"bad host in net: {h}")
    entry = m.get("entry") or "main.lua"

    files, total = [], 0
    for p in sorted(app_dir.rglob("*")):
        if p.is_dir():
            continue
        rel = p.relative_to(app_dir).as_posix()
        if not valid_rel_path(rel):
            err(f"bad file path: {rel}")
            continue
        data = p.read_bytes()
        if len(data) > MAX_FILE_BYTES:
            err(f"{rel} is over {MAX_FILE_BYTES} bytes")
        total += len(data)
        files.append({"path": rel, "size": len(data),
                      "sha256": hashlib.sha256(data).hexdigest(), "_src": p})
    if len(files) > MAX_FILES:
        err(f"more than {MAX_FILES} files")
    if total > MAX_APP_BYTES:
        err(f"over {MAX_APP_BYTES} bytes in total")
    if entry not in {f["path"] for f in files}:
        err(f"entry script {entry} not found")
    if not version:
        return None
    files.sort(key=lambda f: (f["path"] != "app.txt", f["path"]))

    return {
        "id": app_id,
        "version": ".".join(map(str, version)),
        "api": int(api) if api.isdigit() else 1,
        "name": m.get("name", ""),
        "name_en": m.get("name_en", ""),
        "summary": m.get("summary", ""),
        "summary_en": m.get("summary_en", ""),
        "author": m.get("author", ""),
        "icon": m.get("icon", ""),
        "entry": entry,
        "net": net,
        "size": total,
        "files": files,
        "_version": version,
        "_order": int(m["order"]) if m.get("order", "").isdigit() else 100,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apps", type=Path, default=ROOT / "apps")
    ap.add_argument("--out", type=Path, default=ROOT / "site")
    ap.add_argument("--base", help="git ref to check version bumps against")
    args = ap.parse_args()

    errors, entries = [], []
    for app_dir in sorted(p for p in args.apps.iterdir() if p.is_dir()):
        e = build_entry(app_dir, errors)
        if e and args.base:
            check_version_bump(e["id"], e["_version"], args.base, errors)
        if e:
            entries.append(e)

    if errors:
        for msg in errors:
            print(f"error: {msg}", file=sys.stderr)
        return 1

    entries.sort(key=lambda e: (e["_order"], e["id"]))
    catalog = args.out / "catalog" / "v1"
    if catalog.exists():
        shutil.rmtree(catalog)
    for e in entries:
        for f in e["files"]:
            dst = catalog / "apps" / e["id"] / e["version"] / f["path"]
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(f.pop("_src"), dst)
        del e["_version"], e["_order"]

    index = json.dumps({"schema": SCHEMA, "apps": entries}, ensure_ascii=False, indent=1)
    if len(index.encode("utf-8")) > MAX_INDEX_BYTES:
        print(f"error: index.json is over {MAX_INDEX_BYTES} bytes", file=sys.stderr)
        return 1
    (catalog / "index.json").write_text(index + "\n", encoding="utf-8")
    for e in entries:
        print(f"{e['id']} {e['version']}: {len(e['files'])} files, {e['size']} bytes")
    print(f"wrote {catalog / 'index.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
