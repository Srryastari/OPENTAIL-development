#!/usr/bin/env python3
"""Import all uploaded OpenTail ZIP parts without overwriting existing history."""
import csv, hashlib, io, pathlib, re, sys, zipfile
root=pathlib.Path(__file__).resolve().parents[1]
inbox=root/"import-inbox"
archive=inbox/"opentail-complete-library-releases-archive.zip"
if not archive.is_file():
    sys.exit(f"Missing archive: {archive}")
files={}
with zipfile.ZipFile(archive) as z:
    bad=z.testzip()
    if bad:sys.exit(f"Corrupt ZIP member: {bad}")
    for name in z.namelist():
        if name.startswith("/") or ".." in pathlib.PurePosixPath(name).parts:
            sys.exit(f"Unsafe path: {name}")
        files[name]=z.read(name)
manifest=files.get("SHA256-MANIFEST.csv")
if not manifest:sys.exit("Missing SHA256-MANIFEST.csv")
records=list(csv.DictReader(io.StringIO(manifest.decode("utf-8-sig"))))
for rec in records:
    name="original-files/"+rec["filename"]
    data=files.get(name)
    if data is None:sys.exit(f"Missing: {name}")
    if len(data)!=int(rec["size_bytes"]) or hashlib.sha256(data).hexdigest()!=rec["sha256"]:
        sys.exit(f"Checksum mismatch: {name}")
originals=[n for n in files if n.startswith("original-files/") and not n.endswith("/")]
if len(originals)!=len(records):sys.exit(f"Manifest covers {len(records)} but archive has {len(originals)} originals")
for name,data in files.items():
    dest=root/"archive"/"library-releases"/name
    if dest.exists() and dest.read_bytes()!=data:sys.exit(f"Refusing overwrite: {dest}")
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_bytes(data)
print(f"Verified and imported {len(records)} original files from the single ZIP archive")
