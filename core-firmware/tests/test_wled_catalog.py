#!/usr/bin/env python3
"""Validate pinned WLED metadata mapping; do not imply the full library is implemented."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
catalog = json.loads((root / "third_party/wled/effects-catalog.json").read_text())
effects = catalog["effects"]
assert catalog["source"]["commit"] == "20e7a717664295eefc6ad77972cee1888d2da072"
assert catalog["source"]["license"] == "EUPL-1.2-or-later"
assert catalog["upstreamMetadataOccurrences"] == 218
assert len(effects) == catalog["distinctResolvedIds"] == 201
assert len(catalog["unresolvedMetadata"]) == catalog["unmappedMetadataSymbols"] == 16
assert len({item["id"] for item in effects}) == len(effects)
assert effects == sorted(effects, key=lambda i: (i["name"].casefold(), i["id"]))
host_ids = {x["id"] for x in effects if x["coreStatus"] == "host-tested-compatibility-module-only"}
assert host_ids == set(catalog["coreCompatibilityModuleIds"]) == {0, 1, 23}
for effect_id, name in [(0, "Solid"), (1, "Blink"), (23, "Strobe")]:
    effect = next(e for e in effects if e["id"] == effect_id)
    assert effect["name"] == name
assert all(item["coreStatus"] in ("host-tested-compatibility-module-only", "not-ported") for item in effects)
assert all(x["metadataSymbol"].startswith("_data_FX_MODE_") for x in catalog["unresolvedMetadata"])
print("Pinned WLED catalog metadata ID uniqueness, provenance and unsupported labels passed.")
