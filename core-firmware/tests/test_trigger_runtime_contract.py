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
logic_header = (root / "core_logic.h").read_text(encoding="utf-8")
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
    assert "const bool inputs_ready=watchdog_ready&&input_init();" in core
    assert "sampler_ready=input_watched &&" in core
assert ".mode=GPIO_MODE_INPUT" in init
assert ".pull_up_en=GPIO_PULLUP_ENABLE" in init
sampler = between(core, "static void input_sampler_task(void *arg){", "static void frame_wake_callback")
for token in ("pdMS_TO_TICKS(10u)", "gpio_get_level(input_pin[i])",
              "OT_INPUT_ACTIVE_LOW?(level==0):(level!=0)",
              "ot_debounce(&sampler,levels,OT_DEBOUNCE_TICKS)",
              "const uint8_t previous=live_input_mask;",
              "if(published!=previous){live_input_mask=published;target=render_task_handle;}",
              "if(target)xTaskNotifyGive(target);",
              "vTaskDelayUntil(&last_wake,period)"):
    assert token in sampler, f"Input sampler contract lost: {token}"
assert "in->ticks[i]>=stable_ticks" in logic
assert "in->settled[i]=in->pending[i]" in logic

# Issue121: ISR-published transition queues feed only Left/Right release holds.
# Absolute uint64 monotonic deadlines remove sampler-tick drift and edge aliasing.
assert "#define OT_TURN_RELEASE_HOLD_US UINT64_C(400000)" in logic_header
assert "ot_signal_debouncer_init" in logic
assert "ot_signal_debouncer_transition" in logic
assert "ot_signal_debouncer_sample" in logic
assert "ot_signal_debouncer_resync" in logic
assert "uint64_t release_deadline_us;" in logic_header
assert "edge_time_us>=state->release_deadline_us" in logic
assert "now_us>=state->release_deadline_us" in logic
assert "state->release_deadline_us=edge_time_us+OT_TURN_RELEASE_HOLD_US;" in logic
assert "static DRAM_ATTR ot_turn_edge_event_t turn_edge_events[2][OT_TURN_EDGE_QUEUE_CAPACITY]" in core
assert "#define OT_TURN_EDGE_QUEUE_CAPACITY 64u" in core
assert "static DRAM_ATTR volatile uint32_t turn_edge_head[2]" in core
assert "static DRAM_ATTR volatile uint32_t turn_edge_tail[2]" in core
assert "static DRAM_ATTR volatile uint32_t turn_edge_overflow[2]" in core
assert "static DRAM_ATTR volatile uint32_t turn_edge_writer_active[2]" in core
# The ISR must guard the entire timestamp+GPIO pair before publication.
# A stale pre-guard GPIO read paired with a new timestamp can cross 400ms.
isr = between(core, "static void IRAM_ATTR input_edge_isr(void *arg){",
              "void ot_input_diag_snapshot_get(")
guard = isr.index("ot_isr_dram_store_release(&turn_edge_writer_active[slot],1u)")
clock = isr.index("const uint64_t edge_time_us=is_turn?(uint64_t)esp_timer_get_time():0u;")
gpio = isr.index("level=gpio_ll_get_level(&GPIO,(uint32_t)input_pin_isr[input_index])")
payload_time = isr.index("event->timestamp_us=edge_time_us;")
payload_gpio = isr.index("event->asserted=asserted;")
publish = isr.index("ot_isr_dram_store_release(&turn_edge_head[slot],head+1u)")
unguard = isr.index("ot_isr_dram_store_release(&turn_edge_writer_active[slot],0u)")
assert guard < clock < gpio < payload_time < payload_gpio < publish < unguard
# Unlike IDF's general __atomic_* fallback with external PSRAM enabled,
# this ISR must use only force-inlined native S32C1I on DRAM_ATTR words.
assert '#include "xt_utils.h"' in core
assert "xt_utils_compare_and_set(word,previous,next)" in core
assert "static inline __attribute__((always_inline))" in core
assert "ot_isr_dram_fence();" in core
assert "__atomic_" not in isr, "ISR cannot call PSRAM-aware libc atomic fallbacks"
for helper in ("ot_isr_dram_fetch_add","ot_isr_dram_fetch_or",
               "ot_isr_dram_fetch_and","ot_isr_dram_store_release",
               "ot_isr_dram_load_acquire","ot_isr_dram_cas"):
    assert helper in isr or helper=="ot_isr_dram_cas",helper

assert "if(is_turn)" in isr
assert "event->timestamp_us=edge_time_us;" in core
assert "event->asserted=asserted;" in core
assert "ot_isr_dram_store_release(&turn_edge_head[slot],head+1u)" in core
assert "__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE)" in core
assert "__atomic_load_n(&turn_edge_tail[slot],__ATOMIC_ACQUIRE)" in core
assert "__atomic_store_n(&turn_edge_tail[slot],tail,__ATOMIC_RELEASE)" in core
assert "turn_edge_overflow[slot]" in core
assert "gpio_ll_get_level(&GPIO,(uint32_t)input_pin_isr[input_index])" in core
assert "ot_isr_dram_store_release(&turn_edge_writer_active[slot],1u)" in core
assert "ot_isr_dram_store_release(&turn_edge_writer_active[slot],0u)" in core
assert "static DRAM_ATTR gpio_num_t input_pin_isr[OT_IN_COUNT]" in core
assert "input_pin_isr[i]=input_pin[i];" in core
assert "input_boot_snapshot_us=(uint64_t)esp_timer_get_time();" in core
assert "static bool turn_edge_queue_update(" in core
assert "ot_signal_debouncer_transition(state,event.asserted,event.timestamp_us)" in core
assert "static bool turn_edge_queue_resync(" in core
assert "__atomic_load_n(&turn_edge_writer_active[slot],__ATOMIC_ACQUIRE)" in core
assert "const uint32_t head_before=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);" in core
assert "const uint32_t head_after=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);" in core
assert "const int level=gpio_get_level(input_pin[slot+2u]);" in core
assert "__atomic_store_n(&turn_edge_tail[slot],head_after,__ATOMIC_RELEASE)" in core
assert "ot_signal_debouncer_resync(state,raw);" in core
assert "ot_signal_debouncer_sample(state,baseline_us,OT_DEBOUNCE_TICKS)" in core
assert "ot_signal_debouncer_sample(state,now_us,OT_DEBOUNCE_TICKS)" in core
assert "head_after!=head" in core
assert "bool left_valid=false,right_valid=false;" in sampler
assert "if(left_valid&&right_valid)" in sampler
assert "const uint8_t previous=live_input_mask;" in sampler
assert "if(left_valid){\n            if(left)published|=OT_SIG_LEFT;" in sampler
assert "else{\n            published=(uint8_t)((published&(uint8_t)~OT_SIG_LEFT)|" in sampler
assert "if(right_valid){\n            if(right)published|=OT_SIG_RIGHT;" in sampler
assert "else{\n            published=(uint8_t)((published&(uint8_t)~OT_SIG_RIGHT)|" in sampler
assert "if(published!=previous){live_input_mask=published;target=render_task_handle;}" in sampler
assert "if(left_valid&&right_valid)\n            ot_sw_idf_input_sampled();" in sampler
assert "if(target)xTaskNotifyGive(target);" in sampler
assert "ot_signal_debouncer_t turn_signal[2];" in sampler
assert "ot_signal_debouncer_init(&turn_signal[0],(boot_physical&OT_SIG_LEFT)!=0u);" in sampler
assert "ot_signal_debouncer_init(&turn_signal[1],(boot_physical&OT_SIG_RIGHT)!=0u);" in sampler
assert "turn_edge_queue_update(0u,&turn_signal[0]" in sampler
assert "turn_edge_queue_update(1u,&turn_signal[1]" in sampler
assert "read_turn_input_snapshot" not in core
assert "uint8_t published=(uint8_t)(physical&" in sampler
assert "if(left_valid){\n            if(left)published|=OT_SIG_LEFT;" in sampler
assert "if(right_valid){\n            if(right)published|=OT_SIG_RIGHT;" in sampler
assert "const uint8_t previous=live_input_mask;" in sampler
assert sampler.index("if(published!=previous){live_input_mask=published;target=render_task_handle;}") < sampler.index("ot_sw_idf_input_sampled();")
assert sampler.index("portEXIT_CRITICAL(&state_lock);") < sampler.index("ot_sw_idf_input_sampled();")
assert sampler.index("ot_sw_idf_input_sampled();") < sampler.index("if(target)xTaskNotifyGive(target);")
assert "millis()" not in logic and "Arduino.h" not in logic_header

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
assert "if(before_deadline(now,sim_deadline)){" in renderer
assert "combined|=sim_mask;fallback_mask|=sim_mask;" in renderer
assert "pixel_zone_map[ch][px]" in renderer
assert "pixel_logical_map[ch][px]" in renderer
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
assert renderer.index("if(render_pixels[ch]>longest_pixels)",completion) > completion
assert renderer.index("if(render_pixels[ch]>longest_pixels)",completion) < renderer.index("ot_fps_pacer_observe(&pacer,work_us,longest_pixels)")
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
stop_core = between(core, "static void core_stop_all_sources(void){",
                    "void ot_ble_stop_show(void){")
assert "core_stop_all_sources();" in stop
assert "sim_mask=0u;" in stop_core
assert "live_input_mask=" not in stop_core
assert "tm_gpio_input_mask" not in stop_core
assert "gpio_set_level" not in stop_core
assert "geometry_commit_active" in stop_core
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
show_stop = between(core, "static esp_err_t show_stop(httpd_req_t *r){",
                  "/* Provision an ephemeral")
assert show_stop.index("portEXIT_CRITICAL(&state_lock);") < show_stop.index(
    "request_render_refresh();")
assert "live_input_mask=0" not in show_stop
stop_post = between(core, "static esp_err_t stop_post(httpd_req_t *r) {",
                    "/* CORE ships only its own")
assert "core_stop_all_sources();" in stop_post
stop_core = between(core, "static void core_stop_all_sources(void){",
                    "void ot_ble_stop_show(void){")
assert stop_core.index("portEXIT_CRITICAL(&state_lock);") < stop_core.index(
    "request_render_refresh();")
assert "live_input_mask=0" not in stop_core
ble_stop = between(core, "void ot_ble_stop_show(void){", "/* Eight opt-in named-by-slot")
assert "core_stop_all_sources();" in ble_stop
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


# 9: active/staged journal protects power-loss and held-input reboot.
# NVS writes never happen inside the 10ms sampler; the promoter requires
# a stable GPIO any-edge epoch and independent four-sample idle window.
raw = between(core, "static uint8_t tm_gpio_input_mask(void){",
              "/* Serialize authenticated BLE")
assert "gpio_get_level(input_pin[i])" in raw
assert "OT_INPUT_ACTIVE_LOW?!high:high" in raw
save = between(core, "static int tm_commit(", "bool ot_ble_trigger_get(")
assert "const bool raw_active=tm_gpio_input_mask()!=0;" in save
assert "raw_active || live_input_mask!=0" in save
assert save.index("if(!tm_save(next))code=4") < save.index("tm_deferred.profile=*next;")
assert "tm_deferred.pending=true;" in save
assert "ot_tm_saved_apply(&tm_profile,&tm_deferred,next," not in save
assert "om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask!=0" in save
assert "ot_om_apply_if_released(om_outputs,&om_deferred," not in sampler
assert "ot_om_saved_apply(om_outputs,&om_deferred,next_all," not in core
# Issue #126 v4 stores hardware-only lane records and map-only geometry records
# in their dedicated namespaces. The legacy Output Manager is a compatibility
# projection; accepted geometry saves stage the real cross-namespace pair first.
assert "pz_deferred.candidate=*candidate;pz_deferred.pending=true;" in core
assert "ot_pz_pair_store_begin(&pz_pair_store_io,&pz_pair_workspace,\n        previous,candidate," in core
assert "ot_pz_pair_store_finish(&pz_pair_store_io,workspace,transaction_id," in core
assert "OT_PZ_PAIR_NS_OUTPUT_CFG" in core and '"ot_zone_map"' in core
# Legacy BLE Save now reuses the existing mutex-protected pair-store workspace
# rather than consuming another full controller object on the NimBLE host stack.
assert "ot_pz_controller_t *const candidate=&pz_pair_workspace.candidate;" in core
assert "pz_save_controller_locked(candidate)" in core
assert 'cJSON_AddBoolToObject(obj,"config_apply_pending",config_pending)' in core
assert "config_apply_pending" in readback
assert "tm_persistent && !tm_deferred.pending" in core
assert "ot_tm_apply_if_released(&tm_profile,&tm_deferred," not in sampler
assert "ot_tm_apply_if_released(&tm_profile,&tm_deferred," in core
assert "if(input_quiet_samples<OT_DEBOUNCE_TICKS)input_quiet_samples++;" in sampler
assert "config_activation_task" in core and "input_edge_isr" in core
assert "config_activation_idle(epoch,false)" in core
assert "config_activation_idle(epoch,true)" in core
assert "ot_tm_journal_recover(" in core and "ot_om_journal_recover(" in core
assert "xSemaphoreGive(om_write_mutex)" in save
assert "require_revision&&tm_revision!=expected" in save
assert "tm_revision++;" in save
for begin, end, persist in (
    ("static esp_err_t om_post(", "bool ot_ble_config_get(", "om_save("),
    ("int ot_ble_config_save(", "/* Normalized instantaneous GPIO levels",
     "pz_save_controller_locked(candidate)"),
):
    saving = between(core, begin, end)
    assert "const bool raw_active=tm_gpio_input_mask()!=0;" in saving
    assert re.search(r"raw_active\s*\|\|\s*live_input_mask",saving)
    assert saving.index("tm_gpio_input_mask()") < saving.index(persist)
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
    ("int ot_ble_config_save(", "/* Normalized instantaneous GPIO levels",
     "pz_save_controller_locked(candidate)"),
    ("static int tm_commit(", "bool ot_ble_trigger_get(", "tm_save(next)"),
):
    op = between(core,begin,end)
    assert "if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault)" in op
    assert op.index("if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault)") < op.index(persist)
    assert "tm_gpio_input_mask()" in op

# Combined CORE #38 + Effects #39 final draw-order / role-isolation fence.
# Host source assertions prevent a future FX port from drawing *over* a
# physical trigger. They do not certify RMT timing or 12V optocouplers.
for term in ("ot_palette_runtime_snapshot(", "ot_wled_palette_global_tick(",
             "ot_effects_1d_render(", "ot_fx_overlay_apply(",
             "ot_om_trigger_overlay(role,combined,",
             "ot_frame_snapshot_pixel(", "ot_om_swizzle(c,output->color_order)",
             "led_strip_set_pixel("):
    assert renderer.count(term)==1, term
order = ["ot_palette_runtime_snapshot(", "ot_wled_palette_global_tick(",
         "ot_effects_1d_render(", "ot_fx_overlay_apply(",
         "ot_om_trigger_overlay(role,combined,",
         "ot_frame_snapshot_pixel(", "ot_om_swizzle(c,output->color_order)",
         "led_strip_set_pixel("]
assert [renderer.index(x) for x in order]==sorted(renderer.index(x) for x in order)
assert "native_rendering=decision.show_requested;" in renderer
assert "if(!trigger_here){" in renderer
assert "if(decorated)c=ot_om_calibrate(c,output);" in renderer
assert "if(trigger_here)c=ot_om_calibrate(" not in renderer
assert "slot->overlay.style" in renderer
# No universal car/body location assumptions; the authoritative Panel/Zone
# pixel map determines each physical pixel's logical zone and local index.
assert "ot_pz_build_pixel_map(&local_panel[ch],pixel_zone_map[ch],pixel_logical_map[ch])" in renderer
assert "pixel_zone_map[ch][px]" in renderer
assert "pixel_logical_map[ch][px]" in renderer
assert "for(unsigned px=0;px<render_pixels[ch];++px)" in renderer
assert "local_tm=tm_profile;" in renderer
assert "combined=ot_tm_resolve(&local_tm,physical);" in renderer

# BLE and palette-v3 additions cannot bypass trigger save admission or retain
# in-flight palette mutation across invalid owner reauthorization / disconnect.
mutation=between(core, "bool ot_ble_palette_mutation_begin(void){",
                 "bool ot_ble_palette_in_use(")
for term in ("input_sampler_ready", "om_write_mutex",
             "tm_gpio_input_mask()!=0", "om_deferred.pending",
             "tm_deferred.pending", "live_input_mask!=0"):
    assert term in mutation, term
assert "if(busy)xSemaphoreGive(om_write_mutex);" in mutation
assert "if(changed)request_render_refresh();" in mutation
assert "ot_palette_ble_session_reset();" in revoke
assert gap.count("ot_palette_ble_session_reset();")==2
# The palette-v3 GATT must not change the existing 11-byte trigger write
# or 15-byte owner-authorized trigger readback.
assert "OT_TM_BLE_WRITE_BYTES (3u+OT_IN_COUNT)" in (
    root / "trigger_manager.h").read_text()
assert "OT_TM_BLE_READ_BYTES (7u+OT_IN_COUNT)" in (
    root / "trigger_manager.h").read_text()
assert "ot_tm_ble_decode(" in gatt and "ot_tm_ble_encode(" in gatt

# P1 #45 trigger policy retains its own journal and release-gated RAM publish.
trigger_promotion=between(core,"static void tm_promote(uint32_t epoch){",
                          "static void pz_promote(uint32_t epoch){")
active_write=trigger_promotion.index("config_write_active(h,")
stage_erase=trigger_promotion.index("config_clear_stage(h)")
after_erase_idle=trigger_promotion.index("config_activation_idle(epoch,false)",stage_erase)
ram_apply=trigger_promotion.index("ot_tm_apply_if_released(")
assert active_write < stage_erase < after_erase_idle < ram_apply
for token in ("if(!config_clear_stage(h))","config_recovery_fault=true;",
              "tm_gpio_input_mask()==0u",
              "__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch",
              "input_quiet_samples>=OT_DEBOUNCE_TICKS",
              "stage.previous_revision","stage.candidate_revision"):
    assert token in trigger_promotion, token

# Issue #126 v4 runtime promotion finishes the coordinated lane/map journal,
# verifies both active records, and publishes only the joined candidate after
# a fresh quiet-input/epoch check. No combined v3 runtime promotion remains.
panel_promotion=between(core,"static void pz_promote(uint32_t epoch){",
                        "static void config_activation_task(void *unused){")
for token in ("ot_pz_pair_store_finish","ot_pz_pair_store_snapshot",
              "ot_pz_pair_store_recover","ot_pz_join",
              "config_activation_idle(epoch,true)",
              "tm_gpio_input_mask()==0u","live_input_mask==0u",
              "__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch",
              "input_quiet_samples>=OT_DEBOUNCE_TICKS",
              "pz_outputs=workspace->pending.candidate",
):
    assert token in panel_promotion, token
assert "nvs_open(" not in panel_promotion
assert "config_recovery_fault=true;" in panel_promotion
panel_finish=panel_promotion.index("ot_pz_pair_store_finish")
panel_snapshot=panel_promotion.index("ot_pz_pair_store_snapshot")
panel_recover=panel_promotion.index("ot_pz_pair_store_recover")
panel_quiet=panel_promotion.rindex("if(!config_activation_idle(epoch,true))")
panel_publish=panel_promotion.index("pz_outputs=workspace->pending.candidate")
assert panel_finish<panel_snapshot<panel_recover<panel_quiet<panel_publish

panel_recovery=between(core,"static void pz_load(void){",
                       "static bool pz_save_controller_locked(")
for token in ("ot_pz_pair_store_snapshot","ot_pz_pair_store_recover",
              "ot_pz_pair_store_begin","pz_load_legacy_snapshot",
              "v4_authoritative","pz_pair_commit_pending"):
    assert token in panel_recovery, token
assert "Corrupt v3 data never falls back to legacy embedded zones" not in panel_recovery
assert "static void om_promote(" not in core
# On power-loss recovery, an active-only journal still restores previous
# mapping/geometry and holds the candidate pending; stage deletion does
# not authorize a fresh boot to change Brake/Turn/Reverse behavior.
journal=(root / "config_journal.c").read_text(encoding="utf-8")
for token in ("*live=r->previous;", "pending->profile=r->candidate;",
              "memcpy(live,r->previous", "memcpy(pending->outputs,r->candidate",
              "pending->pending=staged!=NULL ||"):
    assert token in journal, token

trigger_recovery=between(core,"static void tm_journal_load(void){",
                         "static bool tm_save(")
output_recovery=between(core,"static void om_journal_load(void){",
                        "static bool om_save(")
for token in ("tm_active_revision=r->previous_revision;",
              "tm_revision=r->candidate_revision;"):
    assert token in trigger_recovery, token
for token in ("om_active_revision,r->previous_revision",
              "om_revision,r->candidate_revision"):
    assert token in output_recovery, token
assert 'cJSON_AddNumberToObject(root,"active_config_revision",active_config_revision)' in readback
assert 'cJSON_AddBoolToObject(root,"config_apply_pending",pending)' in readback
assert "tm_revision==UINT16_MAX" in core
assert "pz_revision[index]==UINT16_MAX" in core
assert 'expected_config_revision required' in core
# No wire-layout changes: the same authenticated 11-byte trigger mapping write
# and 15-byte active-profile readback continue to use the durable edit token.
assert "OT_TM_BLE_WRITE_BYTES (3u+OT_IN_COUNT)" in (root/"trigger_manager.h").read_text()
assert "OT_TM_BLE_READ_BYTES (7u+OT_IN_COUNT)" in (root/"trigger_manager.h").read_text()

# 10–11: hardware proof remains explicitly required, not encoded as a host PASS.
assert "OT_INPUT_ACTIVE_LOW 1" in board  # provisional bench assumption
assert "BENCH ONLY" in core or "BENCH ONLY" in board
print("Trigger runtime source contract: 11 responsibility boundaries verified (source/host only).")
