#!/usr/bin/env python3
"""Import all uploaded OpenTail ZIP parts without overwriting existing history."""
import csv, hashlib, io, pathlib, re, sys, zipfile
root=pathlib.Path(__file__).resolve().parents[1]
inbox=root/"import-inbox"
parts=sorted(inbox.glob("opentail-github-small-*-of-*.zip"))
if not parts:
    sys.exit("No opentail-github-import ZIP parts in import-inbox/")
m=re.fullmatch(r"opentail-github-small-(\d+)-of-(\d+)\.zip",parts[0].name)
expected=int(m.group(2))
if len(parts)!=expected or [int(re.fullmatch(r"opentail-github-small-(\d+)-of-(\d+)\.zip",p.name).group(1)) for p in parts]!=list(range(1,expected+1)):
    sys.exit(f"Expected all {expected} ZIP parts in numeric order")
files={}
for part in parts:
    with zipfile.ZipFile(part) as z:
        if z.testzip():sys.exit(f"Corrupt ZIP: {part}")
        for name in z.namelist():
            if name in files:sys.exit(f"Duplicate archive path: {name}")
            if name.startswith("/") or ".." in pathlib.PurePosixPath(name).parts:sys.exit(f"Unsafe path: {name}")
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
print(f"Verified and imported {len(records)} original files from {len(parts)} ZIP parts")
