#!/usr/bin/env python3
"""Static NimBLE GAP recovery contract; ESP-IDF compile and phone tests are separate."""
from pathlib import Path

src = (Path(__file__).resolve().parents[1] / "main" / "ble_status.c").read_text()

def section(start: str, end: str) -> str:
    assert src.count(start) == 1, f"Missing/duplicate marker: {start}"
    return src.split(start, 1)[1].split(end, 1)[0]

reset = section("static void on_reset(int reason){", "static void on_sync(void){")
sync = section("static void on_sync(void){", "static void host_task(")
gap = section("static int gap_event(struct ble_gap_event *ev,void *arg){", "static void on_reset(int reason){")
advertise = section("static void advertise(void){", "static int gap_event(struct ble_gap_event *ev,void *arg){")
init = section("bool ot_ble_start(void){", "#else")

assert "ble_hs_cfg.reset_cb=on_reset;" in init
assert "ble_hs_cfg.sync_cb=on_sync;" in init
assert "active_conn=BLE_HS_CONN_HANDLE_NONE;" in reset
assert "authorized=false" in reset and "challenge_ready=false" in reset
assert "ot_ble_disconnect_show();" in reset and "ot_st_ble_connected(false)" in reset
for transient in ("cfg_reset()", "frame_reply_reset()", "ot_palette_ble_session_reset()", "trigger_last_status=0"):
    assert transient in reset and transient in gap, transient
assert "owner_key" not in reset and "owner_create" not in reset, "Reset must preserve persisted key"
assert "advertise();" not in reset, "Cannot advertise until controller resynchronization"
assert "advertise();" in sync, "Sync must restart advertising with cleared stale handle"
assert "if(active_conn!=BLE_HS_CONN_HANDLE_NONE)return;" in advertise
assert "ble_gap_adv_start(" in advertise
assert "p.itvl_min=48;p.itvl_max=96" in advertise

assert "BLE_GAP_EVENT_DISCONNECT" in gap
assert "ev->disconnect.conn.conn_handle!=active_conn" in gap, "Stray central must not revoke owner"
assert "ble_gap_update_params(active_conn,&fast)" in gap
assert ".itvl_min=12,.itvl_max=24,.latency=0,.supervision_timeout=400" in gap
assert 'if(rc)ESP_LOGW(' in gap, "A central refusing interval request must not disconnect"
assert "ble_gap_terminate(active_conn" not in gap
assert "BLE_GAP_EVENT_MTU" in gap
assert "ble_att_set_preferred_mtu(247)" in init
assert "owner_valid=owner_load()" in init
assert "ble_hs_cfg.reset_cb=on_reset;" in init
for name in ("command_write", "cfg_write", "trigger_access", "palette_write", "frame_write"):
    body = section(f"static int {name}(", "static " if name != "frame_write" else "static int frame_read(")
    assert "authorized" in body or name == "command_write", f"Authorization guard lost in {name}"
assert "fx_capabilities_read" in src and "ctx->offset" in src
assert "OT_PALETTE_WIRE_VERSION" in src and "OT_TM_BLE_WRITE_BYTES" in src

print("PASS: GAP reset/reconnect policy, bounded low-latency negotiation, preserved BLE ABI/auth guards (source contract)")
