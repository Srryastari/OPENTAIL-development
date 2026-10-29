#!/usr/bin/env python3
"""Keep REST and BLE firmware identity tied to the same release constant."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / "main"
version = (root / "ot_version.h").read_text()
core = (root / "opentail_core.c").read_text()
ble = (root / "ble_status.c").read_text()
assert re.search(r'#define\s+OT_FW_VERSION\s+"0\.1\.29-alpha"', version)
assert '#define OT_FW OT_FW_VERSION' in core
assert '"OpenTail CORE " OT_FW_VERSION " | BLE HMAC enrollment bench v2"' in ble
assert 'OpenTail CORE 0.1.25' not in ble
print("CORE REST/BLE firmware version identity regression passed")
