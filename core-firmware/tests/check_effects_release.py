#!/usr/bin/env python3
"""Audit implementation identities separately from release parity certification.

Use --audit in ordinary CI.  --release intentionally fails until all borrowed
modes have direct source adapters and physical/golden-frame evidence is present.
Never interpret a successful compile or an approximated catalog as parity.
"""
import argparse
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--release", action="store_true")
parser.add_argument("--audit", action="store_true", help="Report and guard the effect inventory without claiming parity")
args = parser.parse_args()

compat = (root / "main/wled_1d.c").read_text()
ports = (root / "main/wled_fx_compat.c").read_text()
original = (root / "main/ot_auto_effects.c").read_text()
source_catalog = compat.split("static const ot_wled_1d_meta_t catalog[]={", 1)[1].split("};", 1)[0]
source_port_entries = ports.split("static const ot_wled_fx_metadata_t supported[] = {", 1)[1].split("};", 1)[0]
original_ids = original.split("static const uint16_t ids[] = {", 1)[1].split("};", 1)[0]

borrowed = re.findall(r'^\s*\{(\d+),', source_catalog, flags=re.M)
ported = re.findall(r'^\s*\{OT_WLED_([A-Z0-9_]+),', source_port_entries, flags=re.M)
approved = re.findall(r'OT_FX_[A-Z0-9_]+', original_ids)
assert len(borrowed) == len(set(borrowed)) == 120, "Borrowed effect IDs changed or duplicated"
assert len(ported) == len(set(ported)) and len(ported) <= len(borrowed), "Source adapter catalog invalid"
assert len(approved) == len(set(approved)) == 19, "An approved original was removed or duplicated"
assert "OT_FX_EVENT_HORIZON" in approved and "OT_FX_SINGULARITY_SHUTDOWN" in approved
assert (root / "tests/test_render_quality.c").exists(), "Fade/strobe host regression absent"
assert (root / "tests/test_proprietary_effects.c").exists(), "Original effect regression absent"

approximation_count = len(borrowed)-len(ported)
print(f"Effects parity AUDIT: {len(borrowed)} borrowed IDs; {len(ported)} source-derived; "
      f"{approximation_count} still approximated; {len(approved)} registered original one-shot/concept IDs, "
      "plus Embers.")
if args.release:
    if approximation_count:
        parser.error(f"Release blocked: {approximation_count} borrowed modes still lack direct source ports")
    evidence = root / "release-evidence/effects-release-evidence.json"
    if not evidence.is_file():
        parser.error("Release blocked: golden-frame and physical evidence file missing")
    e = json.loads(evidence.read_text())
    required = ("golden_frames_all", "palette_options_parity",
                "low_brightness_motion", "strobe_scope_timing",
                "physical_input_priority", "stop_true_off",
                "two_output_bench", "status_led", "ble_session")
    failed = [k for k in required if e.get(k) is not True]
    if failed:
        parser.error("Release blocked: missing signed-off evidence: "+", ".join(failed))
    print("Effects parity RELEASE gate passed.")
else:
    print("Audit only: passing CI is not a full-effects release certificate.")
