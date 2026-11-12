#!/usr/bin/env python3
"""Verify integrated source identities; never substitute source checks for device proof."""
import json
from pathlib import Path
import subprocess
def git(*a): return subprocess.check_output(["git",*a],text=True).strip()
m=json.loads(Path("docs/SOFTWARE-COMPATIBILITY-MATRIX.json").read_text())
integrated=m.get("integrated_trees",{})
for area,key in (("android","android"),("core-firmware","firmware")):
    expected=integrated.get(area,m["sources"][key]["tree"])
    actual=git("rev-parse","HEAD:"+area)
    assert actual==expected,(area,actual,expected)
gate=".github/workflows/verify-active-sources.yml"
workflow_pin=m.get("integrated_workflow_pin")
if workflow_pin:
    # This is a strict reviewed blob pin, not a waiver. The old Firmware #84
    # workflow cannot equal Android's independently updated Firmware #127
    # checkout, but its old source/renderer provenance remains immutable.
    current_blob=git("rev-parse","HEAD:"+gate)
    assert current_blob==workflow_pin["blob_sha"],(current_blob,workflow_pin["blob_sha"])
    authorized_firmware=workflow_pin["firmware127_sha"]
    assert len(authorized_firmware)==40 and all(ch in "0123456789abcdef" for ch in authorized_firmware)
    assert ("ref: "+authorized_firmware) in Path(gate).read_text()
    assert ("PIN = \""+authorized_firmware+"\"") in Path("android/qa/check_pinned_firmware127_contract.py").read_text()
else:
    assert git("rev-parse","HEAD:"+gate)==git("rev-parse",m["sources"]["firmware"]["sha"]+":"+gate)
names=("core_logic.h","effects_1d.c","effects_1d.h","ot_auto_effects.c","ot_auto_effects.h","palette_1d.c","palette_1d.h","particle_system_1d.c","particle_system_1d.h","wled_1d.c","wled_1d.h","wled_fx_compat.c","wled_fx_compat.h")
renderer_pin=m.get("integrated_renderer_pin")
if renderer_pin:
    # The Android branch retains its separately pinned historical CORE tree.
    # New JNI renderer bytes must instead equal the *approved* Firmware #127
    # source exactly. Keep the only platform header difference explicit.
    approved=renderer_pin["firmware127_sha"]
    assert workflow_pin and approved==workflow_pin["firmware127_sha"]
    for name in names:
        actual=git("rev-parse","HEAD:android/app/src/main/cpp/core/"+name)
        if name=="core_logic.h":
            assert actual==renderer_pin["android_core_logic_blob"],(name,actual)
            assert actual==git("rev-parse",renderer_pin["base_effects_commit"]+":core-firmware/main/"+name)
            firmware_header=git("rev-parse",approved+":core-firmware/main/"+name)
            assert firmware_header==renderer_pin["firmware_core_logic_blob"],(name,firmware_header)
        else:
            expected=git("rev-parse",approved+":core-firmware/main/"+name)
            assert actual==expected,(name,actual,expected)
else:
    for name in names:
        assert git("rev-parse","HEAD:android/app/src/main/cpp/core/"+name)==git("rev-parse","HEAD:core-firmware/main/"+name),name
assert m["release_approval"] is False and m["hardware_failover_certified"] is False
correction=m["continuous_correction"]
changed=set(git("diff","--name-only",correction["base_sha"],m["issue93_correction"]["base_sha"],"--","android","core-firmware").splitlines())
assert changed==set(correction["allowed_runtime_changes"]),changed
print("PASS scoped continuous playback and owner-approved UI changes; all 13 renderers/palettes and physical release gate unchanged")

issue=m["issue93_correction"]
p0=m.get("owner_mandated_p0_addendum")
historical_end=p0["base_sha"] if p0 else "HEAD"
changed=set(git("diff","--name-only",issue["base_sha"],historical_end,"--","android","core-firmware").splitlines())
# Issue93's original reviewed list is an allowlist, not a requirement to
# change every listed file. Its historical window ends at the owner P0 base;
# owner P0 has a separate, exact changed-file lock below.
assert changed.issubset(set(issue["allowed_android_changes"])),changed-set(issue["allowed_android_changes"])
assert len(changed)==issue.get("historical_changed_file_count",len(changed)),len(changed)

if p0:
    p0_changed=set(git("diff","--name-only",p0["base_sha"],"HEAD","--","android","core-firmware").splitlines())
    assert p0_changed==set(p0["allowed_changes"]),(p0_changed,set(p0["allowed_changes"]))
    assert m["release_approval"] is False
    assert "100/100" in p0["physical_release_gate"] and "BLOCKED" in p0["physical_release_gate"]
print("PASS scoped Issue93 integration plus exact owner-mandated Android P0 allowlist; physical release remains blocked")
