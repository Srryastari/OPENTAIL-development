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
gate = "if(full_frame && om_write_mutex && ble_startup_ready && http_startup_ready &&"
confirm = "ot_ota_confirm_boot(true)"
assert led.index(done) < led.index(all_lanes) < led.index(gate) < led.index(confirm)
assert "ot_ble_radio_ever_ready() && !boot_frame_verified)" in led.split(gate, 1)[1].split(confirm, 1)[0]
assert confirm not in startup, "Creating the FreeRTOS render task cannot validate OTA"
assert "boot_frame_verified=ot_ota_confirm_boot(true);" in led.split(gate, 1)[1]
assert "ble_startup_ready=ot_ble_start();" in startup
assert 'if(!om_write_mutex)ot_st_set_fault("Output Manager save mutex allocation failed");' in startup
assert 'if(!om_write_mutex){ESP_LOGE(TAG,"Output Manager save mutex allocation failed");abort();}' not in startup
assert "if(!om_write_mutex || xSemaphoreTake(" in core
assert startup.index("om_write_mutex=xSemaphoreCreateMutex();") < startup.index("xTaskCreate(led_task")
assert "http_startup_ready=wifi_started&&http_start();" in startup
assert "if(!http_startup_ready)ot_st_set_fault(" in startup
assert startup.index("http_startup_ready=wifi_started&&http_start();") < startup.index("xTaskCreate(led_task")
# The diagnostic HTTP service must never take down physical lighting on
# registration failure. OTA acceptance still requires service startup.
http = core.split("static bool http_start(void) {",1)[1].split("static bool wifi_ap_init(void)",1)[0]
wifi = core.split("static bool wifi_ap_init(void) {",1)[1].split("static bool input_init(void)",1)[0]
assert "ESP_ERROR_CHECK(" not in wifi
assert "abort();" not in wifi
assert "return false;" in wifi and "return true;" in wifi
assert "if(!esp_netif_create_default_wifi_ap())" in wifi
# Trigger GPIO setup is a fail-closed prerequisite for physical rendering.
# Bad input pin configuration must not panic-loop or emit lighting based on
# untrusted GPIO levels. Diagnostic Wi-Fi/BLE may still start for bench repair.
input_init = core.split("static bool input_init(void) {",1)[1].split(
    "static bool strip_configure_channel(",1
)[0]
assert "ESP_ERROR_CHECK(gpio_config(" not in input_init
assert "const esp_err_t e=gpio_config(&cfg);" in input_init
assert "return false;" in input_init and "return true;" in input_init
assert "const bool inputs_ready=input_init();" in startup
assert 'if(!inputs_ready)ot_st_set_fault("physical trigger GPIO initialization failed");' in startup
assert "const bool sampler_ready=inputs_ready &&" in startup
assert "bool outputs_ready=sampler_ready&&strip_init();" in startup
assert startup.index("const bool inputs_ready=input_init();") < startup.index("xTaskCreate(led_task")
assert "const bool wifi_started=wifi_ap_init();" in startup
assert 'if(!wifi_started)ot_st_set_fault("Wi-Fi diagnostic AP startup failed");' in startup
assert "http_startup_ready=wifi_started&&http_start();" in startup
assert startup.index("const bool wifi_started=wifi_ap_init();") < startup.index("xTaskCreate(led_task")
assert "ESP_ERROR_CHECK(httpd_start" not in http
assert "ESP_ERROR_CHECK(httpd_register_uri_handler" not in http
assert "httpd_stop(server);" in http
assert "return false;" in http and "return true;" in http
assert 'ot_st_set_fault("BLE GATT startup failed")' in startup
assert startup.index("ble_startup_ready=ot_ble_start();") < startup.index("xTaskCreate(led_task")
ble = (Path(__file__).resolve().parents[1] / "main/ble_status.c").read_text()
assert "bool ot_ble_start(void)" in ble
assert "bool ot_ble_radio_ever_ready(void)" in ble
# Physical enrollment must not read GPIO0 if its input configuration failed.
# Preserve existing-owner HMAC connectivity; only new key disclosure fails closed.
assert "static bool boot_enrollment_ready=false;" in ble
enroll=ble.split("static int owner_enroll_read(",1)[1].split("static int owner_challenge_read(",1)[0]
assert "conn!=active_conn||!boot_enrollment_ready||gpio_get_level(GPIO_NUM_0)!=0" in enroll
start_ble=ble.split("bool ot_ble_start(void){",1)[1].split("#else",1)[0]
assert "const esp_err_t boot_err=gpio_config(&boot);" in start_ble
assert "boot_enrollment_ready=boot_err==ESP_OK;" in start_ble
assert 'ot_st_set_fault("BLE BOOT enrollment button unavailable");' in start_ble
assert 'boot_enrollment_ready?"available":"disabled"' in start_ble
assert "__atomic_load_n(&ble_radio_ever_ready,__ATOMIC_ACQUIRE)" in ble
assert "__atomic_store_n(&ble_radio_ever_ready,true,__ATOMIC_RELEASE)" in ble
assert ble.index("ble_gap_adv_start(addr_type,") < ble.index("__atomic_store_n(&ble_radio_ever_ready,true")
assert "else __atomic_store_n(&ble_radio_ever_ready,true,__ATOMIC_RELEASE);" in ble
assert "__atomic_store_n(&ble_radio_ever_ready,false,__ATOMIC_RELEASE);" in ble
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
