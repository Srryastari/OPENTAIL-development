#!/usr/bin/env python3
"""Explicit owner-authorized bench erase/reflash; never called by normal firmware.
Validate an extracted exact-build EasyFlash package before any serial operation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

IMAGES = {'0x0': 'bootloader/bootloader.bin',
          '0x8000': 'partition_table/partition-table.bin',
          '0xf000': 'ota_data_initial.bin', '0x20000': 'opentail_core_alpha.bin'}

def validate(package, expected_sha):
    package = Path(package).resolve()
    manifest = json.loads((package/'release_manifest.json').read_text())
    if len(expected_sha) != 40 or any(c not in '0123456789abcdef' for c in expected_sha):
        raise ValueError('Expected a full lowercase source SHA')
    if manifest.get('git_sha') != expected_sha or manifest.get('chip') != 'ESP32-S3 N16R8':
        raise ValueError('Source or board identity mismatch')
    if manifest.get('flasher_offsets') != IMAGES:
        raise ValueError('Unexpected flash layout')
    checks = {}
    for line in (package/'SHA256SUMS.txt').read_text().splitlines():
        digest, name = line.split('  ', 1)
        target = (package/name).resolve()
        if not target.is_relative_to(package) or name in checks:
            raise ValueError('Unsafe or duplicate checksum path')
        if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise ValueError('Checksum mismatch: '+name)
        checks[name] = digest
    for name in ['release_manifest.json', *IMAGES.values()]:
        if name not in checks or not (package/name).stat().st_size:
            raise ValueError('Missing verified file: '+name)
    return package

def flash(package, expected_sha, port, confirmation, run=subprocess.run):
    package = validate(package, expected_sha)
    if confirmation != 'ERASE-ALL':
        raise ValueError('Explicit ERASE-ALL confirmation required; nothing erased')
    base = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', port, '--baud', '115200']
    # Each nonzero exit stops the sequence. Never attempt flash after failed erase.
    run(base+['erase_flash'], check=True, cwd=package)
    args = base+['write_flash', '--flash_mode', 'dio', '--flash_freq', '80m', '--flash_size', '16MB']
    for offset, name in IMAGES.items():
        args += [offset, name]
    run(args, check=True, cwd=package)

if __name__ == '__main__':
    p = argparse.ArgumentParser(description='Erases ALL controller configurations and BLE ownership; bench only')
    p.add_argument('--package', required=True, type=Path)
    p.add_argument('--sha', required=True)
    p.add_argument('--port', default='COM9')
    p.add_argument('--confirm', choices=['ERASE-ALL'])
    a = p.parse_args()
    validate(a.package, a.sha)
    if a.confirm is None:
        print('PASS package validation only. No device accessed. To erase/reflash explicitly add --confirm ERASE-ALL.')
    else:
        flash(a.package, a.sha, a.port, a.confirm)
