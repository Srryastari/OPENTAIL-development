#!/usr/bin/env python3
"""Extract pinned OpenTail baselines from the immutable historical archive."""
import io
import pathlib
import shutil
import zipfile

root=pathlib.Path(__file__).resolve().parents[1]
archive=root/"import-inbox/opentail-complete-library-releases-archive.zip"
if not archive.exists():
    raise SystemExit(f"Missing {archive}")
packages={
    "core": "opentail-core-v0.1.24-ble-boot-enrollment.zip",
    "android": "opentail-android-v0.1.15-approved-compact-status-easy-build.zip",
    "hardware": "OpenTail-CORE-Flux-RevA-Design-Package.zip",
}
def safe_extract(z, dest, strip_first=True):
    members=[i for i in z.infolist() if not i.is_dir()]
    for info in members:
        path=pathlib.PurePosixPath(info.filename)
        parts=path.parts[1:] if strip_first else path.parts
        if not parts or any(p in (".","..") for p in parts) or path.is_absolute():
            raise ValueError(f"Unsafe archive path {info.filename}")
        target=dest.joinpath(*parts)
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(z.read(info))
    return len(members)

with zipfile.ZipFile(archive) as outer:
    for target,filename in packages.items():
        inner=zipfile.ZipFile(io.BytesIO(outer.read("original-files/"+filename)))
        with inner:
            count=safe_extract(inner,root/target)
        print(f"{target}: {count} source files extracted")
    studio=root/"studio"
    studio.mkdir(exist_ok=True)
    (studio/"opentail-v0.19.50.htm").write_bytes(outer.read("original-files/opentail-v0.19.50.htm"))
    shutil.copyfile(studio/"opentail-v0.19.50.htm",studio/"opentail.htm")
    print("studio: versioned baseline and stable deployment copy extracted")
print("Source extraction completed; builds and hardware tests are NOT verified.")
