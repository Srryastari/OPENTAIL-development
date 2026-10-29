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
entry = re.search(r"static (?:void|bool) input_init\\(void\\) \\{", core)
assert entry, "Trigger GPIO initializer missing"
init = core[entry.end():].split("/* RMT drivers", 1)[0]
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

# 9: save admission includes instant GPIO and settled GPIO, source-level
# ordering preserves RAM on NVS failure. No claim of atomicity DURING NVS.
raw = between(core, "static uint8_t tm_gpio_input_mask(void){",
              "/* Serialize authenticated BLE")
assert "gpio_get_level(input_pin[i])" in raw
assert "OT_INPUT_ACTIVE_LOW?!high:high" in raw
save = between(core, "static int tm_commit(", "bool ot_ble_trigger_get(")
assert "const bool raw_active=tm_gpio_input_mask()!=0;" in save
assert "raw_active || live_input_mask!=0" in save
assert save.index("if(!tm_save(next))code=4") < save.index("tm_profile=*next;")
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

# 10–11: hardware proof remains explicitly required, not encoded as a host PASS.
assert "OT_INPUT_ACTIVE_LOW 1" in board  # provisional bench assumption
assert "BENCH ONLY" in core or "BENCH ONLY" in board
print("Trigger runtime source contract: 11 responsibility boundaries verified (source/host only).")
