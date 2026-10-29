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

show_stop = section("static esp_err_t show_stop(httpd_req_t *r){", "/* Provision an ephemeral")
assert "ble_fx_enabled=false" in show_stop and "ot_fx_bank_stop_all(&fx_bank)" in show_stop

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
