"""sync_ui.py — populate ./interface/ from canonical ../esp-rack/ui/.

Architectural contract: the React frontend lives ONLY in the library
(esp-rack/ui/). The demo has zero per-service React — every C++ service
declares its UI via FormBuilder/WebFeatureSpec, and the library renders
dynamically. So the consumer's `interface/` is a build artefact, not a
source of truth.

This script is the bridge: pre:build it copies the library tree into
`./interface/` so build_interface.py can run npm against a complete
project. Re-running is idempotent.

The consumer can override individual files by dropping them under
./interface_overrides/ — that overlay is applied AFTER the library
copy. Today no overrides exist; the hook is here for the rare case
(custom favicon / project-specific theme tweak) without touching
library files.

Failure modes:
  * `../esp-rack/ui/` missing -> fail fast with a clear message.
  * `interface/node_modules` and `interface/build` are PRESERVED
    across syncs (npm install + webpack output, expensive to redo).
"""

from pathlib import Path
from shutil import copy2, copytree, rmtree
import os
import sys

Import("env")  # noqa: F821  (PlatformIO injects)

ROOT          = Path.cwd()
LIBRARY_UI    = ROOT.parent / "esp-rack" / "ui"
INTERFACE_DIR = ROOT / "interface"
OVERRIDES_DIR = ROOT / "interface_overrides"

# Files at the top level of ../esp-rack/ui/ — flat copy. Anything not
# listed here is ignored (e.g. node_modules / build live elsewhere).
LIBRARY_PAYLOAD_FILES = [
    "config-overrides.js",
    "package.json",
    "progmem-generator.js",
    "tsconfig.json",
]

# Top-level subdirs that are FULLY refreshed on each sync. node_modules/
# and build/ are NOT in this list — they live inside `interface/` but
# are populated by npm and survive the sync untouched.
LIBRARY_PAYLOAD_DIRS = [
    "src",
    "public",
]


def fail(msg: str) -> None:
    print(f"[sync_ui] ERROR: {msg}", file=sys.stderr)
    raise SystemExit(1)


def replace_dir(src: Path, dst: Path) -> None:
    if dst.exists():
        rmtree(dst)
    copytree(src, dst)


def overlay_dir(src: Path, dst: Path) -> None:
    if not src.exists():
        return
    for cur, _dirs, files in os.walk(src):
        rel = Path(cur).relative_to(src)
        target = dst / rel
        target.mkdir(parents=True, exist_ok=True)
        for f in files:
            copy2(Path(cur) / f, target / f)


def sync() -> None:
    if not LIBRARY_UI.is_dir():
        fail(f"canonical UI not found at {LIBRARY_UI}; clone esp-rack as a sibling repo")

    INTERFACE_DIR.mkdir(parents=True, exist_ok=True)

    for f in LIBRARY_PAYLOAD_FILES:
        src = LIBRARY_UI / f
        if not src.exists():
            print(f"[sync_ui] note: {f} missing from library; skipping")
            continue
        copy2(src, INTERFACE_DIR / f)

    for d in LIBRARY_PAYLOAD_DIRS:
        src = LIBRARY_UI / d
        if not src.exists():
            print(f"[sync_ui] note: {d}/ missing from library; skipping")
            continue
        replace_dir(src, INTERFACE_DIR / d)

    overlay_dir(OVERRIDES_DIR, INTERFACE_DIR)

    print(f"[sync_ui] synced {LIBRARY_UI} -> {INTERFACE_DIR}")


sync()
