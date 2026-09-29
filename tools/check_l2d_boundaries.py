#!/usr/bin/env python3
"""Fail when application code reaches past the public Live2D runtime API.

Scans firmware/ and examples/. A private adapter may include the internal
engine; its path must contain a /private/ directory. Public headers under
include/l2d must not include ESP-IDF, LVGL, Windows, or PainterEngine.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP_ROOTS = [ROOT / "firmware", ROOT / "examples"]
PUBLIC = ROOT / "include" / "l2d"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}
SKIP_DIRS = {"build", "managed_components", ".git"}
APP_PATTERNS = (
    "live2d_engine_",
    "live2d_engine.h",
    "PX_Live",
    "PX_Surface.h",
    "px_surface",
)
PUBLIC_PATTERNS = (
    "esp_",
    "freertos",
    "lvgl",
    "windows.h",
    "PainterEngine",
    "PX_",
    "driver/ppa.h",
)


def iter_sources(root: Path):
    if not root.exists():
        return
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
            continue
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        yield path


def main() -> int:
    failures = []
    for root in APP_ROOTS:
        for path in iter_sources(root):
            if "/private/" in path.as_posix():
                continue
            text = path.read_text(errors="replace")
            for pattern in APP_PATTERNS:
                if pattern in text:
                    failures.append(f"{path.relative_to(ROOT)}: contains {pattern}")
    if PUBLIC.exists():
        for path in iter_sources(PUBLIC):
            text = path.read_text(errors="replace")
            for pattern in PUBLIC_PATTERNS:
                if pattern in text:
                    failures.append(f"{path.relative_to(ROOT)}: public header contains {pattern}")
    if failures:
        print("l2d boundary check failed:")
        for item in failures:
            print(f"  {item}")
        return 1
    print("l2d boundary check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
