#!/usr/bin/env python3
"""Issue113 scope lock against the owner's exact reviewed PR112 baseline."""
from pathlib import Path
import subprocess

BASE = "1a300767d985343d060faf344bff2da772dca88f"
ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {
    "core-firmware/main/CMakeLists.txt",
    "core-firmware/main/ble_status.c", # owner-requested BLE catalog optimization
    "core-firmware/main/Kconfig.projbuild",
    "core-firmware/main/opentail_core.c",
    "core-firmware/main/ot_ota_update.c",
    "core-firmware/main/ot_ota_update.h",
    "core-firmware/main/ot_recovery_policy.c",
    "core-firmware/main/ot_recovery_policy.h",
    "core-firmware/main/ot_watchdog.c",
    "core-firmware/main/ot_watchdog.h",
    "core-firmware/sdkconfig.defaults",
    "core-firmware/tests/run-host-tests.sh",
    "core-firmware/tests/test_failover_runtime_isolation.py",
    "core-firmware/tests/test_recovery_policy.c",
    "core-firmware/tests/test_watchdog_runtime.py",
    "core-firmware/tests/test_watchdog_integration.py",
    "core-firmware/tests/test_ota_deadline.py",
    "core-firmware/tests/test_nimble_long_read_contract.py",
    "core-firmware/tests/test_ota_boot_confirmation_contract.py",
    "scripts/check_integrated_candidate.py",
    "scripts/check_watchdog_candidate.py",
    ".github/workflows/verify-single-esp-watchdog.yml",
    "docs/SINGLE-ESP-WATCHDOG-113.md",
}

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

subprocess.run(["git", "merge-base", "--is-ancestor", BASE, "HEAD"], cwd=ROOT, check=True)
changed = set(git("diff", "--name-only", BASE, "--").splitlines())
assert changed <= ALLOWED, f"Outside Firmware/CORE watchdog scope: {changed - ALLOWED}"
# This lock preserves the complete Android tree, shared 140-effect renderer,
# tertiary BLE IDs, GPIO mapping, version identity, journals, partition layout,
# packaging/installer and every existing physical release gate by exact bytes.
assert not git("diff", "--name-only", BASE, "--", "android")
core = (ROOT / "core-firmware/main/opentail_core.c").read_text()
assert "ot_watchdog_init();" in core and "ot_watchdog_progress(OT_WATCH_RENDER);" in core
assert '"hardware_failover\\\":false' in core
print(f"PASS Issue113: scoped single-ESP recovery atop {BASE}; Android/renderers/protocol/version/22 physical gates unchanged")
