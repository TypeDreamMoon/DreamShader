#!/usr/bin/env python3
"""Refresh loctext.json from the source tree, and report what translations.json is missing.

This is the middle link of the localization chain:

    Source/**/*.{h,cpp}
        |  localization_lint.ps1 -Json      (scan, this script shells out to it)
        v
    loctext.json                            (written here)
    translations.json                       (hand-authored zh-Hans, never written here)
        |  build_locres.py
        v
    DreamShader.locres  ->  Content/Localization/DreamShader/zh-Hans/

None of it needs the editor: the scan is regex over the source, and build_locres.py
writes the LocRes container itself.

Usage:
    python sync_loctext.py            # rewrite loctext.json, print the diff summary
    python sync_loctext.py --check    # report only, touch nothing; exit 1 if stale

The 'src' field must be the string the RUNTIME sees, not the C++ literal as written:
build_locres.py stores FCrc::StrCrc32(src) in the LocRes, and FTextLocalizationManager
throws the translation away when that hash does not match the live source string. So
localization_lint.ps1 decodes the C escape sequences, and so must anything else here.
"""
import json
import io
import os
import subprocess
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(os.path.dirname(BASE))
SOURCE_PREFIX = os.path.join(PLUGIN, "Source") + os.sep
LOCTEXT_PATH = os.path.join(BASE, "loctext.json")
TRANSLATIONS_PATH = os.path.join(BASE, "translations.json")
LINT_PATH = os.path.join(BASE, "localization_lint.ps1")


def run_lint():
    """Run localization_lint.ps1 -Json and return its parsed output.

    The inventory holds em dashes, ellipses and box-drawing characters, and a
    redirected PowerShell stdout is written in the console code page, not UTF-8.
    So the script writes to a temp file with an encoding we name, and we read that.
    """
    import tempfile
    fd, tmp = tempfile.mkstemp(suffix=".json", prefix="dsloctext-")
    os.close(fd)
    command = (
        "$ErrorActionPreference = 'Stop'; "
        "& '%s' -Json | Out-File -LiteralPath '%s' -Encoding utf8"
        % (LINT_PATH.replace("'", "''"), tmp.replace("'", "''"))
    )
    try:
        for exe in ("pwsh", "powershell"):
            try:
                proc = subprocess.run(
                    [exe, "-NoProfile", "-NonInteractive", "-Command", command],
                    capture_output=True, check=False)
            except FileNotFoundError:
                continue
            if proc.returncode != 0:
                sys.stderr.write(proc.stderr.decode("utf-8", "replace"))
                raise SystemExit("localization_lint.ps1 failed (exit %d)" % proc.returncode)
            return json.load(io.open(tmp, encoding="utf-8-sig"))
        raise SystemExit("neither pwsh nor powershell was found on PATH")
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass


def to_entries(lint):
    """Inventory rows -> loctext.json rows, with paths made relative to Source/."""
    entries = []
    for row in lint["inventory"]:
        path = row["file"]
        if not path.startswith(SOURCE_PREFIX):
            raise SystemExit("inventory entry outside Source/: %s" % path)
        entries.append({
            "ns": row["namespace"],
            "key": row["key"],
            "src": row["text"],
            "file": path[len(SOURCE_PREFIX):],
            "line": row["line"],
        })
    entries.sort(key=lambda e: (e["ns"], e["key"]))
    return entries


def write_json(path, data):
    """Write UTF-8, no BOM, CRLF, one-space indent -- the shape already checked in."""
    text = json.dumps(data, ensure_ascii=False, indent=1)
    with io.open(path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(text)
        f.write("\n")


def main():
    check_only = "--check" in sys.argv[1:]

    lint = run_lint()
    entries = to_entries(lint)

    old = []
    if os.path.exists(LOCTEXT_PATH):
        old = json.load(io.open(LOCTEXT_PATH, encoding="utf-8"))
    oldmap = {(e["ns"], e["key"]): e for e in old}
    newmap = {(e["ns"], e["key"]): e for e in entries}

    added = sorted(k for k in newmap if k not in oldmap)
    removed = sorted(k for k in oldmap if k not in newmap)
    changed = sorted(k for k in newmap
                     if k in oldmap and newmap[k]["src"] != oldmap[k]["src"])
    moved = sorted(k for k in newmap
                   if k in oldmap and newmap[k]["file"] != oldmap[k]["file"])

    print("loctext: %d -> %d entries (+%d, -%d, %d source-text changed, %d moved file)"
          % (len(old), len(entries), len(added), len(removed), len(changed), len(moved)))
    for k in added:
        print("  + %s | %s" % k)
    for k in removed:
        print("  - %s | %s   (its zh-Hans translation is now orphaned)" % k)
    for k in changed:
        print("  ~ %s | %s   (source text changed; re-check the translation)" % k)

    trans = json.load(io.open(TRANSLATIONS_PATH, encoding="utf-8"))
    missing = [e for e in entries if not trans.get(e["ns"] + "|" + e["key"])]
    live = set(e["ns"] + "|" + e["key"] for e in entries)
    orphan = sorted(k for k in trans if k not in live)

    print("translations: %d entries, %d missing, %d orphaned"
          % (len(trans), len(missing), len(orphan)))
    for e in missing:
        print("  MISSING %s | %s :: %s" % (e["ns"], e["key"], e["src"]))
    for k in orphan:
        print("  ORPHAN  %s" % k)

    if check_only:
        stale = bool(added or removed or changed or moved)
        if stale:
            print("STALE: run 'python sync_loctext.py' to refresh loctext.json")
        if missing:
            print("STALE: %d entries have no zh-Hans translation" % len(missing))
        return 1 if (stale or missing) else 0

    write_json(LOCTEXT_PATH, entries)
    print("wrote %s" % LOCTEXT_PATH)
    if missing:
        print("NOTE: %d entries still need a zh-Hans string in translations.json" % len(missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
