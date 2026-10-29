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
gate = "if(full_frame && ble_startup_ready && http_startup_ready && !boot_frame_verified)"
confirm = "ot_ota_confirm_boot(true)"
assert led.index(done) < led.index(all_lanes) < led.index(gate) < led.index(confirm)
assert confirm not in startup, "Creating the FreeRTOS render task cannot validate OTA"
assert "boot_frame_verified=ot_ota_confirm_boot(true);" in led.split(gate, 1)[1]
assert "ble_startup_ready=ot_ble_start();" in startup
assert "http_startup_ready=http_start();" in startup
assert "if(!http_startup_ready)ot_st_set_fault(" in startup
assert startup.index("http_startup_ready=http_start();") < startup.index("xTaskCreate(led_task")
# The diagnostic HTTP service must never take down physical lighting on
# registration failure. OTA acceptance still requires service startup.
http = core.split("static bool http_start(void) {",1)[1].split("static void wifi_ap_init(void)",1)[0]
assert "ESP_ERROR_CHECK(httpd_start" not in http
assert "ESP_ERROR_CHECK(httpd_register_uri_handler" not in http
assert "httpd_stop(server);" in http
assert "return false;" in http and "return true;" in http
assert 'ot_st_set_fault("BLE GATT startup failed")' in startup
assert startup.index("ble_startup_ready=ot_ble_start();") < startup.index("xTaskCreate(led_task")
ble = (Path(__file__).resolve().parents[1] / "main/ble_status.c").read_text()
assert "bool ot_ble_start(void)" in ble
assert 'if(rc){token_ready=false;ESP_LOGE(TAG,"BLE service registration failed: %d",rc);return false;}' in ble
assert "boot_frame_verified=true;" not in led, "A failed flash confirmation must retry"
ota = (Path(__file__).resolve().parents[1] / "main/ot_ota_update.c").read_text()
assert "bool ot_ota_confirm_boot(bool outputs_ready)" in ota
assert "if(!outputs_ready)return false;" in ota
assert "return e==ESP_OK;" in ota
assert "if(state!=ESP_OTA_IMG_PENDING_VERIFY)return true;" in ota
# Factory/non-OTA images have no OTA rollback state: don't poll flash every frame.
assert "running->subtype<ESP_PARTITION_SUBTYPE_APP_OTA_0" in ota
assert "running->subtype>ESP_PARTITION_SUBTYPE_APP_OTA_15)return true;" in ota
assert ota.index("running->subtype<ESP_PARTITION_SUBTYPE_APP_OTA_0") < ota.index("esp_ota_get_state_partition(running,&state)")
# The HTTP/BLE renderer-ready flag must not be raised during strip creation;
# it is raised by the render task only after its frame timer exists.
assert "native_outputs_ready=outputs_ready" not in startup
timer_ready = led.index("if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)")
renderer_ready = led.index("native_outputs_ready=true;")
assert timer_ready < renderer_ready < led.index("while(1){")
assert "if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)" in led
print("PASS: CORE OTA image confirmation requires first completed multi-output frame")
