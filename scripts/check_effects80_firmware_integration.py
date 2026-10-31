#!/usr/bin/env python3
"""Fail-closed CORE integration provenance for Effects #80 (software only).

This verifies exact source integration, not actual Android display parity,
vehicle optical behavior, or physical failover.
"""
import argparse
from pathlib import Path
import subprocess

EFFECTS_HEAD = "7c0fcc450a7619a57603aabf833dea855a18bcd8"
CORE_BASE = "05509c894ad6dbd48566c3784466aa005fb61263"
ANDROID_SOURCE = "3f7d7dc0b3d3248bb5b04ec6a2eb4a425adb6e13"
SHARED = (
    "core_logic.h", "effects_1d.c", "effects_1d.h",
    "ot_auto_effects.c", "ot_auto_effects.h", "palette_1d.c",
    "palette_1d.h", "particle_system_1d.c", "particle_system_1d.h",
    "wled_1d.c", "wled_1d.h", "wled_fx_compat.c", "wled_fx_compat.h",
)

def git(*args, cwd=None):
    return subprocess.check_output(("git", *args), cwd=cwd, text=True).strip()

def checked_equal(a, b, name):
    assert a == b, f"{name}: source SHA drift {a} != {b}"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--android-root", type=Path,
                        help="Read-only checkout pinned to Android PR #79")
    args = parser.parse_args()
    head = git("rev-parse", "HEAD")
    git("cat-file", "-e", f"{CORE_BASE}^{{commit}}")
    git("cat-file", "-e", f"{EFFECTS_HEAD}^{{commit}}")
    assert subprocess.run(
        ("git", "merge-base", "--is-ancestor", EFFECTS_HEAD, head),
        check=False).returncode == 0, "Effects #80 is not in CORE ancestry"

    # No firmware code or BLE/NVS/driver modifications after the approved
    # Effects source. All source changes in this PR must be audit and CI.
    paths = git("ls-tree", "-r", "--name-only", "HEAD", "core-firmware/main").splitlines()
    assert paths, "Missing CORE runtime source tree"
    additions = {
        "core-firmware/main/ot_failover_essential.c",
        "core-firmware/main/ot_failover_essential.h",
    }
    for path in paths:
        if path in additions:
            assert git("cat-file", "-t", f"HEAD:{path}") == "blob"
            continue
        if path == "core-firmware/main/CMakeLists.txt":
            original = git("show", f"{EFFECTS_HEAD}:{path}")
            anchor = '"ot_fps_pacer.c" "ot_frame_snapshot.c"'
            assert original.count(anchor) == 1
            expected = original.replace(anchor, anchor + ' "ot_failover_essential.c"')
            checked_equal(git("show", f"HEAD:{path}"), expected, path)
            continue
        checked_equal(git("rev-parse", f"HEAD:{path}"),
                      git("rev-parse", f"{EFFECTS_HEAD}:{path}"), path)
    assert additions.issubset(set(paths))

    # Both firmware and Effects source retain the same release-evidence gate.
    gate = ".github/workflows/verify-active-sources.yml"
    for source in (EFFECTS_HEAD, CORE_BASE):
        checked_equal(git("rev-parse", f"HEAD:{gate}"),
                      git("rev-parse", f"{source}:{gate}"), "physical release gate")
    assert git("cat-file", "-t", f"HEAD:core-firmware/tests/test_proprietary_loop_continuity.c") == "blob"
    assert git("cat-file", "-t", f"HEAD:core-firmware/tests/test_nimble_long_read_contract.py") == "blob"

    allowed = {
        "scripts/check_effects80_firmware_integration.py",
        "docs/CORE-EFFECTS80-INTEGRATION-2026-10-31.md",
        ".github/workflows/verify-core-effects80-integration.yml",
        "core-firmware/main/ot_failover_essential.c",
        "core-firmware/main/ot_failover_essential.h",
        "core-firmware/main/CMakeLists.txt",
        "core-firmware/tests/run-host-tests.sh",
        "core-firmware/tests/test_failover_essential.c",
        "core-firmware/tests/test_failover_runtime_isolation.py",
        "core-firmware/tests/test_failover_release_evidence.py",
        "docs/FAILOVER-SOFTWARE-BOUNDARY-2026-10-31.md",
        "docs/FAILOVER-22-PHYSICAL-ACCEPTANCE.csv",
        ".github/workflows/verify-core-failover-policy.yml",
    }
    changed = set(git("diff", "--name-only", EFFECTS_HEAD, head).splitlines())
    assert changed <= allowed, f"Out-of-scope integration changes: {sorted(changed - allowed)}"
    assert not any(p.startswith("core-firmware/main/") and p not in additions | {"core-firmware/main/CMakeLists.txt"} for p in changed)
    print(f"PASS: approved Effects #80 runtime unchanged, except isolated backup module + CMake registration")
    print(f"PASS: #69 ancestry, 140-effect continuity test present, full release gate unchanged")

    if args.android_root:
        assert git("rev-parse", "HEAD", cwd=args.android_root) == ANDROID_SOURCE, (
            "Android checkout must pin approved PR #79 exact source SHA")
        for filename in SHARED:
            core = git("rev-parse", f"HEAD:core-firmware/main/{filename}")
            app = git("rev-parse", f"HEAD:android/app/src/main/cpp/core/{filename}",
                      cwd=args.android_root)
            checked_equal(core, app, f"CORE/Android {filename}")
        print(f"PASS: {len(SHARED)}/13 CORE/Android JNI renderer source files byte-identical")
        print("NOTE: source identity != Android JNI golden-frame test or device playback")
    print("BENCH SOFTWARE ONLY: no physical GPIO, failover, radio latency or release certification")

if __name__ == "__main__":
    main()
