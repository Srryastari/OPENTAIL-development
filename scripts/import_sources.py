#!/usr/bin/env python3
"""Import original pre-GitHub source archives without discarding release history.

Usage:
  python3 scripts/import_sources.py /path/to/opentail-pre-github-initial-source-bundle.zip
Run from repository root. Review the resulting git diff before committing.
"""
from pathlib import Path, PurePosixPath
from zipfile import ZipFile
import hashlib
import io
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "opentail-android-v0.1.15-approved-compact-status-easy-build.zip": ("android", "android-releases"),
    "opentail-core-v0.1.24-ble-boot-enrollment.zip": ("firmware", "firmware-releases"),
    "OpenTail-CORE-Flux-RevA-Design-Package.zip": ("hardware/flux-rev-a", "designs"),
}
STUDIO = "opentail-v0.19.50.htm"

def write_once(path: Path, data: bytes):
    if path.exists():
        if path.read_bytes() != data:
            raise RuntimeError(f"Existing file differs; refusing overwrite: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)

def safe_members(blob: bytes):
    with ZipFile(io.BytesIO(blob)) as z:
        entries = [(name, z.read(name)) for name in z.namelist() if not name.endswith("/")]
    for name, _ in entries:
        p = PurePosixPath(name)
        if p.is_absolute() or ".." in p.parts or not p.parts:
            raise RuntimeError(f"Unsafe archive path: {name}")
    return entries

def main(bundle: Path):
    with ZipFile(bundle) as outer:
        for filename, (active_dir, archive_dir) in FILES.items():
            raw = outer.read(filename)
            print(f"{filename}: SHA256 {hashlib.sha256(raw).hexdigest()}")
            write_once(ROOT / "archive" / archive_dir / filename, raw)
            entries = safe_members(raw)
            roots = {PurePosixPath(n).parts[0] for n, _ in entries}
            if len(roots) != 1:
                raise RuntimeError(f"Unexpected archive roots for {filename}: {roots}")
            for name, data in entries:
                rel = Path(*PurePosixPath(name).parts[1:])
                if not rel.parts:
                    continue
                if rel.parts[0] == ".github":
                    # Keep nested Android workflows as source documentation, not root CI.
                    target = ROOT / active_dir / rel
                else:
                    target = ROOT / active_dir / rel
                write_once(target, data)
        raw = outer.read(STUDIO)
        print(f"{STUDIO}: SHA256 {hashlib.sha256(raw).hexdigest()}")
        write_once(ROOT / "archive" / "studio-releases" / STUDIO, raw)
        write_once(ROOT / "studio" / STUDIO, raw)
    print("Import complete. Inspect git status and run available tests before commit.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python3 scripts/import_sources.py <source-bundle.zip>")
    main(Path(sys.argv[1]))
