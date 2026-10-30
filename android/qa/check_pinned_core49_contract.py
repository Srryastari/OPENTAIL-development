#!/usr/bin/env python3
"""Read-only Android ABI gate against immutable CORE #49 / Trigger #48 / Effects #50.
Source contract evidence only; not ESP-IDF, GPIO, BLE radio or optical proof.
"""
from pathlib import Path
import subprocess

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
