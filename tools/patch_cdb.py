#!/usr/bin/env python3
"""Append compile_commands.json coverage for tools/screenshot_window.cpp.

The screenshot tool is a dev asset compiled outside SOURCES, so qmake's
database never lists it and clangd falls back to empty flags -> bogus
'file not found' cascades. This appends an entry mirroring the src flags
plus -Isrc/-Itools. Idempotent; run after qmake regenerates the database.
"""
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve()
cdb = root / "compile_commands.json"
if not cdb.exists():
    sys.exit(0)
try:
    db = json.loads(cdb.read_text())
except (OSError, ValueError):
    sys.exit(0)  # best-effort: a broken database must not break the build
if any(e["file"].endswith("tools/screenshot_window.cpp") for e in db):
    sys.exit(0)
ref = next((e for e in db if e["file"].endswith("src/theme.cpp")), None)
if ref is None:
    sys.exit(0)
db.append({
    "directory": str(root),
    "file": str(root / "tools" / "screenshot_window.cpp"),
    "command": ref["command"]
    .replace("src/theme.cpp", "tools/screenshot_window.cpp")
    .replace("-o theme.o", "-o screenshot_window.o")
    .replace("-I/usr/include/qt6", "-Isrc -Itools -I/usr/include/qt6", 1),
})
cdb.write_text(json.dumps(db, indent=1))
print(f"patched {cdb}: +tools/screenshot_window.cpp")
