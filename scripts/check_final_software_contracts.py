#!/usr/bin/env python3
"""Fail-closed provenance check for OpenTail's combined SOFTWARE candidate.

Validates exact source blobs and immutable SHAs. NOT an electrical/hardware
release certificate. Does not run hardware tests or permit flashing.
"""
from pathlib import Path
import subprocess

PINS = {
    "firmware": "adcdff0b2f43061bc3e222d1d77de0a34b095c0a",
    "triggers": "e44a98fcaa51ecbf5e9c15bf85b8d129adb4e1a9",
    "effects": "1767b144e193ed6978816899ebb5eba237ee6e2d",
    "android": "73827e55a503670ac21c8554e4d9f14de27749cf",
}
BLOBS = {
    "android": "88ade4b946861c9e74ce2b0a780f9fbe84792159",
    "web-studio": "07dc933321e423b4effc2eee2b77bfedbaab0091",
    "core-firmware/tests/test_trigger_runtime_contract.py": "c2a81a83ef5f7640cfde85248e8721bb95be7e4a",
    "core-firmware/tests/run-host-tests.sh": "b1251aa18806e4ffc1fa7fe5dc4111b7df1cd331",
    "core-firmware/tests/test_effects_nvs_recovery_isolation.c": "2c65d91d4591f9a249fe292ca3fc03215a802abf",
    "core-firmware/tests/test_effects_oem_safety_matrix.c": "af913c9a42167e9b462d1cea72e898fb190e9446",
    "core-firmware/tests/test_effects_render_safety_contract.py": "e04a9c8fd39c4ff4efc7db7d20df52cd28bb3574",
}

def git(*args):
    return subprocess.check_output(("git", *args), text=True).strip()

assert git("rev-parse", "--is-inside-work-tree") == "true"
head = git("rev-parse", "HEAD")
for role,sha in PINS.items():
    assert git("cat-file", "-t", sha) == "commit", f"{role} source SHA not available"
    print(f"{role}: immutable source {sha}")
for path,expected in BLOBS.items():
    actual=git("rev-parse", f"HEAD:{path}")
    assert actual == expected, f"Source changed: {path} {actual} != {expected}"
    print(f"Verified unchanged source blob: {path}")
assert subprocess.run(
    ["git","diff","--quiet",PINS["firmware"],head,"--","core-firmware/main"],
).returncode == 0, "Approved firmware runtime changed after Firmware #49"

# The integrated candidate must not smuggle any unknown changes (especially
# physical release evidence, firmware runtime, a different Studio or CI bypass).
approved={
    ".github/workflows/verify-active-sources.yml",
    ".github/workflows/verify-opentail-software-freeze.yml",
    "scripts/check_final_software_contracts.py",
    "docs/FINAL-SOFTWARE-CANDIDATE-2026-10-30.md",
    "core-firmware/tests/run-host-tests.sh",
    "core-firmware/tests/test_trigger_runtime_contract.py",
    "core-firmware/tests/test_effects_nvs_recovery_isolation.c",
    "core-firmware/tests/test_effects_oem_safety_matrix.c",
    "core-firmware/tests/test_effects_render_safety_contract.py",
}
changes=git("diff","--name-only",PINS["firmware"],head).splitlines()
unexpected=[p for p in changes if not (p.startswith("android/") or p.startswith("web-studio/") or p in approved)]
assert not unexpected, f"Unexpected integration-scope changes: {unexpected}"

# The mandatory release gate and physical evidence rules must not be rewritten.
baseline=subprocess.check_output(
    ["git","show",f"{PINS['firmware']}:.github/workflows/verify-active-sources.yml"],
    text=True,
)
current=Path(".github/workflows/verify-active-sources.yml").read_text()
def release_gate(text):
    return text.split("  release-evidence-gate:",1)[1].split("  android-debug-apk:",1)[0]
assert release_gate(baseline)==release_gate(current), "Release evidence gate changed"
assert "scripts/check_regressions.py" in current
assert not (Path("android/app/src/main/assets/opentail.htm")).exists()
assert "ComposeMainActivity" in Path("android/app/src/main/AndroidManifest.xml").read_text()
print(f"PASS: four-owner exact source contract and unchanged CORE runtime at {head}")
print("SOFTWARE SOURCE FREEZE ONLY; 22 physical checks, NVS real brownout, failover and flashing remain blocked")
