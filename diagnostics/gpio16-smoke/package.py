#!/usr/bin/env python3
"""Package independently built ESP-IDF firmware and a checksummed Windows runner."""
from argparse import ArgumentParser
from pathlib import Path
import hashlib
import json
import shutil

parser=ArgumentParser()
parser.add_argument("--source-sha",required=True)
args=parser.parse_args()
assert len(args.source_sha)==40 and all(c in "0123456789abcdef" for c in args.source_sha)
base=Path(__file__).resolve().parent
build=base/"build"
out=build/"SmokePackage"
assert not out.exists(),"Do not overwrite a package"
out.mkdir()
paths=(
    "bootloader/bootloader.bin",
    "partition_table/partition-table.bin",
    "ota_data_initial.bin",
    "opentail_gpio16_smoke.bin",
    "flasher_args.json"
)
expected={
    0x0:paths[0], 0x8000:paths[1],
    0xf000:paths[2], 0x20000:paths[3]
}
actual=json.loads((build/"flasher_args.json").read_text())["flash_files"]
assert {int(k,0):v for k,v in actual.items()}==expected, "Flash offsets mismatch"
assert (build/paths[2]).stat().st_size==8192,"OTA data size mismatch"
for rel in paths:
    src=build/rel
    assert src.is_file() and src.stat().st_size>0,rel
    dst=out/rel
    dst.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(src,dst)
for name in ("Flash-GPIO16-Smoke.ps1","RUN-GPIO16-SMOKE.bat","README.md"):
    shutil.copy2(base/name,out/name)
manifest={
    "test":"gpio16-rmt-smoke-1",
    "source_sha":args.source_sha,
    "idf":"5.5.5",
    "device":"ESP32-S3 N16R8",
    "gpio":16,
    "pixels":8,
    "brightness":204,
    "dma":False,
    "colors":["red","green","blue","white"],
    "nvs_erase":False,
    "nvs_offset":"0x9000",
    "flashed":False,
    "physically_verified":False,
    "flash_offsets":{hex(k):v for k,v in expected.items()}
}
(out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
hashed=(*paths,"manifest.json")
(out/"SHA256SUMS.txt").write_text("".join(
    hashlib.sha256((out/n).read_bytes()).hexdigest()+"  "+n+"\n" for n in hashed
))
for line in (out/"SHA256SUMS.txt").read_text().splitlines():
    expected_hash,rel=line.split("  ",1)
    assert hashlib.sha256((out/rel).read_bytes()).hexdigest()==expected_hash
print("PASS isolated 80% 8-LED GPIO16 images/manifest/runner SHA="+args.source_sha)
