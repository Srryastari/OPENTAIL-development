#!/usr/bin/env python3
"""Package only exact-CI-built OpenTail ESP32-S3 images for Windows COM9.
Never builds CMake, flashes a device, or erases NVS.
"""
from argparse import ArgumentParser
from pathlib import Path
import hashlib
import json
import shutil

parser = ArgumentParser()
parser.add_argument("--build", required=True, type=Path)
parser.add_argument("--out", required=True, type=Path)
parser.add_argument("--sha", required=True)
parser.add_argument("--version", required=True)
args = parser.parse_args()
build = args.build.resolve()
out = args.out.resolve()
root = Path(__file__).resolve().parent.parent
assert len(args.sha) == 40 and all(ch in "0123456789abcdef" for ch in args.sha.lower())
assert f'set(PROJECT_VER "{args.version}")' in (root/"core-firmware/CMakeLists.txt").read_text()
assert f'#define OT_FW_VERSION "{args.version}"' in (root/"core-firmware/main/ot_version.h").read_text()
source_manifest = json.loads((build/"flasher_args.json").read_text())
expected = {
    0x0: "bootloader/bootloader.bin",
    0x8000: "partition_table/partition-table.bin",
    0xF000: "ota_data_initial.bin",
    0x20000: "opentail_core_alpha.bin",
}
actual = {int(offset, 0): name for offset, name in source_manifest["flash_files"].items()}
assert actual == expected, (actual, expected)
assert 0x9000 not in actual, "Never erase live NVS at 0x9000"
app = (build/expected[0x20000]).read_bytes()
assert args.version.encode("ascii") in app, "Compiled version missing from actual app BIN"
if out.exists():
    raise SystemExit("Refusing overwrite of previous package: " + str(out))
out.mkdir(parents=True)
for rel in expected.values():
    src = build/rel
    assert src.is_file() and src.stat().st_size > 0, rel
    dest = out/rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
shutil.copy2(build/"flasher_args.json", out/"flasher_args.json")
manifest = {
    "product": "OpenTail CORE",
    "version": args.version,
    "git_sha": args.sha,
    "chip": "ESP32-S3 N16R8",
    "esp_idf": "v5.5.5",
    "port": "COM9",
    "baud": 115200,
    "flasher_offsets": {hex(k):v for k,v in expected.items()},
    "nvs_partition": "0x9000",
    "erase_nvs": False,
    "physical_boot_status": "NOT_RUN",
    "signed_release": False,
    "road_safe": False,
}
(out/"release_manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
scripts = {
    "01_FLASH_COM9.bat": r"""@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo OpenTail 0.1.32-alpha BENCH ONLY. Never erase NVS at 0x9000.
py -3 verify_sha256.py
if errorlevel 1 (echo SHA-256 validation FAILED. Nothing flashed. & exit /b 1)
echo Close other COM9 serial monitors before continuing.
set /p CONFIRM=Type FLASH to write the verified images to COM9:
if /I not "%CONFIRM%"=="FLASH" (echo Cancelled. & exit /b 1)
py -3 -m esptool --chip esp32s3 --port COM9 --baud 115200 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0xf000 ota_data_initial.bin 0x20000 opentail_core_alpha.bin
if errorlevel 1 (echo Flash failed. & exit /b 1)
echo Flash finished. Run 02_CAPTURE_BOOT_LOG_COM9.bat now.
pause
""",
    "02_CAPTURE_BOOT_LOG_COM9.bat": r"""@echo off
setlocal
cd /d "%~dp0"
echo Close any other COM9 serial monitor. Press EN/RESET after opening.
py -3 capture_boot.py
if errorlevel 1 (echo Capture failed. Check pyserial, COM9, and cable.)
pause
""",
    "INSTALL_ESPTOOL.bat": r"""@echo off
setlocal
echo Install Python 3 dependencies for the COM9 bench tools.
py -3 -m pip install --user --upgrade esptool pyserial
if errorlevel 1 (echo Python 3 launcher or installer not found. & exit /b 1)
py -3 -m esptool version
pause
""",
}
for name, body in scripts.items():
    (out/name).write_bytes(body.strip().replace("\n","\r\n").encode("ascii"))
(out/"verify_sha256.py").write_text("""from pathlib import Path
import hashlib, sys
root=Path(__file__).resolve().parent
ok=True
for record in (root/"SHA256SUMS.txt").read_text().splitlines():
    expected,rel=record.split("  ",1)
    actual=hashlib.sha256((root/rel).read_bytes()).hexdigest()
    print(("PASS" if actual==expected else "FAIL"),rel)
    ok &= actual==expected
sys.exit(0 if ok else 1)
""")
(out/"capture_boot.py").write_text("""from pathlib import Path
from datetime import datetime
import serial
folder=Path(__file__).resolve().parent/"logs"
folder.mkdir(exist_ok=True)
target=folder/("COM9_boot_"+datetime.now().strftime("%Y%m%d_%H%M%S")+".txt")
print("Opening COM9 at 115200. Press EN/RESET. Ctrl+C saves log:",target,flush=True)
try:
    with serial.Serial("COM9",115200,timeout=0.5) as device, target.open("wb") as log:
        while True:
            data=device.read(4096)
            if data:
                log.write(data)
                log.flush()
                print(data.decode("utf-8",errors="replace"),end="",flush=True)
except KeyboardInterrupt:
    print("\\nSaved:",target)
""")
(out/"START_HERE.txt").write_text(
    ("OpenTail CORE " + args.version + " | Git SHA " + args.sha + "\n"
    "WINDOWS BENCH ONLY. Extract this artifact ZIP to C:\\OpenTail\\EasyFlash\\\n\n"
    "1. If needed, run INSTALL_ESPTOOL.bat to install esptool + pyserial.\n"
    "2. Close ESP-IDF, Arduino, and other COM9 serial monitors.\n"
    "3. Run 01_FLASH_COM9.bat and type FLASH after SHA-256 checks pass.\n"
    "4. Run 02_CAPTURE_BOOT_LOG_COM9.bat, press EN/RESET, capture two boots.\n"
    "5. Press Ctrl+C to save logs. Return the COM9 log for physical review.\n\n"
    "Four verified offsets: 0x0 bootloader, 0x8000 partition, 0xf000 OTA data,\n"
    "0x20000 app. NVS at 0x9000 remains untouched.\n"
    "Stable renderer/BLE/Wi-Fi without panic is required; neither CI nor flashing\n"
    "alone constitutes physical acceptance. Do not use on the road.\n").replace("\n","\r\n"))
hashed = [*expected.values(), "flasher_args.json", "release_manifest.json"]
(out/"SHA256SUMS.txt").write_text("".join(
    hashlib.sha256((out/path).read_bytes()).hexdigest()+"  "+path+"\n" for path in hashed))
for record in (out/"SHA256SUMS.txt").read_text().splitlines():
    expected_hash, name = record.split("  ",1)
    assert hashlib.sha256((out/name).read_bytes()).hexdigest() == expected_hash
assert all((out/name).is_file() for name in [
    "01_FLASH_COM9.bat","02_CAPTURE_BOOT_LOG_COM9.bat",
    "INSTALL_ESPTOOL.bat","START_HERE.txt","flasher_args.json"])
print("PASS: four real compiled images, exact offsets, version, SHA-256, ZIP-root BATs; NVS untouched.")
