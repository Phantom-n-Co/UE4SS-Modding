#!/usr/bin/env python3
"""Build, package and publish one mod of this monorepo to Nexus Mods (API v3).

  tools/nexus_release.py lookup [--write]      list your Abiotic Factor mods/files; --write fills tools/nexus.json
  tools/nexus_release.py pull-changelogs       write <Mod>/CHANGELOG.md from the Nexus changelog (refuses to overwrite)
  tools/nexus_release.py publish <Mod> [opts]  build + test + zip + upload a new version of <Mod>'s file

The changelog for a release is the `## <version>` section of AbioticFactor/<Mod>/CHANGELOG.md.

Per-mod release tags: <Mod>-v<version>. The version comes from ModVersion in <Mod>/dllmain.cpp.
API key: $NEXUSMODS_API_KEY, else libsecret (`secret-tool store --label='Nexus Mods API key' service nexusmods account api`).
Stdlib only.
"""
import argparse
import base64
import html
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path
from typing import Any, NoReturn

ROOT = Path(__file__).resolve().parent.parent
MODS_DIR = ROOT / "AbioticFactor"
CONFIG = ROOT / "tools" / "nexus.json"
DIST = ROOT / "dist"
PRESET = "game-shipping-xwin"
BUILD_DIR = ROOT / "cmake-build-game__shipping__win64-xwin"
API = os.environ.get("NEXUSMODS_API_BASE", "https://api.nexusmods.com/v3")
API_V1 = "https://api.nexusmods.com/v1"
GAME_DOMAIN = "abioticfactor"
VERSION_RE = re.compile(r'ModVersion\s*=\s*STR\("([^"]+)"\)')


def die(msg) -> NoReturn:
    sys.exit(f"error: {msg}")


def run(cmd, **kw):
    print("+", " ".join(map(str, cmd)))
    return subprocess.run(cmd, cwd=ROOT, check=True, **kw)


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()


# ---------------------------------------------------------------- API

def api_key():
    key = os.environ.get("NEXUSMODS_API_KEY", "").strip()
    if not key:
        try:
            key = subprocess.run(["secret-tool", "lookup", "service", "nexusmods", "account", "api"],
                                 capture_output=True, text=True).stdout.strip()
        except FileNotFoundError:
            pass
    if not key:
        die("no API key: set NEXUSMODS_API_KEY or store it with "
            "`secret-tool store --label='Nexus Mods API key' service nexusmods account api`")
    return key


def request(method, url, *, body=None, headers=None, key=None) -> Any:
    hdrs = {"User-Agent": "UE4SS-Modding/nexus_release.py", **(headers or {})}
    data = body
    if key is not None:
        hdrs["apikey"] = key
        url = API + url
        if body is not None and not isinstance(body, (bytes, bytearray)):
            data = json.dumps(body).encode()
            hdrs["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, method=method, headers=hdrs)
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            raw = resp.read()
    except urllib.error.HTTPError as e:
        die(f"{method} {url.split('?')[0]} -> {e.code}: {e.read().decode(errors='replace')}")
    if not raw:
        return None
    try:
        return json.loads(raw)
    except ValueError:
        return raw


def api(method, path, key, body=None) -> Any:
    return request(method, path, body=body, key=key)


# ---------------------------------------------------------------- lookup

def cmd_lookup(args):
    key = api_key()
    found, page = [], 1
    while True:
        res = api("GET", f"/team-mods?affiliation=owner&page={page}&page_size=100", key)
        mods = res["data"]["mods"]
        found += [m["mod"] for m in mods if m["mod"]["game"]["domain_name"] == GAME_DOMAIN]
        meta = res["meta"]
        if not mods or page * meta["page_size"] >= meta["total_count"]:
            break
        page += 1

    cfg = load_config(required=False)
    local_mods = sorted(p.parent.name for p in MODS_DIR.glob("*/dllmain.cpp"))
    for mod in found:
        files = api("GET", f"/mods/{mod['id']}/files", key)["data"]["mod_files"]
        print(f"mod {mod['name']!r}: id={mod['id']} game_scoped_id={mod['game_scoped_id']}")
        active = [f for f in files if f.get("is_active", True)]
        for f in files:
            print(f"    file {f['name']!r}: id={f['id']} active={f.get('is_active')} versions={f.get('versions_count')}")
        if mod["name"] in local_mods:
            entry = cfg.setdefault(mod["name"], {})
            entry["mod_id"] = mod["id"]
            entry["game_scoped_id"] = mod["game_scoped_id"]
            if len(active) == 1:
                entry["file_id"] = active[0]["id"]
                entry.setdefault("file_name", mod["name"])
            else:
                print(f"    -> {len(active)} active files; set file_id for {mod['name']} in {CONFIG.name} by hand")
    missing = [m for m in local_mods if m not in {x["name"] for x in found}]
    if missing:
        print(f"no Nexus mod named exactly like: {', '.join(missing)} (map them in {CONFIG.name} by hand)")
    if args.write:
        CONFIG.write_text(json.dumps(cfg, indent=2) + "\n")
        print(f"wrote {CONFIG.relative_to(ROOT)}")


def load_config(required=True):
    if CONFIG.exists():
        return json.loads(CONFIG.read_text())
    if required:
        die(f"{CONFIG.relative_to(ROOT)} missing; run `lookup --write` first")
    return {}


# ---------------------------------------------------------------- publish

def mod_version(mod):
    m = VERSION_RE.search((MODS_DIR / mod / "dllmain.cpp").read_text())
    if not m:
        die(f"ModVersion not found in {mod}/dllmain.cpp")
    return m.group(1)


def version_key(v):
    parts = [int(x) for x in re.findall(r"\d+", v)]
    while parts and parts[-1] == 0:
        parts.pop()
    return parts


def changelog_path(mod):
    return MODS_DIR / mod / "CHANGELOG.md"


def changelog_entries(mod, version):
    """Bullets of the `## <version>` section; one bullet = one Nexus changelog entry."""
    path = changelog_path(mod)
    if not path.exists():
        return []
    entries, current = [], None
    for line in path.read_text().splitlines():
        if line.startswith("## "):
            current = version_key(line[3:].strip()) == version_key(version)
        elif current and line.startswith("- "):
            entries.append(line[2:].strip())
        elif current and line.startswith("  ") and entries:
            entries[-1] += " " + line.strip()
    return [e for e in entries if e]


def git_hint(mod):
    tags = git("tag", "--list", f"{mod}-v*", "--merged", "HEAD", "--sort=-v:refname").splitlines()
    rng = [f"{tags[0]}..HEAD"] if tags else ["-10"]
    return git("log", "--no-merges", "--format=  %h %s", *rng, "--", f"AbioticFactor/{mod}")


def cmd_pull_changelogs(args):
    key = api_key()
    for mod, entry in load_config().items():
        path = changelog_path(mod)
        if path.exists() and not args.force:
            print(f"skip {path.relative_to(ROOT)}: exists (--force to overwrite)")
            continue
        gid = entry.get("game_scoped_id") or die(f"{CONFIG.name}: {mod}.game_scoped_id missing; run `lookup --write`")
        data = request("GET", f"{API_V1}/games/{GAME_DOMAIN}/mods/{gid}/changelogs.json", headers={"apikey": key})
        out = [f"# {mod} changelog", ""]
        for ver in sorted(data, key=version_key, reverse=True):
            out += [f"## {ver}", *(f"- {html.unescape(e).strip()}" for e in data[ver]), ""]
        path.write_text("\n".join(out))
        print(f"wrote {path.relative_to(ROOT)} ({len(data)} versions)")


def build_and_test(mod):
    if not BUILD_DIR.exists():
        run(["cmake", "--preset", PRESET])
    run(["cmake", "--build", "--preset", PRESET, "--target", mod, f"{mod}ConfigTest"])
    exe = BUILD_DIR / "AbioticFactor" / mod / f"{mod}ConfigTest.exe"
    out = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True,
                         env={**os.environ, "WINEDEBUG": "-all"})
    print(out.stdout.rstrip())
    if out.returncode != 0 or "ALL PASS" not in out.stdout:
        die(f"{exe.name} did not print ALL PASS (exit {out.returncode})")
    return BUILD_DIR / "AbioticFactor" / mod / f"{mod}.dll"


def package(mod, version, dll):
    DIST.mkdir(exist_ok=True)
    archive = DIST / f"{mod}-{version}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(dll, f"{mod}/dlls/{mod}.dll")
        z.write(MODS_DIR / mod / "enabled.txt", f"{mod}/enabled.txt")
    print(f"packaged {archive.relative_to(ROOT)} ({archive.stat().st_size} bytes)")
    return archive


def upload_archive(archive, key):
    data = archive.read_bytes()
    digest = hashlib.md5(data)
    up = api("POST", "/uploads", key, {"size_bytes": len(data), "filename": archive.name,
                                       "md5": digest.hexdigest()})["data"]
    request("PUT", up["presigned_url"], body=data, headers={
        "Content-Disposition": f'attachment; filename="{archive.name}"',
        "Content-MD5": base64.b64encode(digest.digest()).decode(),
        "Content-Type": "application/octet-stream",
    })
    api("POST", f"/uploads/{up['id']}/finalise", key)
    delay = 2.0
    for _ in range(40):
        state = api("GET", f"/uploads/{up['id']}", key)["data"]["state"]
        print(f"upload {up['id']}: {state}")
        if state == "available":
            return up["id"]
        time.sleep(delay)
        delay = min(delay * 1.5, 30)
    die(f"upload {up['id']} never became available")


def cmd_publish(args):
    mod = args.mod
    if not (MODS_DIR / mod / "dllmain.cpp").exists():
        die(f"unknown mod {mod!r}")
    entry = load_config(required=not args.dry_run).get(mod, {})
    for k in ("mod_id", "file_id"):
        if not entry.get(k) and not args.dry_run:
            die(f"{CONFIG.name}: {mod}.{k} missing; run `lookup --write`")

    version = mod_version(mod)
    tag = f"{mod}-v{version}"
    if git("tag", "--list", tag):
        die(f"tag {tag} exists: bump ModVersion in {mod}/dllmain.cpp")
    dirty = git("status", "--porcelain", "--", f"AbioticFactor/{mod}")
    if dirty and not args.allow_dirty:
        die(f"uncommitted changes in AbioticFactor/{mod} (the release must match the tagged commit):\n{dirty}")

    entries = changelog_entries(mod, version)
    if not entries and not args.dry_run:
        die(f"no `## {version}` section with `- ` entries in AbioticFactor/{mod}/CHANGELOG.md; "
            f"commits since the last release:\n{git_hint(mod)}")
    changelog = "\n".join(f"- {e}" for e in entries)

    key = None if args.dry_run else api_key()
    if key:
        versions = api("GET", f"/mod-files/{entry['file_id']}/versions", key)["data"]["versions"]
        latest = max((v["version"] for v in versions if v["category"] != "removed"), key=version_key, default=None)
        if latest is not None and version_key(version) <= version_key(latest):
            die(f"{mod} {version} is not newer than {latest} on Nexus: bump ModVersion in {mod}/dllmain.cpp")

    dll = build_and_test(mod)
    archive = package(mod, version, dll)
    print(f"\n{mod} {version} -> Nexus mod {entry.get('mod_id')}, file {entry.get('file_id')}\nchangelog:\n{changelog}\n")
    if args.dry_run:
        print("dry run: nothing uploaded or tagged")
        return
    if not args.yes and input("upload? [y/N] ").strip().lower() != "y":
        die("aborted")

    upload_id = upload_archive(archive, key)
    body = {
        "upload_id": upload_id,
        "name": entry.get("file_name", mod),
        "version": version,
        "file_category": "main",
        "update_mod_version": True,
        # false = previous version moves to "Old files"; true would hide it as archived
        "archive_existing_file": False,
    }
    if entry.get("description"):
        body["description"] = entry["description"]
    created = api("POST", f"/mod-files/{entry['file_id']}/versions", key, body)["data"]
    print(f"created file version {created['version']['id']}")
    for e in entries:  # one POST per entry, matching how Nexus stores them
        api("POST", f"/mods/{entry['mod_id']}/changelogs", key, {"version": version, "changelog": e})
    print(f"changelog added ({len(entries)} entries)")

    run(["git", "tag", "-a", tag, "-m", f"{mod} {version}\n\n{changelog}"])
    if args.push_tag:
        run(["git", "push", "origin", tag])
    else:
        print(f"tag {tag} created locally; push with: git push origin {tag}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    lk = sub.add_parser("lookup", help="list your Nexus mods/files for the game")
    lk.add_argument("--write", action="store_true", help=f"merge found ids into {CONFIG.name}")
    lk.set_defaults(func=cmd_lookup)
    pc = sub.add_parser("pull-changelogs", help="write <Mod>/CHANGELOG.md from Nexus")
    pc.add_argument("--force", action="store_true", help="overwrite existing CHANGELOG.md files")
    pc.set_defaults(func=cmd_pull_changelogs)
    pb = sub.add_parser("publish", help="build, test, zip and upload one mod")
    pb.add_argument("mod")
    pb.add_argument("--dry-run", action="store_true", help="build, test and zip only")
    pb.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
    pb.add_argument("--push-tag", action="store_true", help="push the release tag to origin")
    pb.add_argument("--allow-dirty", action="store_true", help="allow uncommitted changes in the mod dir")
    pb.set_defaults(func=cmd_publish)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
