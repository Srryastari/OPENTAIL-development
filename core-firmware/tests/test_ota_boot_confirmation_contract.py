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
assert "boot_frame_verified=true;" in led.split(gate, 1)[1]
assert "if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)" in led
print("PASS: CORE OTA image confirmation requires first completed multi-output frame")
