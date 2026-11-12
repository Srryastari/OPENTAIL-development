#!/usr/bin/env python3
"""Save stack-safety, zero-LED inertness, and probe-to-RMT boundary.
Static host contract only. Hardware SAVE and RMT remain NOT_RUN.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
ble=(root/"main/ble_status.c").read_text()
fn=core.split("int ot_ble_config_save(",1)[1].split("/* Versioned authenticated hierarchical",1)[0]
assert "ot_pz_controller_t candidate;" not in fn
assert "ot_pz_controller_t *const candidate=&pz_pair_workspace.candidate;" in fn
assert fn.index("xSemaphoreTake(om_write_mutex") < fn.index("pz_pair_workspace.candidate")
assert "ot_pz_from_legacy_output(next,index,candidate,&replacement)" in fn
panel=(root/"main/panel_zone.c").read_text()
conversion=panel.split("bool ot_pz_from_legacy_output(",1)[1].split("bool ot_pz_to_legacy(",1)[0]
assert "ot_pz_controller_t check=*existing;" not in conversion
assert "if(existing && !ot_pz_valid(existing))return false;" in conversion
assert "pz_allocate_id(existing,lane,&next" in conversion
assert "ot_pz_output_valid(&next,lane)" in conversion
assert "pz_save_controller_locked(candidate)" in fn
assert "BLE_OUTPUT_SAVE_BEGIN" in fn and "BLE_OUTPUT_SAVE_END" in fn
assert "uxTaskGetStackHighWaterMark(NULL)" in fn
assert "elapsed_ms" in fn and "MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT" in fn
assert "nvs_commit(" in core and "PZ_PAIR_PHASE" in core
cb=ble.split("static int cfg_write(",1)[1].split("/* BLE post-render snapshot v1:",1)[0]
assert "ot_ble_config_save(index,&output,cfg_upload_revision)" in cb
assert "cfg_ack(opcode,index,(uint8_t)rc)" in cb
assert "cfg_upload_index=255" in cb
render=core.split("static void led_task(void *p) {",1)[1]
assert "if(local_panel[ch].active_pixels!=0u)" in render
assert "c=ot_pz_fallback_color((unsigned)ch,fallback_mask,255u)" in render
assert "ot_pz_diag_render(&local_diag" in render
assert "PIXEL_PROBE_ACCEPT" in core and "PIXEL_PROBE_REJECT" in core
assert "PIXEL_PROBE_RMT_DONE" in core and "PIXEL_PROBE_NONBLACK_FRAME" in core
assert "BLE_FX_DECISION" in core and "BLE_FX_LANE" in core
fxfn=core.split("static bool ot_ble_apply_fx_internal(",1)[1].split("bool ot_ble_apply_fx(",1)[0]
assert fxfn.index("portEXIT_CRITICAL(&state_lock);") < fxfn.index('ESP_LOGI(TAG,"BLE_FX_DECISION')
assert "fx_diag_eligible" in fxfn and "ot_pz_zone_pixel_count(zone)>0u" in fxfn
assert "output_runtime_fault_mask" in fxfn
assert "probe_rmt_first_frame_generation" in core
assert "probe_rmt_bright_frame_generation" in core
assert "probe_nonblack[ch]=true" in render
assert "if(local_panel[ch].active_pixels!=0u)" in render
assert "led_strip_refresh_async(strip[ch])" in render
assert "led_strip_refresh_async_done(strip[ch])" in render
assert "OT_OUT_LEFT_GPIO 16" in (root/"main/board_config.h").read_text()
assert "OT_OUT_RIGHT_GPIO 17" in (root/"main/board_config.h").read_text()
print("PASS: save uses mutex-protected static controller, bounded BLE stack, logged durable phase, zero-LED inert, RMT+probe unchanged (SOURCE ONLY)")
