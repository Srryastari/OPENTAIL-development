#!/usr/bin/env python3
"""Effects #120 exact integrated renderer/watchdog compatibility certification.

Runs with cwd at an immutable checkout of Firmware #127, while this script and
the test-only workflow are sourced from Effects PR #120. No source is rewritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path.cwd()
INTEGRATED = "dff7fe070b134daaaad09a8fb989f76a6c5bda39"
EFFECTS = "601155a196a692792e60fd399505fdc676dc57d9"
IDENTITY = "97f4e2fd29df5aa9249292cee2649404fc98ed48"
NAMES = (
    "wled_fx_compat.c", "wled_fx_compat.h", "effects_1d.c", "effects_1d.h",
    "wled_1d.c", "wled_1d.h", "palette_1d.c", "palette_1d.h",
    "particle_system_1d.c", "particle_system_1d.h",
    "ot_auto_effects.c", "ot_auto_effects.h",
)
IDENTITY_FILES = {"effects_1d.c", "effects_1d.h"}

def git(root, *args):
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True
    ).strip()

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--effects-dir", type=Path, required=True)
    p.add_argument("--integration-sha", default=INTEGRATED)
    args = p.parse_args()
    effects = args.effects_dir.resolve()

    assert args.integration_sha == INTEGRATED, "Unreviewed firmware SHA"
    assert git(ROOT, "rev-parse", "HEAD") == INTEGRATED, "Wrong firmware checkout"
    assert git(effects, "rev-parse", "HEAD") == EFFECTS, "Wrong Effects checkout"
    assert git(ROOT, "status", "--porcelain") == "", "Source tree modified"
    manifest_path = ROOT / "android/app/src/main/cpp/core/SOURCE.json"
    manifest = json.loads(manifest_path.read_text())
    assert manifest["source_commit"] == EFFECTS
    assert manifest["seamless_loop_source_commit"] == EFFECTS
    assert manifest["color_slot_source_commit"] == EFFECTS
    assert manifest["core_identity_source_commit"] == IDENTITY
    assert manifest["tertiary_source_proven_modes"] == 13
    assert manifest["tertiary_transport_admitted_modes"] == 13

    for name in NAMES:
        core = ROOT / "core-firmware/main" / name
        jni = ROOT / "android/app/src/main/cpp/core" / name
        expected = (effects / "core-firmware/main" / name)
        assert core.read_bytes() == jni.read_bytes(), "CORE/JNI mismatch: " + name
        assert manifest["files"][name] == sha256(jni.read_bytes()), "Manifest SHA256 mismatch: " + name
        if name in IDENTITY_FILES:
            original_blob = git(ROOT, "rev-parse", f"{IDENTITY}:core-firmware/main/{name}")
            current_blob = git(ROOT, "hash-object", str(core))
            assert current_blob == original_blob, "Identity extension differs: " + name
        else:
            assert core.read_bytes() == expected.read_bytes(), "Approved Effects bytes differ: " + name
        print(f"PASS exact renderer bytes: {name} sha256={sha256(core.read_bytes())}")

    # Do not conflate old Effects-only adapter with new stable Panel/Zone APIs.
    c = (ROOT / "core-firmware/main/effects_1d.c").read_text()
    h = (ROOT / "core-firmware/main/effects_1d.h").read_text()
    assert "ot_effects_1d_find_prior_zone_state" in c and "ot_effects_1d_move_state" in c
    assert "ot_effects_1d_find_prior_zone_state" in h and "ot_effects_1d_move_state" in h

    wled = (ROOT / "core-firmware/main/wled_fx_compat.c").read_text()
    assert "tertiary_set" in wled and "popcorn_frame" in wled
    assert "const bool has_third=tertiary_set;" in wled
    core = (ROOT / "core-firmware/main/opentail_core.c").read_text()
    assert "ot_effects_1d_find_prior_zone_state" in core
    assert "ot_sw_idf_render_cycle" in core
    assert "led_strip_refresh_async_done" in core
    watchdog = (ROOT / "core-firmware/main/ot_single_watchdog_idf.c").read_text()
    assert "esp_task_wdt_reset_user" in watchdog
    runner = (ROOT / "core-firmware/tests/run-host-tests.sh").read_text()
    assert "test_effects_panel_zone_continuity.c" in runner
    for path in ("core-firmware/main/trigger_manager.c",
                 "core-firmware/tests/test_single_watchdog_timeline.c",
                 "core-firmware/tests/test_effect_capabilities_manifest.py"):
        assert (ROOT / path).is_file(), path

    print("PASS integrated #127 source integrity: exact CORE/JNI, manifest SHA256, "
          "approved Effects 110 renderer, Firmware 127 stable identities, "
          "13 tertiary modes, watchdog/zone/trigger test contracts. "
          "Live hardware safety remains NOT_RUN.")

if __name__ == "__main__":
    main()
