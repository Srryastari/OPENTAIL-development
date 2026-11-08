from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
core = (ROOT / "main/opentail_core.c").read_text()
ble = (ROOT / "main/ble_status.c").read_text()
api = (ROOT / "main/panel_zone_api.h").read_text()

for token in (
    "ot_pz_build_pixel_map(&local_panel[ch],pixel_zone_map[ch],pixel_logical_map[ch])",
    "pixel_zone_map[ch][px]",
    "pixel_logical_map[ch][px]",
    "ot_pz_fallback_color",
    "ot_pz_diag_render",
    "prior_pixel_zone_map",
    "fx_zone_scratch",
):
    assert token in core, token

assert "prior_fx_offset" not in core
assert "config_activation_idle(epoch)" not in core
assert re.search(r"ot_frame_snapshot_begin\(\(uint8_t\)ch,\s*render_pixels\[ch\]", core)
assert "ot_pz_preview_cancel(&pz_preview)" in core
for token in ("ot_ble_panel_preview_begin", "ot_ble_panel_preview_update",
              "ot_ble_panel_preview_commit", "ot_ble_panel_preview_cancel"):
    assert token in core and token in api, token
assert "OT_PZ_PREVIEW_LEASE_MS" in core
assert "config_activation_idle(epoch,true)" in core
assert "config_activation_idle(epoch,false)" in core
assert "if(pz_preview.active&&physical==0u)" in core
assert "pz_preview.candidate.outputs" in core

panel_save = core.split("int ot_ble_panel_save(", 1)[1].split("bool ot_ble_pixel_probe(", 1)[0]
assert "show_window(now)" not in panel_save
assert "raw_active" not in panel_save
assert "is_show_mode_active" not in panel_save

for opcode in ("case 0x10u:", "case 0x11u:", "case 0x12u:",
               "case 0x13u:", "case 0x14u:"):
    assert opcode in ble, opcode
assert "BLE_GATT_CHR_F_WRITE_NO_RSP" in ble
assert "panel_caps_seen" in ble and "authorized" in ble
assert "OT_PZ_PREVIEW_LEASE_MS/1000u" in ble
assert "(uint8_t)OT_MAX_BRIGHTNESS" in ble
assert "led_strip_" not in ble and "rmt_" not in ble

# Panel/Zone active/staged records now live in the dedicated namespace.
pz_load = core.split("static void pz_load(void)", 1)[1].split("static bool pz_save_controller_locked(", 1)[0]
pz_save = core.split("static bool pz_save_controller_locked(", 1)[1].split("int ot_ble_panel_get(", 1)[0]
pz_promote = core.split("static void pz_promote(", 1)[1].split("static void om_promote(", 1)[0]
assert 'nvs_open("ot_zone_map"' in pz_load
assert 'nvs_open("ot_zone_map"' in pz_save
assert 'nvs_open("ot_zone_map"' in pz_promote
assert "pz_stage_legacy_migration" in core
assert 'nvs_open("ot_output_cfg"' not in pz_load

# Trigger ISR telemetry is additive to the existing two SPSC turn queues.
telemetry = (ROOT / "main/input_telemetry.h").read_text()
for token in ("__atomic_always_lock_free", "input_raw_edges",
              "input_asserted_edges", "input_deasserted_edges",
              "turn_edge_high_water", "turn_edge_overflow_count",
              "input_index>=OT_IN_COUNT", "gpio_ll_get_level"):
    assert token in core, token
for token in ("OT_INPUT_DIAG_INPUTS", "OT_INPUT_DIAG_WIRE_BYTES",
              "ot_input_diag_snapshot_get"):
    assert token in telemetry or token in ble or token in core, token
assert "ot_input_diag_uuid" in ble
assert "input_diag_access" in ble
assert "conn!=active_conn||!authorized" in ble

# Full 8-bit requests are accepted, then limited once at the final CORE RGB boundary.
board = (ROOT / "main/board_config.h").read_text()
output_header = (ROOT / "main/output_manager.h").read_text()
fx_command = (ROOT / "main/ble_fx_command.c").read_text()
assert "ot_brightness_limit_request" in board
assert "ot_brightness_limit_request(p[12])" in fx_command
assert "ot_om_limit_brightness" in output_header
assert "ot_om_limit_brightness(c,(uint8_t)OT_MAX_BRIGHTNESS)" in core
assert "auto_sweep" not in core.lower()

print("PASS Issue126 static runtime, namespace, ISR telemetry and brightness contracts")
