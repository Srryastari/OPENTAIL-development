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
assert "pz_preview_commit_requested=true" in preview_commit
assert "pz_save_controller_locked(" not in preview_commit
assert "ot_fx_bank_stop_all" not in preview_begin+preview_update+preview_heartbeat+preview_crc+preview_commit+preview_cancel
assert "native_show" not in preview_begin+preview_update+preview_heartbeat+preview_crc+preview_commit+preview_cancel
assert "nvs_open(" not in preview_cancel
assert "pz_render_revision[ch]=local_active_revision[ch]" in core
assert "ot_ble_panel_preview_commit_pending" in api
assert "ot_ble_panel_preview_commit_pending" in ble
config_worker = core.split("static void config_activation_task(", 1)[1].split(
    "static void input_sampler_task(", 1)[0]
assert "pz_preview_commit_requested" in config_worker
assert "pz_preview_commit_worker_active=true" in config_worker
assert "pz_save_controller_locked(&pz_pair_workspace.candidate)" in config_worker
assert "pz_preview_commit_worker_active=false" in config_worker

pz_save = core.split("static bool pz_save_controller_locked(", 1)[1].split(
    "static bool om_save(", 1)[0]
assert "pz_preview.active&&!pz_preview.committing" in pz_save
assert "pz_preview.committing=true;" in preview_commit
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
preview_model = (ROOT / "main/panel_zone.c").read_text()
preview_model_header = (ROOT / "main/panel_zone.h").read_text()
for token in ("pz_find_panel_wire", "pz_find_zone_wire",
              "OT_PZ_PANEL_ID_PREFIX|u->panel_id",
              "OT_PZ_ZONE_ID_PREFIX|u->zone_id",
              "OT_PZ_ID_WIRE_MASK"):
    assert token in preview_model or token in preview_model_header, token
assert "low-byte alias" in preview_model

# Runtime v4 pair storage: hardware-only lanes in ot_output_cfg and map-only
# Panel/Zone geometry in ot_zone_map. The same portable state machine is called
# by CORE and fault-injected by the host suite.
assert '#include "panel_zone_pair_store.h"' in core
pz_load = core.split("static void pz_load(void)", 1)[1].split(
    "static bool pz_save_controller_locked(", 1)[0]
for token in ("ot_pz_pair_store_snapshot", "ot_pz_pair_store_recover",
              "ot_pz_pair_store_begin", "pz_load_legacy_snapshot",
              "pz_pair_commit_pending", "om_deferred.pending=false"):
    assert token in pz_load, token
legacy_load = core.split("static bool pz_load_legacy_snapshot(", 1)[1].split(
    "static void pz_load(void)", 1)[0]
assert 'nvs_open("ot_zone_map",NVS_READONLY' in legacy_load
assert "ot_pz_journal_recover(" in legacy_load
assert "Fresh Panel v4 defaults" in legacy_load
assert "ot_pz_migrate_legacy(om_outputs" not in legacy_load
assert "if(om_legacy_journal_fault)return false;" not in legacy_load
assert "ot_pz_defaults(previous);*candidate=*previous;" in legacy_load
assert "pz_reset_tombstone_valid" in legacy_load
assert "OT_PZ_RESET_TOMBSTONE_KEY" in legacy_load
assert "nvs_set_blob(" not in legacy_load and "nvs_commit(" not in legacy_load

pair_store = (ROOT / "main/panel_zone_pair_store.c").read_text()
pair_header = (ROOT / "main/panel_zone_pair_store.h").read_text()
for token in ("ot_pz_pair_store_begin", "ot_pz_pair_store_finish",
              "ot_pz_pair_store_recover", "ot_pz_pair_store_mark_reset","OT_PZ_PAIR_STORE_MARKER_KEY",
              "write_verified","has_marker","OT_PZ_LANE_KEY_STAGED", "OT_PZ_MAP_KEY_STAGED",
              "OT_PZ_PAIR_INTENT_KEY", "OT_PZ_LANE_KEY_ACTIVE",
              "OT_PZ_MAP_KEY_ACTIVE"):
    assert token in pair_store or token in pair_header, token
assert pair_store.index("OT_PZ_LANE_KEY_STAGED") < pair_store.index(
    "OT_PZ_MAP_KEY_STAGED") < pair_store.index("OT_PZ_PAIR_INTENT_KEY")
assert pair_store.index("OT_PZ_LANE_KEY_ACTIVE") < pair_store.index(
    "OT_PZ_MAP_KEY_ACTIVE")
for phase in range(1, 10):
    assert f"phase_context,{phase}u" in pair_store or f"phase_context,{phase}u" in pair_store

pz_save = core.split("static bool pz_save_controller_locked(", 1)[1].split(
    "/* Legacy output writes", 1)[0]
assert "ot_pz_pair_store_begin" in pz_save
assert "pz_pair_commit_pending" in pz_save
assert "reset_mask" in pz_save
assert "nvs_open(" not in pz_save and "nvs_set_blob(" not in pz_save
panel_promotion = core.split("static void pz_promote(uint32_t epoch){", 1)[1].split(
    "static void config_activation_task(void *unused){", 1)[0]
for token in ("ot_pz_pair_store_finish", "ot_pz_pair_store_snapshot",
              "ot_pz_pair_store_recover", "ot_pz_join",
              "config_activation_idle(epoch,true)",
              "input_quiet_samples>=OT_DEBOUNCE_TICKS",
              "live_input_mask==0u", "tm_gpio_input_mask()==0u",
              "input_edge_epoch,__ATOMIC_ACQUIRE", "pz_outputs=workspace->pending.candidate"):
    assert token in panel_promotion, token
assert "config_recovery_fault=true" in panel_promotion
assert "nvs_open(" not in panel_promotion
assert panel_promotion.index("__atomic_load_n(&pz_pair_commit_pending") < panel_promotion.index(
    "&pz_pair_transaction_id,__ATOMIC_RELAXED")
# These values are observed by the BLE status path and the activation worker.
# Mixing plain volatile accesses with compiler atomics is still a C data race;
# require every runtime access to use the same atomic builtins.
core_lines=core.splitlines()
for variable in ("pz_pair_commit_pending","pz_pair_transaction_id",
                 "pz_pair_commit_phase","pz_pair_commit_phase_txn"):
    for index,line in enumerate(core_lines):
        if variable in line:
            start=index
            while start>0 and ";" not in core_lines[start-1]:
                start-=1
            statement="\n".join(core_lines[start:index+1])
            assert statement.lstrip().startswith("static ") or "__atomic_" in statement, (
                variable,statement)

# Phase telemetry is observable for a hardware relay/logger; this is a marker,
# not a claim that the physical endurance test has been run.
assert "PZ_PAIR_PHASE phase=%u transaction=%08lx" in core
status_endpoint = core.split("static esp_err_t status_get(", 1)[1].split(
    "static esp_err_t caps_get(", 1)[0]
for token in ("config_commit_phase", "config_commit_transaction_id",
              "config_commit_pending"):
    assert token in status_endpoint, token

# Trigger ISR telemetry is additive to the existing two SPSC turn queues.
telemetry = (ROOT / "main/input_telemetry.h").read_text()
for token in ("__atomic_always_lock_free", "input_edge_epoch __attribute__((aligned(4)))",
              "input_raw_edges",
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
assert "while(depth>high" not in core
assert "if(depth>high)(void)ot_isr_dram_cas(" in core
assert "xt_utils_compare_and_set(word,previous,next)" in core
assert "ot_isr_dram_fence();" in core
assert "DRAM_ATTR volatile uint32_t input_edge_epoch" in core

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
              "pz_reset_pending_mask", "pz_reset_challenge.expires_ms",
              "ot_pz_pair_store_mark_reset", "readback->map_active.reset_mask"):
    assert token in core, token
assert "reset_v3" in core
assert "pz_reset_tombstone_set_handle" not in core
assert "pz_reset_tombstone_write_handle" not in core
reset_confirm = core.split("int ot_ble_panel_reset_confirm(", 1)[1].split(
    "int ot_ble_panel_save(", 1)[0]
assert "candidate->outputs[lane].panel_count=0u" in reset_confirm
assert "candidate->outputs[lane].zone_count=0u" in reset_confirm
assert "candidate->outputs[lane].active_pixels=0u" in reset_confirm
assert '"Pixel OUT %u"' in reset_confirm
assert "memset(&candidate->outputs[lane],0,sizeof(candidate->outputs[lane]));" in reset_confirm
assert "candidate->outputs[lane].color_order=OT_OM_GRB;" in reset_confirm
assert "candidate->outputs[lane].rgb_gain[c]" in reset_confirm
assert "candidate->outputs[lane].white_gain[c]=255u;" in reset_confirm
assert "candidate->outputs[lane].gamma_enabled=false;" in reset_confirm
# Reset persistence is performed exactly once by the shared save path:
# changed geometry carries reset_mask in the pair intent; unchanged geometry
# uses mark_reset() inside pz_save_controller_locked().
pz_save_impl = core.split("static bool pz_save_controller_locked(",1)[1].split(
    "/* Legacy output writes are lossless",1)[0]
assert "ot_pz_pair_store_mark_reset" in pz_save_impl
assert "ot_pz_pair_store_mark_reset" not in reset_confirm
assert "nvs_open(" not in reset_confirm
assert "ot_tm_defaults" not in reset_confirm
assert "ot_ble_delete_fx_preset" not in reset_confirm

print("PASS Issue126 static runtime, namespace, ISR telemetry, brightness and reset contracts")


# Owner P0 acceptance addendum: COMMITTING is a protected, immutable boundary.
protocol_header = (ROOT / "main/panel_zone_protocol.h").read_text()
protocol = (ROOT / "main/panel_zone_protocol.c").read_text()
assert "OT_PZ_TX_ERROR_COMMITTING_LOCKED 5u" in protocol_header
assert "OT_PZ_TX_STATUS_V2_BYTES 61u" in protocol_header
assert "OT_PZ_TX_ERROR_SYNC_REQUIRED 10u" in protocol_header
assert "OT_PZ_TX_UPDATE_REJECT_COMMITTING" in protocol
assert "OT_PZ_TX_UPDATE_REJECT_SYNC_REQUIRED" in protocol
assert "OT_PZ_TX_SEQUENCE_GAP" in protocol
assert "OT_PZ_TX_SEQUENCE_OUT_OF_ORDER" in protocol

update_handler = ble.split("static int panel_preview_update_write(", 1)[1].split(
    "static int panel_preview_status_read(", 1)[0]
assert update_handler.index("if(panel_preview_tx.committing)") < update_handler.index(
    "ot_ble_panel_preview_update_ranges(")
assert "OT_PZ_TX_ERROR_COMMITTING_LOCKED" in update_handler
assert "panel_preview_reject_update(update.sequence" in update_handler
assert "panel_preview_tx.sequence_sync_fault" in update_handler

control_handler = ble.split("static int panel_preview_control_write(", 1)[1].split(
    "static int panel_preview_update_write(", 1)[0]
assert control_handler.index("if(panel_preview_tx.committing)") < control_handler.index(
    "ot_ble_panel_preview_commit(")
assert "command.candidate_crc32==panel_preview_tx.candidate_crc32" in control_handler
assert "OT_PZ_TX_ERROR_SYNC_REQUIRED" in control_handler

legacy_panel_write = ble.split("static int panel_cfg_write(", 1)[1].split(
    "static void panel_preview_set_error(", 1)[0]
assert "panel_preview_tx.committing" in legacy_panel_write
assert "OT_PZ_TX_ERROR_COMMITTING_LOCKED" in legacy_panel_write

status_handler = ble.split("static int panel_preview_status_read(", 1)[1].split(
    "static int pixel_probe_write(", 1)[0]
assert "OT_PZ_TX_STATUS_V2_BYTES" in status_handler
assert "OT_PZ_TX_STATUS_V2_VERSION" in status_handler
for token in ("last_rejected_sequence", "missing_sequence_count",
              "out_of_order_count", "duplicate_count", "conflict_count",
              "committing_reject_count", "sequence_sync_fault"):
    assert token in status_handler, token
assert "OT_PZ_TX_STATUS_V2_VERSION 2u" in protocol_header
assert "OT_PZ_TX_STATUS_V2_BYTES 61u" in protocol_header
assert "OT_PZ_MAX_RANGES_PER_ZONE,0x0fu" in ble

print("PASS Owner P0 protected-COMMITTING and sequence-watchdog source contracts")


# The COMMITTING protocol lock is published before CORE queues the durable save.
assert "panel_preview_tx.committing=true;" in control_handler
assert control_handler.index("panel_preview_tx.committing=true;") < control_handler.index(
    "ot_ble_panel_preview_commit(")
assert "panel_preview_reject_committing_update(&update)" in update_handler
assert "panel_preview_missing_before(update.sequence)" in update_handler
committing_classifier = ble.split("static void panel_preview_reject_committing_update(", 1)[1].split(
    "static bool panel_preview_session_matches(", 1)[0]
for token in ("OT_PZ_TX_SEQUENCE_DUPLICATE", "OT_PZ_TX_SEQUENCE_GAP",
              "OT_PZ_TX_SEQUENCE_OUT_OF_ORDER", "OT_PZ_TX_SEQUENCE_CONFLICT",
              "duplicate_count", "missing_sequence_count", "out_of_order_count",
              "conflict_count"):
    assert token in committing_classifier, token

# A random per-boot identity is available to the physical endurance log via /api/v1/status.
core = (ROOT / "main/opentail_core.c").read_text()
assert "core_boot_id=esp_random()" in core
status_endpoint = core.split("static esp_err_t status_get(", 1)[1].split(
    "static esp_err_t caps_get(", 1)[0]
assert "boot_id" in status_endpoint and "core_boot_id" in status_endpoint

print("PASS owner addendum commit race, delayed sequence telemetry and boot identity contracts")
