#!/usr/bin/env python3
"""Create and independently verify a real ESP-IDF 5.5.5 ESP32-S3 flash ZIP.

No image generation, flashing, NVS modification, serial IO or padding occurs.
All binaries MUST come from the actual same-SHA IDF build output.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shlex
import zipfile

IMAGE_PATHS = {
    0x0: "bootloader/bootloader.bin",
    0x8000: "partition_table/partition-table.bin",
    0xF000: "ota_data_initial.bin",
    0x20000: "opentail_core_alpha.bin",
}
ANCILLARY = ("flasher_args.json", "flash_args", "partitions_ota.csv", "sdkconfig.defaults")
PROVENANCE = "opentail-flash-provenance.json"
SHA_FILE = "SHA256SUMS.txt"
REQUIRED_ARCHIVE = set(IMAGE_PATHS.values()) | set(ANCILLARY) | {PROVENANCE, SHA_FILE}
SHA_RE = re.compile(r"^[0-9a-f]{40}$")


class PackageError(ValueError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def expected_layout(csv_data: bytes, sdk_config: bytes) -> None:
    """Assert the reviewed 16MB dual-OTA partition layout; never regenerate it."""
    import csv
    import io
    text = csv_data.decode("utf-8")
    rows = {}
    for row in csv.reader(io.StringIO(text)):
        if not row or row[0].strip().startswith("#"):
            continue
        if len(row) < 5:
            raise PackageError("incomplete partition CSV")
        name, kind, subtype, offset, size = (v.strip() for v in row[:5])
        if name in rows:
            raise PackageError("duplicate partition")
        rows[name] = (kind, subtype, int(offset, 0), int(size, 0))
    for name, expected in {
        "nvs": ("data", "nvs", 0x9000, 0x6000),
        "otadata": ("data", "ota", 0xF000, 0x2000),
        "ota_0": ("app", "ota_0", 0x20000, 0x600000),
        "ota_1": ("app", "ota_1", 0x620000, 0x600000),
    }.items():
        if rows.get(name) != expected:
            raise PackageError("reviewed partition layout changed: " + name)
    config = sdk_config.decode("utf-8")
    for required in ('CONFIG_IDF_TARGET="esp32s3"',
                     "CONFIG_ESPT OOLPY_FLASHSIZE_16MB=y".replace(" ", ""),
                     'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_ota.csv"',
                     "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y"):
        if required not in config:
            raise PackageError("SDK configuration changed: " + required)


def offset_map(manifest: bytes) -> dict[int, str]:
    try:
        spec = json.loads(manifest)
        entries = spec["flash_files"]
        if not isinstance(entries, dict):
            raise PackageError("flasher_args.json flash_files is not an object")
        result = {int(k, 0): v for k, v in entries.items()}
    except (KeyError, ValueError, TypeError, json.JSONDecodeError) as exc:
        raise PackageError("invalid flasher_args.json flash_files: " + str(exc)) from exc
    if result != IMAGE_PATHS:
        raise PackageError(f"four flash images/offsets must match reviewed ESP32-S3 layout: {result!r}")
    return result


def validate_flash_args(raw: bytes) -> None:
    # flash_args is the original generated esptool @file; it must independently
    # agree with flasher_args.json, rather than being created by this packager.
    tokens = shlex.split(raw.decode("utf-8"), comments=False, posix=True)
    pairs = {}
    for i in range(len(tokens) - 1):
        if re.fullmatch(r"0[xX][0-9a-fA-F]+", tokens[i]):
            offset = int(tokens[i], 16)
            if offset in pairs:
                raise PackageError("repeated offset in original flash_args")
            pairs[offset] = tokens[i + 1]
    if pairs != IMAGE_PATHS:
        raise PackageError("actual generated flash_args disagrees with manifest: " + repr(pairs))


def safe_name(name: str) -> None:
    p = PurePosixPath(name)
    if not name or name.startswith("/") or "\\" in name or ".." in p.parts or str(p) != name:
        raise PackageError("unsafe archive image name: " + name)


def validate_files(files: dict[str, bytes], source_sha: str) -> dict:
    if set(files) != REQUIRED_ARCHIVE:
        raise PackageError(f"ZIP content mismatch missing={REQUIRED_ARCHIVE - set(files)} extras={set(files) - REQUIRED_ARCHIVE}")
    for name, data in files.items():
        safe_name(name)
        if not data:
            raise PackageError("missing/empty archive payload: " + name)
    offset_map(files["flasher_args.json"])
    validate_flash_args(files["flash_args"])
    expected_layout(files["partitions_ota.csv"], files["sdkconfig.defaults"])
    if len(files["ota_data_initial.bin"]) != 0x2000:
        raise PackageError("real ota_data_initial.bin must fill reviewed 0x2000 otadata partition")
    for off, image in IMAGE_PATHS.items():
        subsequent = min((v for v in (0x8000, 0x9000, 0xF000, 0x11000, 0x20000,
                                     0x620000) if v > off), default=0x1000000)
        if len(files[image]) > subsequent - off:
            raise PackageError(f"flash image crosses partition boundary: {image}")
    try:
        meta = json.loads(files[PROVENANCE])
    except (ValueError, UnicodeDecodeError) as exc:
        raise PackageError("bad embedded provenance") from exc
    if meta.get("source_sha") != source_sha or meta.get("idf_version") != "v5.5.5":
        raise PackageError("source SHA or ESP-IDF pin mismatch")
    if meta.get("chip") != "esp32s3" or meta.get("bench_only") is not True:
        raise PackageError("missing ESP32-S3 bench-only identity")
    if meta.get("offsets") != {f"0x{k:x}": v for k, v in IMAGE_PATHS.items()}:
        raise PackageError("provenance offsets mismatch")
    checksums = {n: sha256(files[n]) for n in REQUIRED_ARCHIVE - {SHA_FILE, PROVENANCE}}
    if meta.get("sha256") != checksums:
        raise PackageError("embedded image/manifest SHA256 mismatch")
    checksums[PROVENANCE] = sha256(files[PROVENANCE])
    actual_lines = [f"{checksums[name]}  {name}" for name in sorted(checksums)]
    if files[SHA_FILE].decode("ascii") != "\n".join(actual_lines) + "\n":
        raise PackageError("SHA256SUMS mismatch")
    return meta


def build(build_dir: Path, project_dir: Path, output: Path, sha: str) -> None:
    if not SHA_RE.fullmatch(sha):
        raise PackageError("exact source SHA required")
    if output.exists():
        raise PackageError("refuse to overwrite existing firmware ZIP: " + str(output))
    source = {}
    for name in IMAGE_PATHS.values():
        source[name] = (build_dir / name).read_bytes()  # no substitutes
    for name in ("flasher_args.json", "flash_args"):
        source[name] = (build_dir / name).read_bytes()
    for name in ("partitions_ota.csv", "sdkconfig.defaults"):
        source[name] = (project_dir / name).read_bytes()
    if any(not v for v in source.values()):
        raise PackageError("empty binary/manifest")
    offset_map(source["flasher_args.json"])
    validate_flash_args(source["flash_args"])
    expected_layout(source["partitions_ota.csv"], source["sdkconfig.defaults"])
    if len(source["ota_data_initial.bin"]) != 0x2000:
        raise PackageError("real OTA initialization data size mismatch")
    provenance = {
        "schema": 1, "source_sha": sha, "idf_version": "v5.5.5",
        "chip": "esp32s3", "bench_only": True, "flashed": False,
        "physical_release_certified": False, "automatic_failover_verified": False,
        "offsets": {f"0x{k:x}": v for k, v in IMAGE_PATHS.items()},
        "sha256": {name: sha256(data) for name, data in source.items()},
    }
    source[PROVENANCE] = (json.dumps(provenance, sort_keys=True, indent=2) + "\n").encode()
    checksums = {name: sha256(data) for name, data in source.items() if name != SHA_FILE}
    source[SHA_FILE] = ("\n".join(f"{checksums[n]}  {n}" for n in sorted(checksums)) + "\n").encode()
    validate_files(source, sha)
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED,
                             compresslevel=9) as archive:
            for name in sorted(source):
                archive.writestr(name, source[name])
        verify(output, sha)
        digest = sha256(output.read_bytes())
        output.with_suffix(output.suffix + ".sha256").write_text(
            f"{digest}  {output.name}\n", encoding="ascii")
        print(f"PASS flash-complete bench ZIP {output.name} SHA256 {digest}")
        for offset, name in IMAGE_PATHS.items():
            print(f"FLASH 0x{offset:x} {name} SHA256 {sha256(source[name])}")
    except Exception:
        output.unlink(missing_ok=True)
        output.with_suffix(output.suffix + ".sha256").unlink(missing_ok=True)
        raise


def verify(zip_path: Path, source_sha: str, checksum: Path | None = None) -> None:
    if not SHA_RE.fullmatch(source_sha):
        raise PackageError("expected exact source SHA required")
    if not zip_path.is_file():
        raise PackageError("ZIP missing: " + str(zip_path))
    with zipfile.ZipFile(zip_path, "r") as archive:
        infos = archive.infolist()
        if len(infos) != len(REQUIRED_ARCHIVE):
            raise PackageError("ZIP has missing/extra/duplicated entries")
        if any(info.is_dir() or info.file_size <= 0 for info in infos):
            raise PackageError("ZIP contains directory/empty payload")
        files = {}
        for info in infos:
            safe_name(info.filename)
            if info.filename in files:
                raise PackageError("duplicate archive path")
            files[info.filename] = archive.read(info)
        if archive.testzip() is not None:
            raise PackageError("invalid ZIP CRC")
    validate_files(files, source_sha)
    digest = sha256(zip_path.read_bytes())
    if checksum:
        expected = f"{digest}  {zip_path.name}\n"
        if checksum.read_text(encoding="ascii") != expected:
            raise PackageError("published ZIP checksum mismatch")
    print(f"PASS: extracted ZIP all four real ESP-IDF flash images, offsets, lengths, CRC and SHA256; SHA={source_sha}")
    print(f"ZIP_SHA256={digest}")
    for offset, name in IMAGE_PATHS.items():
        print(f"OFFSET 0x{offset:x} FILE {name} SHA256={sha256(files[name])}")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build")
    b.add_argument("--build-dir", type=Path, required=True)
    b.add_argument("--project-dir", type=Path, required=True)
    b.add_argument("--output", type=Path, required=True)
    b.add_argument("--source-sha", required=True)
    v = sub.add_parser("verify")
    v.add_argument("--zip", type=Path, required=True)
    v.add_argument("--sha256-file", type=Path)
    v.add_argument("--source-sha", required=True)
    args = p.parse_args()
    if args.command == "build":
        build(args.build_dir, args.project_dir, args.output, args.source_sha)
    else:
        verify(args.zip, args.source_sha, args.sha256_file)


if __name__ == "__main__":
    main()
