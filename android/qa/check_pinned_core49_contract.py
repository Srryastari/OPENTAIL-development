#!/usr/bin/env python3
"""Read-only Android ABI gate against immutable CORE #49 / Trigger #48 / Effects #50.
Source contract evidence only; not ESP-IDF, GPIO, BLE radio or optical proof.
"""
from pathlib import Path
import subprocess
import re

PINS = {
    "pinned-core49": "adcdff0b2f43061bc3e222d1d77de0a34b095c0a",
    "pinned-trigger48": "e44a98fcaa51ecbf5e9c15bf85b8d129adb4e1a9",
    "pinned-effects50": "1767b144e193ed6978816899ebb5eba237ee6e2d",
}

def require_text(path, *tokens):
    text = Path(path).read_text()
    for token in tokens:
        assert token in text, f"{path}: pinned protocol token absent: {token}"
    return text

for folder, sha in PINS.items():
    actual = subprocess.check_output(
        ["git", "-C", folder, "rev-parse", "HEAD"], text=True
    ).strip()
    assert actual == sha, f"{folder} changed: {actual}, expected {sha}"

ble = require_text("pinned-core49/core-firmware/main/ble_status.c",
    "ot_palette_cmd_uuid=BLE_UUID128_INIT(0x2d",
    "ot_palette_rsp_uuid=BLE_UUID128_INIT(0x2e",
    "ot_palette_caps_uuid=BLE_UUID128_INIT(0x2f",
    "OT_PALETTE_WIRE_VERSION,OT_PALETTE_SLOTS",
    "OT_PALETTE_STOPS_MAX,OT_PALETTE_CHUNK_MAX,200u,73u",
    "ot_palette_ble_process(", "ot_palette_ble_session_reset();",
    "ot_ble_trigger_get(", "ot_ble_trigger_save(",
    "ot_ble_config_get(", "ot_ble_config_save(",
    "OT_CFG_WIRE_BYTES", "OT_TM_BLE_READ_BYTES",
    "OT_TM_BLE_WRITE_BYTES",
)
assert "0x2c,0xaf" in ble, "Trigger mapping 0x2c must coexist with palette 0x2d/e/f"
core = require_text("pinned-core49/core-firmware/main/opentail_core.c",
    "om_active_revision", "tm_active_revision", "config_apply_pending",
    "active_config_revisions", "config_revisions",
    "active_config_revision", "expected_config_revision",
    "om_revision[index]==UINT16_MAX", "tm_revision==UINT16_MAX",
    "*out=om_outputs[index];*revision=om_revision[index];",
    "*out=tm_profile;", "*revision=tm_revision;",
    "*persistent=tm_persistent && !tm_deferred.pending;",
    "config_recovery_fault", "input_edge_epoch",
)
assert "tm_revision++" in core and "om_revision[index]++" in core
assert '"saved_on_device"' in core
revision = require_text("pinned-core49/core-firmware/main/config_journal.c",
    "candidate_revision", "previous_revision",
    "ot_tm_journal_build_revisions", "ot_om_journal_build_revisions",
)
require_text("pinned-core49/core-firmware/main/palette_ble_v3.c",
    "ot_palette_ble_process(", "case 1:", "case 2:",
    "case 3:", "case 4:", "case 5:", "case 6:", "case 7:",
)
require_text("pinned-core49/core-firmware/main/ble_fx_command.c",
    "OT_BLE_FX_WIRE_BYTES", "p[0]!=1u",
    "ot_effects_1d_metadata(id,&meta)", "ot_palette_1d_supported(id,p[15])",
)
require_text("pinned-trigger48/core-firmware/tests/test_trigger_runtime_contract.py",
    "tm_revision", "om_revision", "config_apply_pending", "input_edge_epoch",
)
require_text("pinned-effects50/core-firmware/tests/test_effects_render_safety_contract.py",
    "ot_palette_runtime_snapshot(", "ot_wled_palette_global_tick(",
    "gamma_enabled", "ot_palette_1d_supported(",
)
require_text("pinned-effects50/core-firmware/tests/test_effects_oem_safety_matrix.c",
    "256", "gamma",
)
# Prove the entire original 120-mode catalog matches firmware exact IDs and
# names. CORE alone decides which modes can be deployed via live capabilities.
core_modes = [
    (int(m.group(1)), m.group(2))
    for m in re.finditer(
        r'^\s*\{(\d+),"[^"]+","([^"]+)",\d+\},',
        Path("pinned-core49/core-firmware/main/wled_1d.c").read_text(), re.M
    )
]
android_modes = [
    (int(m.group(1)), m.group(2))
    for m in re.finditer(
        r'new Effect\((\d+),\s*"([^"]+)"\)',
        Path("android/app/src/main/java/com/opentail/android/NativeEffectCatalog.java").read_text()
    )
    if int(m.group(1)) < 32768
]
assert len(core_modes) == 120, f"CORE #49 effects catalog changed: {len(core_modes)}"
assert len(android_modes) == 120, f"Android effects catalog changed: {len(android_modes)}"
assert core_modes == android_modes, "Android 120 effect ID/name catalog diverges from CORE #49"
require_text("android/app/src/main/java/com/opentail/android/ComposeMainActivity.kt",
    "supportedIds.value", "effectId !in supportedIds.value",
    "NativePaletteEngine.coreSupports(effectId, paletteId)",
    "ExactLedFrame.fromProjectModel(project)",
)

# Android has one readback protocol implementation, not a second renderer.
android = require_text("android/app/src/main/java/com/opentail/android/BleDiscovery.java",
    "PALETTE_CMD_UUID", "PALETTE_RSP_UUID", "PALETTE_CAP_UUID",
    "NativePaletteV3", "cfgSaveBase", "cfgRevision>cfgSaveBase+1",
    "CORE active output differs; may be pending trigger release",
    "CORE trigger revision exhausted; cannot save or wrap",
)
require_text("android/app/src/main/java/com/opentail/android/NativeTriggerMappingCodec.java",
    "baseRevision >= 1 && baseRevision < 65535",
    "revision == baseRevision + 1 && matches(expected);",
)
print("PASS: pinned CORE #49, Trigger #48, Effects #50 source-SHA/ABI read-only compatibility gate")

# Parameter controls must reproduce the pinned CORE flags, not assume every
# effect has three colors and universal motion/palette controls.
metadata_source = Path('pinned-core49/core-firmware/main/wled_fx_compat.c').read_text()
metadata_source = metadata_source.split('static const ot_wled_fx_metadata_t supported[]')[1].split('bool ot_wled_fx_metadata')[0]
metadata_rows = re.findall(
    r'\{\s*OT_\w+,\s*"([^"]+)",\s*"[^"]*",\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false)\s*\}', metadata_source)
assert len(metadata_rows) == 120
expected_flags = {row[0]: tuple(row[1:]) for row in metadata_rows}
auto_source = Path('pinned-core49/core-firmware/main/ot_auto_effects.c').read_text()
assert '*out=(ot_wled_fx_metadata_t){id,name,name,true,true,true,true,false};' in auto_source
for name in re.findall(r'case OT_FX_\w+:name="([^"]+)";', auto_source):
    expected_flags[name] = ('true', 'true', 'true', 'true', 'false')
expected_flags['Embers'] = ('true', 'false', 'true', 'true', 'false')
actual_flags = {
    row[0]: tuple(row[1:]) for row in re.findall(
        r'"([^"]+)" to EffectParameters\((true|false), (true|false), (true|false), (true|false), (true|false)\)',
        Path('android/app/src/main/java/com/opentail/android/NativeEffectParameters.kt').read_text())
}
assert actual_flags == expected_flags, 'Android effect parameter flags diverge from pinned CORE #49'
print('PASS: all 140 Android effect parameter definitions match pinned CORE metadata')
