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
for token in ("ot_ble_panel_preview_begin", "ot_ble_panel_preview_update_ranges",
              "ot_ble_panel_preview_heartbeat", "ot_ble_panel_preview_crc32",
              "ot_ble_panel_preview_commit", "ot_ble_panel_preview_cancel",
              "ot_ble_panel_render_revision"):
    assert token in core and token in api, token
assert "OT_PZ_PREVIEW_LEASE_MS" in core
assert "config_activation_idle(epoch,true)" in core
assert "config_activation_idle(epoch,false)" in core
assert "if(pz_preview.active&&physical==0u)" in core
assert "pz_preview.candidate.outputs" in core

preview_begin = core.split("int ot_ble_panel_preview_begin(", 1)[1].split("int ot_ble_panel_preview_update_ranges(", 1)[0]
preview_update = core.split("int ot_ble_panel_preview_update_ranges(", 1)[1].split("int ot_ble_panel_preview_heartbeat(", 1)[0]
preview_heartbeat = core.split("int ot_ble_panel_preview_heartbeat(", 1)[1].split("bool ot_ble_panel_preview_crc32(", 1)[0]
preview_crc = core.split("bool ot_ble_panel_preview_crc32(", 1)[1].split("uint16_t ot_ble_panel_render_revision(", 1)[0]
preview_commit = core.split("int ot_ble_panel_preview_commit(", 1)[1].split("void ot_ble_panel_preview_cancel(", 1)[0]
preview_cancel = core.split("void ot_ble_panel_preview_cancel(", 1)[1].split("int ot_ble_panel_reset_challenge(", 1)[0]
assert "ot_pz_preview_begin(" in preview_begin and "nvs_open(" not in preview_begin
assert "ot_pz_preview_apply_updates(" in preview_update and "nvs_open(" not in preview_update
assert "xSemaphoreTake(om_write_mutex,0)" in preview_update
assert "ot_pz_preview_heartbeat(" in preview_heartbeat and "nvs_open(" not in preview_heartbeat
assert "ot_pz_wire_encode" in core and "ot_cfg_crc32" in core
assert "pz_save_controller_locked(&snapshot.candidate)" in preview_commit
assert "ot_fx_bank_stop_all" not in preview_begin+preview_update+preview_heartbeat+preview_crc+preview_commit+preview_cancel
assert "native_show" not in preview_begin+preview_update+preview_heartbeat+preview_crc+preview_commit+preview_cancel
assert "nvs_open(" not in preview_cancel
assert "pz_render_revision[ch]=local_active_revision[ch]" in core

pz_save = core.split("static bool pz_save_controller_locked(", 1)[1].split(
    "static bool om_save(", 1)[0]
assert "pz_preview.active&&!pz_preview.committing" in pz_save
assert "else pz_preview.committing=true" in core
panel_save = core.split("int ot_ble_panel_save(", 1)[1].split("bool ot_ble_pixel_probe(", 1)[0]
assert "show_window(now)" not in panel_save
assert "raw_active" not in panel_save
assert "pz_preview.active" in panel_save
assert "is_show_mode_active" not in panel_save

for token in ("panel_preview_control_write","panel_preview_update_write",
              "panel_preview_status_read","ot_panel_preview_control_uuid",
              "ot_panel_preview_update_uuid","ot_panel_preview_status_uuid",
              "ot_pz_tx_parse_update","ot_pz_tx_sequence_check","ble_att_mtu(conn)"):
    assert token in ble, token
assert "panel_upload_preview" not in ble
assert "case 0x10u:" not in ble and "case 0x14u:" not in ble
assert "BLE_GATT_CHR_F_WRITE_NO_RSP" in ble
assert "panel_caps_seen" in ble and "authorized" in ble
assert "OT_PZ_PREVIEW_LEASE_MS/1000u" in ble
assert "(uint8_t)OT_MAX_BRIGHTNESS" in ble
assert "led_strip_" not in ble and "rmt_" not in ble
protocol = (ROOT / "main/panel_zone_protocol.c").read_text()
protocol_header = (ROOT / "main/panel_zone_protocol.h").read_text()
for token in ("OT_PZ_TX_UPDATE_HEADER_BYTES","OT_PZ_TX_DESCRIPTOR_BYTES",
              "OT_PZ_TX_MAX_DESCRIPTORS","ot_pz_tx_sequence_check"):
    assert token in protocol_header, token
assert "ot_pz_tx_parse_update" in protocol
assert "OT_PZ_PREVIEW_LEASE_MS 5000u" in (ROOT / "main/panel_zone.h").read_text()
assert "OT_PZ_TX_OP_HEARTBEAT" in protocol_header

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
assert "ot_brightness_limit_request" not in board
assert ".brightness=p[12]" in fx_command
assert 'bounded_int(obj,"brightness",24,0,255)' in core
assert 'bounded_int(obj,"brightness",20,0,255)' in core
assert 'bounded_int(obj,"brightness",32,0,255)' in core
assert "req->brightness>OT_MAX_BRIGHTNESS" not in core
assert "ot_om_limit_brightness" in output_header
assert "ot_om_limit_brightness(c,(uint8_t)OT_MAX_BRIGHTNESS)" in core
assert "auto_sweep" not in core.lower()

# Destructive reset is a revision-bound authenticated challenge/confirm flow.
panel_api = (ROOT / "main/panel_zone_api.h").read_text()
for token in ("ot_ble_panel_reset_challenge", "ot_ble_panel_reset_confirm",
              "OT_PZ_RESET_CHALLENGE_TTL_MS"):
    assert token in panel_api, token
for token in ("opcode==0xffu&&lane==0xaau", "opcode==0xffu&&lane==0xabu",
              "ot_ble_panel_reset_challenge", "ot_ble_panel_reset_confirm"):
    assert token in ble, token
for token in ("OT_PZ_RESET_TOMBSTONE_KEY", "pz_reset_tombstone_valid",
              "pz_reset_tombstone_set_handle", "pz_reset_pending_mask",
              "pz_reset_challenge.expires_ms", "readback.candidate_revision"):
    assert token in core, token
assert "reset_v3" in core

print("PASS Issue126 static runtime, namespace, ISR telemetry, brightness and reset contracts")
