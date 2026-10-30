#!/usr/bin/env python3
"""Validate the client-facing effects manifest against authoritative CORE C sources.

This is a schema/source contract, not physical LED or complete RGB parity evidence.
"""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / "main"
manifest = json.loads((root / "metadata/effects-capabilities.v1.json").read_text())
def read(path):
    return (src / path).read_text()

header = read("wled_fx_compat.h")
renderer = read("wled_fx_compat.c")
palette = read("palette_1d.c")
original_h = read("ot_auto_effects.h")
original_c = read("ot_auto_effects.c")

assert manifest["schema"] == "opentail.effects-capabilities.v1"
assert manifest["source"]["borrowed_fx_commit"] == "20e7a717664295eefc6ad77972cee1888d2da072"
assert manifest["runtime_gate"]["require_installed_core_to_advertise_each_effect"] is True
assert manifest["runtime_gate"]["offline_catalog_does_not_authorize_physical_playback"] is True
assert manifest["palettes"]["dynamic_random_id_1"] == "unsupported"
assert manifest["palettes"]["user_custom_palettes"] == "unsupported"
assert manifest["palettes"]["rgb_preview_stops"] == "not_included_do_not_synthesize"
assert manifest["overlays"]["default_style"] == 0
assert manifest["overlays"]["compositing"] == "effect_then_overlay_then_physical_trigger"

ids = {k: int(v) for k, v in re.findall(r"\b(OT_WLED_[A-Z0-9_]+)\s*=\s*(\d+)", header)}
source_registry = renderer.split("static const ot_wled_fx_metadata_t supported[] = {", 1)[1].split("bool ot_wled_fx_metadata(", 1)[0]
registered = re.findall(r'\{\s*(OT_WLED_[A-Z0-9_]+)\s*,\s*"([^"]+)"', source_registry)
palette_part = palette.split("if(effect_id==", 1)[1].split("return palette>=2u&&palette<=72u;", 1)[0]
palette_modes = set(re.findall(r"\bOT_WLED_[A-Z0-9_]+\b", palette_part))
expected = {}
for symbol, name in registered:
    assert symbol in ids, f"Undeclared effect {symbol}"
    expected[ids[symbol]] = {
        "id": ids[symbol],
        "name": name,
        "origin": "source_derived_1d",
        "palette_ids": [0] + list(range(2, 73)) if symbol in palette_modes else [0],
        "requires_measured_audio": symbol == "OT_WLED_ROCKTAVES",
    }
assert len(expected) == 120 and len(palette_modes) == 16

native_ids = {k: int(v) for k, v in re.findall(r"\b(OT_FX_[A-Z0-9_]+)\s*=\s*(\d+)u", original_h)}
for symbol, name in re.findall(r'case\s+(OT_FX_[A-Z0-9_]+)\s*:\s*name="([^"]+)";break;', original_c):
    assert symbol in native_ids, f"Undeclared original {symbol}"
    ident = native_ids[symbol]
    assert ident not in expected
    expected[ident] = {
        "id": ident,
        "name": name,
        "origin": "opentail_original",
        "palette_ids": [0],
        "requires_measured_audio": False,
    }
expected[32768] = {
    "id": 32768,
    "name": "Embers",
    "origin": "opentail_original",
    "palette_ids": [0, 2, 3, 4],
    "requires_measured_audio": False,
}
assert len(expected) == 140 and len(native_ids) == 19
actual = manifest["effects"]
assert len(actual) == len(expected), "Client-visible catalog count disagrees with CORE"
assert [item["id"] for item in actual] == sorted(expected)
for item in actual:
    assert item == expected[item["id"]], f"Stale effect metadata/palette support: {item['id']}"
assert manifest["runtime_gate"]["requires_measured_audio_effect_ids"] == [185]
print("Effects capability manifest: 120 source-derived + 20 originals, 16 fixed-palette-enabled modes, fail-closed runtime gating PASS")
