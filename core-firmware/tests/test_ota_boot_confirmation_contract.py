#!/usr/bin/env python3
"""CORE source guard: an OTA candidate is not accepted before LED rendering.

Structural assertion only. Physical RMT output and real rollback need bench tests.
"""
from pathlib import Path

core = (Path(__file__).resolve().parents[1] / "main/opentail_core.c").read_text()
led = core.split("static void led_task(void *p) {", 1)[1].split("void app_main(void) {", 1)[0]
startup = core.split("void app_main(void) {", 1)[1]
done = "led_strip_refresh_async_done(strip[ch])"
all_lanes = "const bool full_frame=ot_frame_all_submitted(frame_submitted,OT_CHAIN_COUNT)"
gate = "if(full_frame && !boot_frame_verified)"
confirm = "ot_ota_confirm_boot(true)"
assert led.index(done) < led.index(all_lanes) < led.index(gate) < led.index(confirm)
assert confirm not in startup, "Creating the FreeRTOS render task cannot validate OTA"
assert "boot_frame_verified=ot_ota_confirm_boot(true);" in led.split(gate, 1)[1]
assert "boot_frame_verified=true;" not in led, "A failed flash confirmation must retry"
ota = (Path(__file__).resolve().parents[1] / "main/ot_ota_update.c").read_text()
assert "bool ot_ota_confirm_boot(bool outputs_ready)" in ota
assert "if(!outputs_ready)return false;" in ota
assert "return e==ESP_OK;" in ota
assert "if(state!=ESP_OTA_IMG_PENDING_VERIFY)return true;" in ota
# The HTTP/BLE renderer-ready flag must not be raised during strip creation;
# it is raised by the render task only after its frame timer exists.
assert "native_outputs_ready=outputs_ready" not in startup
timer_ready = led.index("if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)")
renderer_ready = led.index("native_outputs_ready=true;")
assert timer_ready < renderer_ready < led.index("while(1){")
assert "if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)" in led
print("PASS: CORE OTA image confirmation requires first completed multi-output frame")
