#!/usr/bin/env python3
"""Verify integrated source identities; never substitute source checks for device proof."""
import json
from pathlib import Path
import subprocess
def git(*a): return subprocess.check_output(["git",*a],text=True).strip()
m=json.loads(Path("docs/SOFTWARE-COMPATIBILITY-MATRIX.json").read_text())
for area,key in (("android","android"),("core-firmware","firmware")):
    expected=m["sources"][key]["tree"]
    actual=git("rev-parse","HEAD:"+area)
    assert actual==expected,(area,actual,expected)
gate=".github/workflows/verify-active-sources.yml"
assert git("rev-parse","HEAD:"+gate)==git("rev-parse",m["sources"]["firmware"]["sha"]+":"+gate)
names=("core_logic.h","effects_1d.c","effects_1d.h","ot_auto_effects.c","ot_auto_effects.h","palette_1d.c","palette_1d.h","particle_system_1d.c","particle_system_1d.h","wled_1d.c","wled_1d.h","wled_fx_compat.c","wled_fx_compat.h")
for name in names:
    assert git("rev-parse","HEAD:android/app/src/main/cpp/core/"+name)==git("rev-parse","HEAD:core-firmware/main/"+name),name
assert m["release_approval"] is False and m["hardware_failover_certified"] is False
print("PASS: exact Android #87 and CORE #84 trees; locked UI/palettes; 13 renderer files; physical gate unchanged")
