#!/usr/bin/env python3
"""Synchronize CORE's embedded Studio asset with the reviewed web-studio source.

Run before ESP-IDF configure/build; no device is flashed by this script.
"""
from __future__ import annotations

import argparse
import gzip
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "web-studio" / "opentail.htm"
DESTINATION = ROOT / "core-firmware" / "main" / "assets" / "studio.htm.gz"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="Verify the embedded asset matches the source without writing")
    args = parser.parse_args()
    if not SOURCE.is_file():
        print(f"ERROR: Studio source missing: {SOURCE}", file=sys.stderr)
        return 2
    raw = SOURCE.read_bytes()
    if not raw.startswith(b"<!doctype html>") or b"ot-core-output-advanced" not in raw:
        print("ERROR: Studio source is missing expected HTML or Output Manager fix", file=sys.stderr)
        return 2
    compressed = gzip.compress(raw, compresslevel=9, mtime=0)
    if gzip.decompress(compressed) != raw:
        print("ERROR: Studio compression round-trip failed", file=sys.stderr)
        return 2
    if args.check:
        if not DESTINATION.is_file() or gzip.decompress(DESTINATION.read_bytes()) != raw:
            print("ERROR: embedded Studio asset is out of sync", file=sys.stderr)
            return 1
        print("PASS: embedded Studio matches reviewed source")
        return 0
    DESTINATION.parent.mkdir(parents=True, exist_ok=True)
    DESTINATION.write_bytes(compressed)
    print(f"Studio embedded asset synchronized: {len(raw)} bytes -> {len(compressed)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
