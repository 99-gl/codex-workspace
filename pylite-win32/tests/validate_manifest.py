#!/usr/bin/env python3
"""Reject DPI declarations that can fail during Windows activation-context setup."""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


OLD_DPI = "http://schemas.microsoft.com/SMI/2005/WindowsSettings"
NEW_DPI = "http://schemas.microsoft.com/SMI/2016/WindowsSettings"


def normalized_text(element: ET.Element) -> str:
    return (element.text or "").strip()


def validate(path: Path) -> list[str]:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as exc:
        return [f"cannot parse manifest: {exc}"]

    errors: list[str] = []
    legacy = root.findall(f".//{{{OLD_DPI}}}dpiAware")
    modern = root.findall(f".//{{{NEW_DPI}}}dpiAwareness")

    if len(legacy) != 1:
        errors.append("manifest must contain exactly one 2005 dpiAware element")
    elif normalized_text(legacy[0]).lower() != "true/pm":
        errors.append("2005 dpiAware must be 'true/pm'; PerMonitorV2 is unsupported there")

    if len(modern) != 1:
        errors.append("manifest must contain exactly one 2016 dpiAwareness element")
    else:
        modes = [part.strip().lower() for part in normalized_text(modern[0]).split(",")]
        if modes != ["permonitorv2", "permonitor"]:
            errors.append("2016 dpiAwareness must be 'PerMonitorV2, PerMonitor'")

    return errors


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} APP.MANIFEST", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    errors = validate(path)
    if errors:
        for error in errors:
            print(f"manifest validation failed: {error}", file=sys.stderr)
        return 1

    print("Manifest validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
