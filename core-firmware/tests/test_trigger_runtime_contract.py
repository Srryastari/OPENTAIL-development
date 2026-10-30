#!/usr/bin/env python3
"""Trigger-only cross-system source contract.

These checks guard wiring/sampling/render/BLE/NVS boundaries on the actual
CORE source. They do not substitute for physical pin, optical, radio or reboot
tests. Keep this separate from effects/Android/OTA release certification.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / "main"
core = (root / "opentail_core.c").read_text(encoding="utf-8")
board = (root / "board_config.h").read_text(encoding="utf-8")
logic = (root / "core_logic.c").read_text(encoding="utf-8")
ble = (root / "ble_status.c").read_text(encoding="utf-8")
outputs = (root / "output_manager.c").read_text(encoding="utf-8")
manager = (root / "trigger_manager.c").read_text(encoding="utf-8")


def between(source, start, end):
    assert source.count(start) == 1, f"Missing/nonunique: {start}"
    return source.split(start, 1)[1].split(end, 1)[0]


# 1–2: physical pin allocation and predebounce levels have one polarity
# interpretation; no renderer-frame-rate-based GPIO polling.
assert re.search(r"#define OT_INPUT_GPIO_LIST\s*\{4,5,6,7,8,9,10,11\}", board)
assert "#define OT_DEBOUNCE_TICKS 4" in board
assert re.search(r"#define OT_INPUT_ACTIVE_LOW\s+[01](?:\s|$)", board)
assert "static const gpio_num_t input_pin[OT_IN_COUNT]=OT_INPUT_GPIO_LIST;" in core
entry = next((signature for signature in
              ("static bool input_init(void) {", "static void input_init(void) {")
              if signature in core), None)
assert entry, "Trigger GPIO initializer missing"
init = core.split(entry, 1)[1].split("/* RMT drivers", 1)[0]
if "static bool input_init(void)" in core:
    assert "if(e!=ESP_OK)" in init and "return false;" in init
    assert "const bool inputs_ready=input_init();" in core
    assert "sampler_ready=inputs_ready &&" in core
assert ".mode=GPIO_MODE_INPUT" in init
assert ".pull_up_en=GPIO_PULLUP_ENABLE" in init
sampler = between(core, "static void input_sampler_task(void *arg){", "static void frame_wake_callback")
for token in ("pdMS_TO_TICKS(10u)", "gpio_get_level(input_pin[i])",
              "OT_INPUT_ACTIVE_LOW?(level==0):(level!=0)",
              "ot_debounce(&sampler,levels,OT_DEBOUNCE_TICKS)",
              "live_input_mask=physical", "xTaskNotifyGive(target)",
              "vTaskDelayUntil(&last_wake,period)"):
    assert token in sampler, f"Input sampler contract lost: {token}"
assert "in->ticks[i]>=stable_ticks" in logic
assert "in->settled[i]=in->pending[i]" in logic

# 3: universal user mapping, duplicate/disabled functions and no
# car-specific physical assignments in trigger resolution.
assert "for(int i=0;i<OT_IN_COUNT;i++)" in manager
assert "p->function[i]!=OT_TM_DISABLED" in manager
assert "mask|=(uint8_t)(1u<<(p->function[i]-1u))" in manager

# 4–6: the authoritative renderer resolves the physical mapping and
# simulation before composing per-user-zone effect pixels. Overlay only
# touches the matched role and must NOT undergo decorative calibration.
renderer = between(core, "static void led_task(void *p) {", "void app_main(")
assert "local_tm=tm_profile;" in renderer
assert "combined=ot_tm_resolve(&local_tm,physical);" in renderer
assert "if(before_deadline(now,sim_deadline)) combined|=sim_mask;" in renderer
assert "ot_om_section_next(output,&zone_cursor,px)" in renderer
assert "ot_om_trigger_overlay(role,combined," in renderer
assert "if(!trigger_here){" in renderer
assert "if(decorated)c=ot_om_calibrate(c,output);" in renderer
assert renderer.index("ot_om_trigger_overlay(role,combined,") < renderer.index(
    "if(!trigger_here){") < renderer.index("ot_om_swizzle(c,output->color_order)")
assert "case OT_OM_PARK_BRAKE:" in outputs
assert "case OT_OM_LEFT:" in outputs
assert "case OT_OM_RIGHT:" in outputs
assert "case OT_OM_REVERSE:" in outputs
assert "case OT_OM_AUX3:" in outputs
assert "show_here?(uint8_t)(mask&~OT_SIG_PARK):mask" in outputs
assert "if(hit&&color)*color=ot_om_trigger_color(role,active,brightness);" in outputs
assert "if(trigger_here)c=ot_om_calibrate(" not in renderer
assert "OT_OM_MAX_PIXELS_PER_OUTPUT" in (root / "output_manager.h").read_text()
# Current firmware owns the final pixel transport and fault signaling.
# The trigger branch must not accidentally roll back newer CORE failure
# handling, runtime boot gates, or the post-trigger logical frame snapshot.
# Check order inside the actual single-owner render task, not by the presence
# of unrelated symbols elsewhere in the file.
frame_capture = renderer.index("ot_frame_snapshot_pixel(")
wire_swizzle = renderer.index("ot_om_swizzle(c,output->color_order)")
trigger_overlay = renderer.index("ot_om_trigger_overlay(role,combined,")
driver_write = renderer.index("led_strip_set_pixel(")
assert trigger_overlay < frame_capture < wire_swizzle < driver_write
assert "output_runtime_fault_mask" in renderer
assert "output_runtime_fault(" in renderer
assert "native_outputs_ready=all_configured && output_runtime_fault_mask==0u" in renderer
assert "ot_frame_snapshot_unavailable(" in renderer
# Only RMT lanes with completed transfers count toward frame-rate wire budget.
# The display may still be subject to separate physical timing acceptance.
assert "uint16_t longest_pixels=0; /* Only successfully refreshed RMT lanes. */" in renderer
assert renderer.index("uint16_t longest_pixels=0;") < renderer.index("bool frame_ready[OT_CHAIN_COUNT]")
completion = renderer.index("if(frame_captured[ch])ot_frame_snapshot_complete(true);")
assert renderer.index("if(local_outputs[ch].active_pixels>longest_pixels)",completion) > completion
assert renderer.index("if(local_outputs[ch].active_pixels>longest_pixels)",completion) < renderer.index("ot_fps_pacer_observe(&pacer,work_us,longest_pixels)")
assert "static bool input_init(void)" in core
assert "boot_enrollment_ready" in ble
assert "__atomic_store_n(&ble_radio_ever_ready,true,__ATOMIC_RELEASE)" in ble
# An authenticated owner's session must survive rejected or stale
# secondary central events. Both owner session resets must clear only the
# trigger GATT's transient last status, never the physical inputs.
advertise = between(ble, "static void advertise(void){", "static int gap_event(struct ble_gap_event *ev,void *arg){")
gap = between(ble, "static int gap_event(struct ble_gap_event *ev,void *arg){", "static void on_sync(")
assert "if(active_conn!=BLE_HS_CONN_HANDLE_NONE)return;" in advertise
assert "ble_gap_terminate(ev->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM)" in gap
assert gap.index("if(active_conn!=BLE_HS_CONN_HANDLE_NONE){") < gap.index("active_conn=ev->connect.conn_handle;")
assert "if(ev->disconnect.conn.conn_handle!=active_conn)return 0;" in gap
assert gap.index("if(ev->disconnect.conn.conn_handle!=active_conn)return 0;") < gap.index(
    "active_conn=BLE_HS_CONN_HANDLE_NONE;")
assert gap.count("trigger_last_status=0;") == 2
assert "ot_ble_disconnect_show();" in gap and "live_input_mask=" not in gap
# Invalid owner re-auth must invalidate cached mapping status along with
# output and RGB frame replies, without mutating physical GPIO signals.
command = between(ble,"static int command_write(","/* BLE Output Config")
assert "const bool was_authorized=authorized;" in command
revoke = between(command,"if(was_authorized && !authorized){","if(last_result==OT_BLE_OK_SHOW")
for action in ("ot_ble_disconnect_show();","cfg_reset();","frame_reply_reset();","trigger_last_status=0;"):
    assert action in revoke
assert "live_input_mask=" not in command
assert ble.index("static uint8_t trigger_last_status=0;") < ble.index("static int command_write(")
assert "elf_sha256" in core  # running-image identity, not physical proof

# 7: time-limited logical bench simulation is not a raw pin mask and
# does not mutate the independent physical input snapshot.
simulate = between(core, "static esp_err_t simulate_post(httpd_req_t *r) {",
                   "static esp_err_t stop_post(httpd_req_t *r) {")
assert "ot_tm_signal_mask_of(v->valuestring)" in simulate
assert 'bounded_int(obj,"ttl_ms",2000,100,OT_SIM_MAX_MS)' in simulate
assert "sim_mask=mask;" in simulate
assert "sim_deadline=mask?now+pdMS_TO_TICKS(ttl):now;" in simulate
assert "live_input_mask=" not in simulate
assert "#define OT_SIM_MAX_MS 15000u" in core
stop = between(core, "static esp_err_t stop_post(httpd_req_t *r) {",
               "extern const uint8_t ot_dev_console_start[]")
assert "sim_mask=0;" in stop
assert "live_input_mask=" not in stop
status = between(core, "static esp_err_t status_get(httpd_req_t *r) {",
                 "static esp_err_t")
assert "ot_tm_resolve(&tm,physical)" in status
assert 'simulated_mask' in status
readback = between(core, "static esp_err_t tm_get(httpd_req_t *r){",
                   "static esp_err_t tm_post(httpd_req_t *r){")
assert 'cJSON_AddNumberToObject(root,"raw_input_mask",physical)' in readback
assert 'cJSON_AddNumberToObject(root,"gpio_input_mask",tm_gpio_input_mask())' in readback
assert 'cJSON_AddNumberToObject(root,"effective_mask",ot_tm_resolve(&cfg,physical)|sim)' in readback

# All required trigger readback fields and both arrays must be atomic from
# the client's perspective: low heap -> HTTP 500, not partial HTTP 200.
# The GET handler must never mutate active or saved input mapping.
for required in (
    '"revision",OT_TM_VERSION',
    '"config_revision",config_revision',
    '"saved_on_device",saved',
    '"config_apply_pending",pending',
    '"bench_only",true',
    '"physical_inputs",OT_IN_COUNT',
    '"fixed_active_low",OT_INPUT_ACTIVE_LOW!=0',
    '"raw_input_mask",physical',
    '"gpio_input_mask",tm_gpio_input_mask()',
    '"physical_function_mask",ot_tm_resolve(&cfg,physical)',
    '"effective_mask",ot_tm_resolve(&cfg,physical)|sim',
):
    assert '!cJSON_Add' in readback.split(required,1)[0].splitlines()[-1], required
for guard in (
    'if(!supported)goto oom;',
    'if(!name)goto oom;',
    'if(!cJSON_AddItemToArray(supported,name)){cJSON_Delete(name);goto oom;}',
    'if(!arr)goto oom;',
    'if(!v)goto oom;',
    'cJSON_Delete(v);goto oom;',
    'if(!cJSON_AddItemToArray(arr,v)){cJSON_Delete(v);goto oom;}',
    'if(!body)return respond_err(r,500,"out of memory");',
    'oom:\n    cJSON_Delete(root);\n    return respond_err(r,500,"out of memory");',
):
    assert guard in readback, guard
assert readback.index('cJSON_PrintUnformatted(root)') < readback.index('oom:')
assert "tm_profile=" not in readback and "tm_save(" not in readback


# 8: BLE owner-authorized read/write and revision readback, including
# disconnect not zeroing physical signals.
gatt = between(ble, "static int trigger_access(", "static const struct ble_gatt_chr_def")
for token in ("conn!=active_conn||!authorized", "ot_ble_trigger_get(",
              "ot_tm_ble_encode(", "ot_tm_ble_decode(",
              "ot_ble_trigger_save(", "trigger_last_status"):
    assert token in gatt
assert ".uuid=&ot_trigger_uuid.u,.access_cb=trigger_access" in ble
assert "size!=OT_TM_BLE_WRITE_BYTES" in manager
assert "wire[0]!=OT_TM_BLE_VERSION" in manager
disconnect = between(core, "void ot_ble_disconnect_show(void){",
                     "static esp_err_t status_get(")
assert "live_input_mask=0" not in disconnect
# New CORE STOP behavior must wake the single render owner immediately
# after exiting state_lock; never mutate GPIO input state or issue RMT writes here.
refresh = between(core, "static void request_render_refresh(void){", "static uint8_t live_input_mask=0;")
assert "task=render_task_handle;" in refresh
assert "portEXIT_CRITICAL(&state_lock);" in refresh
assert refresh.index("portEXIT_CRITICAL(&state_lock);") < refresh.index("xTaskNotifyGive(task)")
for begin, end in (("static esp_err_t show_stop(httpd_req_t *r){", "/* Provision an ephemeral"),
                   ("static esp_err_t stop_post(httpd_req_t *r) {", "/* CORE ships only its own")):
    op = between(core, begin, end)
    assert op.index("portEXIT_CRITICAL(&state_lock);") < op.index("request_render_refresh();")
    assert "live_input_mask=0" not in op
ble_stop = between(core, "void ot_ble_stop_show(void){", "/* Eight opt-in named-by-slot")
assert "if(refresh_requested)request_render_refresh();" in ble_stop
disconnect_stop = between(core, "void ot_ble_disconnect_show(void){", "static esp_err_t status_get(")
assert "if(owned_before)request_render_refresh();" in disconnect_stop
# Accepted show, FX, demo and synthetic trigger updates must wake the
# single render owner after state_lock is released, including sim-mask release.
# Rejected requests and hardware inputs never use these synthetic handlers.
for begin,end,expected in (
    ("static esp_err_t show_start(httpd_req_t *r){","static esp_err_t show_stop(httpd_req_t *r){","request_render_refresh();"),
    ("bool ot_ble_apply_show(","/* Runtime control over BLE;","if(ready)request_render_refresh();"),
    ("static bool ot_ble_apply_fx_internal(","bool ot_ble_apply_fx(","if(ready)request_render_refresh();"),
    ("bool ot_ble_play_fx_preset(","bool ot_ble_delete_fx_preset(","if(ready)request_render_refresh();"),
    ("static esp_err_t demo_post(httpd_req_t *r) {","static esp_err_t simulate_post(httpd_req_t *r) {","request_render_refresh();"),
    ("static esp_err_t simulate_post(httpd_req_t *r) {","static esp_err_t stop_post(httpd_req_t *r) {","request_render_refresh();"),
):
    operation=between(core,begin,end)
    assert operation.index("portEXIT_CRITICAL(&state_lock);") < operation.index(expected)
    assert "live_input_mask=" not in operation
assert "ot_tm_signal_mask_of(v->valuestring)" in between(
    core,"static esp_err_t simulate_post(httpd_req_t *r) {","static esp_err_t stop_post(httpd_req_t *r) {")


# 9: save admission includes instant GPIO and settled GPIO, source-level
# ordering preserves RAM on NVS failure. No claim of atomicity DURING NVS.
raw = between(core, "static uint8_t tm_gpio_input_mask(void){",
              "/* Serialize authenticated BLE")
assert "gpio_get_level(input_pin[i])" in raw
assert "OT_INPUT_ACTIVE_LOW?!high:high" in raw
save = between(core, "static int tm_commit(", "bool ot_ble_trigger_get(")
assert "const bool raw_active=tm_gpio_input_mask()!=0;" in save
assert "raw_active || live_input_mask!=0" in save
assert save.index("if(!tm_save(next))code=4") < save.index("ot_tm_saved_apply(")
assert "const uint8_t pins_after=tm_gpio_input_mask();" in save
assert "ot_tm_saved_apply(&tm_profile,&tm_deferred,next," in save
assert "om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask!=0" in save
assert "ot_om_apply_if_released(om_outputs,&om_deferred," in sampler
assert "ot_om_saved_apply(om_outputs,&om_deferred,next_all," in core
assert 'cJSON_AddBoolToObject(obj,"config_apply_pending",config_pending)' in core
assert "config_apply_pending" in readback
assert "tm_persistent && !tm_deferred.pending" in core
assert "ot_tm_apply_if_released(&tm_profile,&tm_deferred," in sampler
assert "instantaneous,physical" in sampler
assert "xSemaphoreGive(om_write_mutex)" in save
assert "require_revision&&tm_revision!=expected" in save
assert "tm_revision++;" in save
for begin, end in (("static esp_err_t om_post(", "bool ot_ble_config_get("),
                   ("int ot_ble_config_save(", "/* Normalized instantaneous GPIO levels")):
    saving = between(core, begin, end)
    assert "const bool raw_active=tm_gpio_input_mask()!=0;" in saving
    assert "raw_active || live_input_mask" in saving
    assert saving.index("tm_gpio_input_mask()") < saving.index("om_save(")
load = between(core, "static void tm_load(void){", "static bool tm_save(")
assert "ot_tm_defaults(&tm_profile);" in load
assert 'nvs_get_blob(h,"profile_v1",&stored,&size)' in load
assert "ot_tm_valid(&stored.profile)" in load
assert "if(tm_persistent)tm_profile=stored.profile" in load
persist = between(core, "static bool tm_save(", "#define OT_OM_STORAGE_MAGIC")
assert "nvs_set_blob(" in persist and "nvs_commit(h)" in persist
assert "tm_load();" in core

# A configured GPIO bank without a live sampler cannot safely authorize
# a durable trigger or output mapping save. The readiness latch is set
# after task creation and before either BLE or HTTP service starts.
boot = between(core, "void app_main(void) {", "\n}")
assert "static bool input_sampler_ready=false;" in core
assert "input_sampler_ready=sampler_ready;" in boot
assert boot.index("input_sampler_ready=sampler_ready;") < boot.index("ble_startup_ready=ot_ble_start();")
assert boot.index("input_sampler_ready=sampler_ready;") < boot.index("http_startup_ready=wifi_started&&http_start();")
for begin,end,persist in (
    ("static esp_err_t om_post(", "bool ot_ble_config_get(", "om_save(next_all)"),
    ("int ot_ble_config_save(", "/* Normalized instantaneous GPIO levels", "om_save(next_all)"),
    ("static int tm_commit(", "bool ot_ble_trigger_get(", "tm_save(next)"),
):
    op = between(core,begin,end)
    assert "if(!input_sampler_ready)" in op
    assert op.index("if(!input_sampler_ready)") < op.index(persist)
    assert "tm_gpio_input_mask()" in op

# 10: Combined CORE/effects/palette candidate retains physical trigger
# ownership of the final logical pixel. This is source ordering evidence,
# not measured LED output parity; standalone trigger #24 has no FX compositor.
assert "ot_palette_runtime_snapshot(" in renderer
assert "ot_fx_overlay_apply(" in renderer
assert "ot_effects_1d_render(" in renderer
assert (renderer.index("ot_palette_runtime_snapshot(") <
        renderer.index("ot_effects_1d_render(") <
        renderer.index("ot_fx_overlay_apply(") <
        renderer.index("ot_om_trigger_overlay(role,combined,") <
        renderer.index("ot_frame_snapshot_pixel(") <
        renderer.index("ot_om_swizzle(c,output->color_order)") <
        renderer.index("led_strip_set_pixel("))
assert "slot->overlay.style" in renderer
assert "ot_arbitrate(combined," in renderer
assert "native_rendering=decision.show_requested;" in renderer
assert "if(!trigger_here){" in renderer
assert "if(decorated)c=ot_om_calibrate(c,output);" in renderer
assert "if(trigger_here)c=ot_om_calibrate(" not in renderer
# Palette mutation must share the existing physical-input save veto,
# and unauthorized sessions cannot retain a partial palette transfer.
mutation=between(core,"bool ot_ble_palette_mutation_begin(void){",
                 "bool ot_ble_palette_in_use(")
for guard in ("input_sampler_ready", "om_write_mutex", "tm_gpio_input_mask()!=0",
              "om_deferred.pending", "tm_deferred.pending", "live_input_mask!=0"):
    assert guard in mutation, guard
assert "ot_palette_ble_session_reset();" in revoke
assert gap.count("ot_palette_ble_session_reset();")==2
assert "ot_palette_ble_session_reset();" in ble
# Trigger BLE ABI stays v1 and remains owner-guarded during palette v3 work.
assert 'OT_TM_BLE_READ_BYTES' in gatt
assert 'OT_TM_BLE_WRITE_BYTES' in gatt
assert '.uuid=&ot_trigger_uuid.u,.access_cb=trigger_access' in ble

# 10–11: hardware proof remains explicitly required, not encoded as a host PASS.
assert "OT_INPUT_ACTIVE_LOW 1" in board  # provisional bench assumption
assert "BENCH ONLY" in core or "BENCH ONLY" in board
print("Trigger runtime source contract: 11 responsibility boundaries verified (source/host only).")
