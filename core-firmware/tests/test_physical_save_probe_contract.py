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
# Authoritative v4 BEGIN/UPDATE CRC must not put a full controller plus
# 2x667-byte wire buffers on the NimBLE GATT task stack.
assert "static ot_pz_controller_t pz_crc_snapshot;" in core
assert "static uint8_t pz_crc_wire[OT_PZ_OUTPUTS][OT_PZ_MAX_WIRE_BYTES];" in core
active_crc=core.split("bool ot_ble_panel_active_crc32(",1)[1].split("int ot_ble_panel_preview_begin(",1)[0]
preview_crc=core.split("bool ot_ble_panel_preview_crc32(",1)[1].split("uint16_t ot_ble_panel_render_revision(",1)[0]
assert "ot_pz_controller_t committed;" not in active_crc
assert "ot_pz_controller_t candidate;" not in preview_crc
assert "xSemaphoreTake(om_write_mutex,0)" in active_crc
assert "xSemaphoreTake(om_write_mutex,0)" in preview_crc
assert "pz_controller_wire_crc32(&pz_crc_snapshot,crc32)" in active_crc
assert "pz_controller_wire_crc32(&pz_crc_snapshot,crc32)" in preview_crc
# BEGIN has the same atomic receipt requirement: creating a first zone
# must not require two follow-up nonblocking CRC mutex reads.
live_begin=core.split("int ot_ble_panel_preview_begin(",1)[1].split("int ot_ble_panel_preview_update_ranges(",1)[0]
assert "uint32_t *base_crc32" in live_begin and "uint32_t *candidate_crc32" in live_begin
assert "pz_controller_wire_crc32(&pz_outputs,&base_crc)" in live_begin
assert "pz_controller_wire_crc32(&pz_preview[lane].candidate,&live_crc)" in live_begin
assert live_begin.index("pz_controller_wire_crc32(&pz_preview[lane].candidate,&live_crc)") < live_begin.index("xSemaphoreGive(om_write_mutex);\n    if(accepted)request_render_refresh();")
assert "*base_crc32=base_crc;" in live_begin and "*candidate_crc32=live_crc;" in live_begin
begin_gatt=ble.split("if(command.opcode==OT_PZ_TX_OP_BEGIN){",1)[1].split("\n    const uint8_t lane=command.lane;",1)[0]
assert "ot_ble_panel_preview_begin(lane,revision,capacity," in begin_gatt
assert "panel_preview_tx.base_crc32=base_crc;" in begin_gatt
assert "panel_preview_tx.candidate_crc32=candidate_crc;" in begin_gatt
assert "ot_ble_panel_active_crc32(" not in begin_gatt
assert "ot_ble_panel_preview_crc32(" not in begin_gatt
assert "BLE_PANEL_BEGIN_ACK" in begin_gatt
# The 0xFE UPDATE callback may not publish a shadow and then race the
# 25ms configuration worker for a separate non-blocking CRC lock. Accepted
# geometry and its CRC must be one atomic receipt.
live_update=core.split("int ot_ble_panel_preview_update_ranges(",1)[1].split("int ot_ble_panel_preview_heartbeat(",1)[0]
assert "uint32_t *accepted_crc32" in live_update
assert "const bool session_ok=" in live_update
assert "if(!session_ok){xSemaphoreGive(om_write_mutex);return 3;}" in live_update
assert "if(!ot_pz_preview_apply_updates(candidate" in live_update
assert "xSemaphoreGive(om_write_mutex);return 1;" in live_update
assert "pz_controller_wire_crc32(&candidate->candidate,&crc)" in live_update
assert live_update.index("pz_controller_wire_crc32(&candidate->candidate,&crc)") < live_update.index("pz_preview[lane]=*candidate;")
assert live_update.index("pz_preview[lane]=*candidate;") < live_update.index("*accepted_crc32=crc;") < live_update.index("xSemaphoreGive(om_write_mutex);\n    if(valid)request_render_refresh();")
ble_update=ble.split("static int panel_preview_update_write(",1)[1].split("static int panel_preview_status_read(",1)[0]
assert "update.descriptors,update.descriptor_count,\n        &candidate_crc);" in ble_update
assert "ot_ble_panel_preview_crc32(" not in ble_update
assert 'BLE_PANEL_UPDATE_ACK lane=' in ble_update
assert ble_update.index("panel_preview_tx.candidate_crc32=candidate_crc;") < ble_update.index('BLE_PANEL_UPDATE_ACK lane=')
assert "if(rc==3){" in ble_update and "panel_preview_tx.sequence_sync_fault=true;" in ble_update

# BLE-only boot must bypass optional Wi-Fi + HTTP diagnostics without
# suppressing real fault latching or silently disabling the SPI status LED.
boot=core.split("void app_main(void) {",1)[1]
assert "static const bool boot_network_diagnostics=false;" in core
network_guard=boot.split("if(!boot_network_diagnostics){",1)[1].split("const bool wifi_started=wifi_ap_init();",1)[0]
assert 'BLE-only bench startup; network diagnostics disabled' in network_guard
assert "return;" in network_guard
assert boot.index("xTaskCreate(config_activation_task") < boot.index("if(!boot_network_diagnostics){")
assert boot.index("if(!boot_network_diagnostics){") < boot.index("wifi_ap_init();") < boot.index("http_start();")
assert 'ot_st_set_fault("BLE GATT startup failed")' in boot
assert 'ot_st_set_fault("render task could not start")' in boot
status=(root/"main/status_led.c").read_text()
assert "if(fault)return OT_ST_FAULT;" in status
assert "void ot_st_set_fault(const char *reason)" in status
assert "fault=true;" in status
cb=ble.split("static int cfg_write(",1)[1].split("/* BLE post-render snapshot v1:",1)[0]
assert "ot_ble_config_save(index,&output,cfg_upload_revision)" in cb
assert "cfg_ack(opcode,index,(uint8_t)rc)" in cb
assert "cfg_upload_index=255" in cb
# Physical Build 957: host HWM 680 bytes, then stack overflow in nimble_host
# during synchronous Save staging. The worker now owns the heavy call chain
# while the caller retains the writer mutex and waits for verified staging.
v4save=core.split("int ot_ble_panel_save(",1)[1].split("bool ot_ble_pixel_probe(",1)[0]
worker=core.split("static void pz_service_delegated_save(void){",1)[1].split("/* Legacy output writes",1)[0]
loop=core.split("static void config_activation_task(void *unused){",1)[1].split("static void input_sampler_task(",1)[0]
assert "ot_pz_controller_t *candidate=&pz_pair_workspace.candidate;" in v4save
assert "pz_save_controller_locked(" not in v4save
assert "pz_save_requested=true;" in v4save
assert "xSemaphoreTake(pz_save_done,portMAX_DELAY);" in v4save
assert v4save.index("pz_save_requested=true;") < v4save.index("xSemaphoreTake(pz_save_done,portMAX_DELAY);") < v4save.index("const bool saved=pz_save_result;")
assert v4save.index("const bool saved=pz_save_result;") < v4save.rindex("xSemaphoreGive(om_write_mutex);")
assert loop.index("pz_service_delegated_save();") < loop.index("xSemaphoreTake(om_write_mutex")
assert "xSemaphoreTake(om_write_mutex" not in worker
assert "BLE_PANEL_SAVE_BEGIN" in worker and "BLE_PANEL_SAVE_END" in worker
assert "uxTaskGetStackHighWaterMark(NULL)" in worker
assert "MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT" in worker
assert worker.index("BLE_PANEL_SAVE_BEGIN") < worker.index("pz_save_controller_locked(&pz_pair_workspace.candidate)") < worker.index("BLE_PANEL_SAVE_END")
assert worker.index("pz_save_result=saved;") < worker.index("xSemaphoreGive(pz_save_done);")
assert "worker_hwm<1024u" in worker
assert "#define OT_CONFIG_WORKER_STACK_BYTES 8192u" in core
assert "pz_save_done=xSemaphoreCreateBinary();" in core
assert "config_promotion_ready=pz_save_done&&" in core
assert "OT_CONFIG_WORKER_STACK_BYTES,NULL,5,NULL" in core
panel_cfg=ble.split("static int panel_cfg_write(",1)[1].split("static void panel_preview_set_error(",1)[0]
assert "ot_ble_panel_save(lane,&output,panel_upload_revision)" in panel_cfg
render=core.split("static void led_task(void *p) {",1)[1]
assert "if(zone>=0&&role!=OT_PZ_ROLE_UNASSIGNED&&role!=OT_OM_OFF)" in render
assert "c=ot_pz_fallback_color" not in render
assert "ot_pz_diag_render(&local_diag" in render
assert "PIXEL_PROBE_ACCEPT" in core and "PIXEL_PROBE_REJECT" in core
reset=core.split("int ot_ble_panel_reset_confirm(",1)[1].split("int ot_ble_panel_save(",1)[0]
assert "candidate->outputs[lane].active_pixels=0u;" in reset
assert '"Pixel OUT %u"' in reset
assert "memset(&candidate->outputs[lane],0,sizeof(candidate->outputs[lane]));" in reset
assert "candidate->outputs[lane].color_order=OT_OM_GRB;" in reset
assert "candidate->outputs[lane].gamma_enabled=false;" in reset
assert "pz_save_controller_locked(candidate)" in reset
legacy=core.split("static bool pz_load_legacy_snapshot(",1)[1].split("static void pz_load(void)",1)[0]
assert "Fresh Panel v4 defaults" in legacy
assert "ot_pz_migrate_legacy(om_outputs" not in legacy
assert "ot_pz_journal_recover(" in legacy
assert "nvs_erase" not in reset
assert "input_quiet_samples>=OT_DEBOUNCE_TICKS" in reset
assert "PIXEL_PROBE_RMT_DONE" in core and "PIXEL_PROBE_NONBLACK_FRAME" in core
rmt_init=core.split("static bool strip_configure_channel(",1)[1].split("static bool strip_init(",1)[0]
assert "RMT_OUTPUT_READY" in rmt_init and "out_pin[ch]" in rmt_init
assert "led_strip_new_rmt_device(&cfg,&rmt,&next)" in rmt_init
assert rmt_init.index("led_strip_new_rmt_device(&cfg,&rmt,&next)") < rmt_init.index("RMT_OUTPUT_READY")
assert "BLE_FX_DECISION" in core and "BLE_FX_LANE" in core
fxfn=core.split("static bool ot_ble_apply_fx_internal(",1)[1].split("bool ot_ble_apply_fx(",1)[0]
assert fxfn.index("portEXIT_CRITICAL(&state_lock);") < fxfn.index('ESP_LOGI(TAG,"BLE_FX_DECISION')
assert "fx_diag_eligible" in fxfn and "ot_pz_zone_pixel_count(zone)>0u" in fxfn
assert "output_runtime_fault_mask" in fxfn
# New zone effects must target the same volatile v4 mapping that the RMT task
# renders, even when the committed NVS map still has zero LEDs.
assert "pz_live_fx_views" in core
live_begin=core.split("int ot_ble_panel_preview_begin(",1)[1].split("int ot_ble_panel_preview_update_ranges(",1)[0]
live_update=core.split("int ot_ble_panel_preview_update_ranges(",1)[1].split("int ot_ble_panel_preview_heartbeat(",1)[0]
assert "pz_preview[lane].candidate.outputs[lane].active_pixels=pixel_capacity" in live_begin
assert "pz_live_fx_views[lane]=projected" in live_begin
assert "pz_live_fx_views[lane]=projected" in live_update
assert "pz_effective_panels_locked(pz_effective_fx_panels)" in fxfn
assert "fx_views,fx_panels" in fxfn
assert "ot_pz_to_fx_view(&candidate->candidate.outputs[lane]" in live_update
assert "probe_rmt_first_frame_generation" in core
assert "probe_rmt_bright_frame_generation" in core
assert "probe_nonblack[ch]=true" in render
assert "if(zone>=0&&role!=OT_PZ_ROLE_UNASSIGNED&&role!=OT_OM_OFF)" in render
assert "led_strip_refresh_async(strip[ch])" in render
assert "led_strip_refresh_async_done(strip[ch])" in render
assert "OT_OUT_LEFT_GPIO 16" in (root/"main/board_config.h").read_text()
assert "OT_OUT_RIGHT_GPIO 17" in (root/"main/board_config.h").read_text()
print("PASS: save uses mutex-protected static controller, bounded v1/v4 BLE stack and durable-phase logs, zero-LED inert, RMT+probe unchanged (SOURCE ONLY)")
