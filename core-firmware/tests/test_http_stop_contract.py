#!/usr/bin/env python3
"""Source-level regression guard for the CORE HTTP Stop and capability contract.

This does not exercise the ESP32 HTTP server or physical LEDs. Host C tests and
the candidate bench flash must validate runtime behavior separately.
"""
import ast
import json
import re
from pathlib import Path

core = Path("main/opentail_core.c").read_text()
outputs = Path("main/output_manager.c").read_text()

def section(start, end):
    assert start in core and end in core, f"Missing endpoint: {start}"
    return core.split(start, 1)[1].split(end, 1)[0]

stop = section("static esp_err_t stop_post(httpd_req_t *r) {", "extern const uint8_t ot_dev_console_start[]")
for required in (
    "demo.mode=OT_DEMO_OFF",
    "sim_mask=0",
    "ble_owns_show=false",
    "ble_fx_enabled=false",
    "ot_fx_bank_stop_all(&fx_bank)",
    "native_show.mode=OT_SHOW_OFF",
    "stop_request_generation++",
    "ot_st_show_active(false)",
):
    assert required in stop, f"Global Stop must cancel all preview sources: {required}"
assert stop.index("portENTER_CRITICAL(&state_lock)") < stop.index("ot_fx_bank_stop_all(&fx_bank)") < stop.index("portEXIT_CRITICAL(&state_lock)")

# Diagnostic countdown is derived from each live CORE FX zone, not the
# last temporary show deadline. Standalone NVS scenes have no TTL.
show_get = section("static esp_err_t show_get(httpd_req_t *r){", "static esp_err_t show_start(")
assert 'ot_fx_slot_active(slot,ms)' in show_get
assert 'if(slot->standalone){persistent=true;continue;}' in show_get
assert 'const uint32_t elapsed=ms-slot->started_ms;' in show_get
assert 'if(duration-elapsed>remaining)remaining=duration-elapsed;' in show_get
assert 'if(persistent)remaining=0;' in show_get
assert r'\"persistent\":%s' in show_get
assert 'remaining=active?' not in show_get

show_stop = section("static esp_err_t show_stop(httpd_req_t *r){", "/* Provision an ephemeral")
assert "ble_fx_enabled=false" in show_stop and "ot_fx_bank_stop_all(&fx_bank)" in show_stop

# A Stop ACK must schedule the next render immediately even when adaptive
# output FPS is very low. The renderer alone still writes physical LEDs;
# Stop completion telemetry still depends on successful RMT refresh.
wake = section("static void request_render_refresh(void){", "static uint8_t live_input_mask")
assert 'task=render_task_handle;' in wake
assert wake.index("portEXIT_CRITICAL(&state_lock);") < wake.index("xTaskNotifyGive(task);")
for body in (stop, show_stop):
    assert body.index("portEXIT_CRITICAL(&state_lock);") < body.index("request_render_refresh();")
ble_stop = section("void ot_ble_stop_show(void){", "static bool fx_slot_key(")
assert "if(refresh_requested)request_render_refresh();" in ble_stop
assert "else if(native_show.mode==OT_SHOW_OFF || !show_window(now))" in ble_stop
ble_disconnect = section("void ot_ble_disconnect_show(void){", "static esp_err_t status_get(")
assert "if(owned_before)request_render_refresh();" in ble_disconnect
assert ble_disconnect.index("portEXIT_CRITICAL(&state_lock);") < ble_disconnect.index("request_render_refresh();")
render = section("static void led_task(void *p) {", "void app_main(void) {")
assert "if(!native_rendering && full_frame)" in render
assert "stop_render_generation=local_stop_generation;" in render
assert "ulTaskNotifyTake(pdTRUE,timeout)" in render

# Display control changes should schedule an immediate render as Stop does.
# Do not wake for a rejected request or hold the state spinlock while doing it.
starts = (
    section("static esp_err_t show_start(httpd_req_t *r){", "static esp_err_t show_stop("),
    section("bool ot_ble_apply_show(const ot_ble_show_packet_t *request){",
            "/* Runtime control over BLE"),
    section("static bool ot_ble_apply_fx_internal(", "bool ot_ble_apply_fx("),
    section("bool ot_ble_play_fx_preset(uint8_t slot){",
            "bool ot_ble_delete_fx_preset("),
)
assert "if(!ready)return respond_err(r,409" in starts[0]
assert starts[0].index('if(!ready)return respond_err(r,409') < starts[0].index("request_render_refresh();")
for body in starts[1:]:
    assert "if(ready)request_render_refresh();" in body
    assert body.index("portEXIT_CRITICAL(&state_lock);") < body.index("if(ready)request_render_refresh();")
# One-shot legacy preset loads dispatch through the same FX function.
assert "ot_ble_apply_fx_internal(&legacy.fx,true,NULL)" in starts[3]

capabilities = section("static esp_err_t caps_get(httpd_req_t *r) {", "static esp_err_t status_led_get")
literal = re.search(r'return send_json\(r,(".*")\);', capabilities)
assert literal, "Capabilities must return valid static JSON"
caps = json.loads(ast.literal_eval(literal.group(1)))
assert "c->color_order=OT_OM_GRB;" in outputs
assert caps["color_order"] == "GRB", "First-boot color-order claim must match Output Manager"
assert caps["color_order_configurable"] is True
assert caps["wled_renderer"] == "compatibility"
assert caps["wled_pixel_parity_verified"] is False
assert caps["native_wled_firmware_unified"] is False
assert caps["hardware_failover"] is False and caps["bench_only"] is True
assert 'studio_gz_start' not in core, "CORE must not embed independent Studio"
assert '"/dev"' in core and 'dev_get' in core
assert 'fx_catalog_get' in core and 'fx_preview_post' in core
assert caps["ble_effect_id_count"] == 140, "CORE catalog must report native Startup/Shutdown effects"
assert caps["studio_embedded"] is False and caps["developer_console"] is True
print("CORE Stop, bench console routing, capabilities and independent Studio contract passed")
