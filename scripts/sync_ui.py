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
INTERFACE_DIR = ROOT / "interface"
OVERRIDES_DIR = ROOT / "interface_overrides"

# Resolve where the canonical UI lives. Two supported modes:
#   1. Local-dev — sibling clone: ../esp-rack/ui/
#      (lib_extra_dirs in platformio.ini points at ../esp-rack/lib +
#      ../esp-rack/modules; we mirror that convention for UI)
#   2. Git-pull — released library cloned by PIO into
#      .pio/libdeps/<env>/ESPRack/ui/ via lib_deps = github URL
# Local sibling wins when present so active framework dev iterates
# without re-tagging releases. PIO-cloned location is the fallback,
# resolved per build env via env["PIOENV"].
def _is_local_dev_mode() -> bool:
    """Detect which library-consumption mode platformio.ini is in.
    Returns True if `lib_extra_dirs` references a sibling `esp-rack`
    path (local-dev mode); False if released-mode (lib_deps github
    URL only). Mirrors what PIO LDF will actually do — keeps sync_ui
    aligned with where PIO compiles from instead of resolving against
    leftover dirs from a previous mode.
    """
    raw = env.GetProjectOption("lib_extra_dirs", "")  # noqa: F821
    if isinstance(raw, (list, tuple)):
        joined = "\n".join(str(p) for p in raw)
    else:
        joined = str(raw or "")
    return "esp-rack" in joined


def _resolve_library_root() -> Path:
    """Return the root of esp-rack (the dir containing lib/, modules/,
    ui/) for the active mode.

    Local-dev mode: sibling `../esp-rack/` (matches lib_extra_dirs).
    Released mode:  PIO-cloned `.pio/libdeps/<env>/ESPRack/`.

    The detection reads `lib_extra_dirs` from platformio.ini so a
    mode switch (uncomment one block, comment the other) takes effect
    without manual cache-cleaning. A leftover `.pio/libdeps/.../ESPRack`
    from a previous released-mode build is correctly ignored when the
    consumer flips back to local-dev.
    """
    pio_env = env["PIOENV"]  # noqa: F821
    sibling = ROOT.parent / "esp-rack"
    cloned  = ROOT / ".pio" / "libdeps" / pio_env / "ESPRack"

    if _is_local_dev_mode():
        if (sibling / "ui").is_dir():
            return sibling
        # Local-dev configured but sibling missing — fail loud, the
        # downstream error will name the missing location.
        return sibling

    # Released-mode (or implicit when lib_extra_dirs absent).
    if (cloned / "ui").is_dir():
        return cloned
    # Neither found — return cloned path so the "missing UI" message
    # points at where PIO is expected to have populated.
    return cloned

def _purge_stale_cloned_lib() -> None:
    """When the demo is in local-dev mode, a leftover `.pio/libdeps/<env>/
    ESPRack/` from a previous released-mode build creates two libraries
    named "ESPRack" in PIO's LDF view (sibling + cloned), each with its
    own library.json. PIO's compat=strict resolution then becomes
    non-deterministic: usually picks one, sometimes warns, occasionally
    links against the wrong one. Pre-emptively clean the cloned copy
    so local-dev builds always resolve against the sibling.
    """
    if not _is_local_dev_mode():
        return
    pio_env = env["PIOENV"]  # noqa: F821
    stale = ROOT / ".pio" / "libdeps" / pio_env / "ESPRack"
    if stale.exists():
        rmtree(stale)
        print(f"[sync_ui] removed stale cloned lib at {stale}")


_purge_stale_cloned_lib()
LIBRARY_ROOT = _resolve_library_root()
LIBRARY_UI   = LIBRARY_ROOT / "ui"

# Webpack's progmem-generator must write WWWData.h into the library's
# include dir so App.cpp's `#include <WWWData.h>` resolves at C++
# compile time. The path differs by mode (sibling vs PIO-cloned), so
# we resolve here and propagate via env var; config-overrides.js
# reads ESPRACK_INCLUDE_DIR and builds the outputPath accordingly.
# os.environ persists into env.Execute("npm run build") in the
# follow-up build_interface.py via standard subprocess env inheritance.
LIBRARY_INCLUDE_DIR = (LIBRARY_ROOT / "lib" / "ESPRack" / "include").resolve()
os.environ["ESPRACK_INCLUDE_DIR"] = str(LIBRARY_INCLUDE_DIR)

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
