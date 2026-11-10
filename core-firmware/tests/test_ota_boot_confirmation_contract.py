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
gate = "if(full_frame && ot_sw_idf_stable() &&"
confirm = "ot_ota_confirm_boot(true)"
assert led.index(done) < led.index(all_lanes) < led.index(gate) < led.index(confirm)
assert "ot_ble_radio_ever_ready() && !boot_frame_verified)" in led.split(gate, 1)[1].split(confirm, 1)[0]
# Early renderer creation must not let an OTA candidate get confirmed before
# the independent BLE radio is actually advertising and the frame completes.
assert led.index("ot_sw_idf_stable()") < led.index("ot_ota_confirm_boot(true)")
assert confirm not in startup, "Creating the FreeRTOS render task cannot validate OTA"
assert "boot_frame_verified=ot_ota_confirm_boot(true);" in led.split(gate, 1)[1]
assert "ble_startup_ready=ot_ble_start();" in startup
assert 'if(!om_write_mutex)ot_st_set_fault("Output Manager save mutex allocation failed");' in startup
assert 'if(!om_write_mutex){ESP_LOGE(TAG,"Output Manager save mutex allocation failed");abort();}' not in startup
assert "if(!om_write_mutex || xSemaphoreTake(" in core
render_alloc = startup.index('led_task,"ot_led",OT_RENDER_TASK_STACK_BYTES,NULL,8,NULL')
assert render_alloc < startup.index("om_write_mutex=xSemaphoreCreateMutex();")
assert "http_startup_ready=wifi_started&&http_start();" in startup
assert "if(!http_startup_ready)ot_st_set_fault(" in startup
assert render_alloc < startup.index("http_startup_ready=wifi_started&&http_start();")
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
assert "const bool inputs_ready=watchdog_ready&&input_init();" in startup
assert 'if(!inputs_ready)ot_st_set_fault("physical GPIO or watchdog initialization failed");' in startup
assert "const bool sampler_ready=input_watched &&" in startup
assert "const bool outputs_ready=sampler_ready&&psram_ready&&!config_recovery_fault&&strip_init();" in startup
assert startup.index("const bool inputs_ready=watchdog_ready&&input_init();") < render_alloc
assert "const bool wifi_started=wifi_ap_init();" in startup
assert 'if(!wifi_started)ot_st_set_fault("Wi-Fi diagnostic AP startup failed");' in startup
assert "http_startup_ready=wifi_started&&http_start();" in startup
assert render_alloc < startup.index("const bool wifi_started=wifi_ap_init();")
assert "ESP_ERROR_CHECK(httpd_start" not in http
assert "ESP_ERROR_CHECK(httpd_register_uri_handler" not in http
assert "httpd_stop(server);" in http
assert "return false;" in http and "return true;" in http
assert 'ot_st_set_fault("BLE GATT startup failed")' in startup
assert render_alloc < startup.index("ble_startup_ready=ot_ble_start();")
# BLE-primary radio resource admission must precede the much larger Wi-Fi
# SoftAP buffer allocations, but OTA still needs real BLE advertising plus
# completed RMT frames and watchdog stability before confirmation.
assert startup.index("ble_startup_ready=ot_ble_start();") < startup.index(
    "const bool wifi_started=wifi_ap_init();")
assert startup.index("ble_startup_ready=ot_ble_start();") < startup.index(
    "xTaskCreate(config_activation_task")
assert startup.index("om_write_mutex=xSemaphoreCreateMutex();") < startup.index(
    "ble_startup_ready=ot_ble_start();")
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
assert "else{" in advertise
assert "__atomic_store_n(&ble_radio_ever_ready,true,__ATOMIC_RELEASE);" in advertise
assert "BLE advertising ACTIVE: OpenTail CORE" in advertise
assert advertise.index("ble_gap_adv_start(") < advertise.index(
    "__atomic_store_n(&ble_radio_ever_ready,true,__ATOMIC_RELEASE)")
assert "__atomic_store_n(&ble_radio_ever_ready,false,__ATOMIC_RELEASE);" in ble
assert 'if(rc){token_ready=false;ESP_LOGE(TAG,"BLE service registration failed: %d",rc);return false;}' in ble
# GAP callbacks must only clear the actual owner session. Disconnects
# from a rejected secondary central may never stop an owner's lighting show.
gap=ble.split("static int gap_event(struct ble_gap_event *ev,void *arg){",1)[1].split(
    "static void on_sync(void)",1
)[0]
advertise=ble.split("static void advertise(void){",1)[1].split(
    "static int gap_event(struct ble_gap_event *ev,void *arg){",1
)[0]
assert "if(active_conn!=BLE_HS_CONN_HANDLE_NONE)return;" in advertise
assert "if(active_conn!=BLE_HS_CONN_HANDLE_NONE){" in gap
assert "ble_gap_terminate(ev->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM)" in gap
assert "if(ev->disconnect.conn.conn_handle!=active_conn)return 0;" in gap
assert gap.index("if(ev->disconnect.conn.conn_handle!=active_conn)return 0;") < gap.index("ot_ble_disconnect_show();")
assert gap.index("if(ev->disconnect.conn.conn_handle!=active_conn)return 0;") < gap.index("active_conn=BLE_HS_CONN_HANDLE_NONE;")
assert "boot_frame_verified=true;" not in led, "A failed flash confirmation must retry"
ota = (Path(__file__).resolve().parents[1] / "main/ot_ota_update.c").read_text()
sdkconfig = (Path(__file__).resolve().parents[1] / "sdkconfig.defaults").read_text()
assert "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" in sdkconfig
assert "bool ot_ota_confirm_boot(bool outputs_ready)" in ota
assert "void ot_ota_watch_boot(void)" in ota
assert "if(!outputs_ready)return false;" in ota
assert "esp_ota_mark_app_valid_cancel_rollback()" in ota
assert "esp_ota_mark_app_invalid_rollback_and_reboot()" in ota
assert "esp_timer_start_once(boot_guard" in ota
assert "OTA_BOOT_CONFIRM_DEADLINE_MS 120000u" in (Path(__file__).resolve().parents[1] / "main/ot_ota_update.h").read_text()
assert "ot_ota_watch_boot();" in startup
assert startup.index("const bool watchdog_ready=ot_sw_idf_configure();") < startup.index("ot_ota_watch_boot();")
assert startup.index("ot_ota_watch_boot();") < startup.index("input_init()")
# Non-OTA, already-valid and NOT_FOUND states are not treated as pending candidates.
assert "ESP_ERR_NOT_FOUND" in ota
assert "image_state!=ESP_OTA_IMG_PENDING_VERIFY" in ota
assert "if(state==OTA_BOOT_EXPIRED)return false;" in ota
assert "esp_timer_stop(boot_guard)" in ota
# The HTTP/BLE renderer-ready flag must not be raised during strip creation;
# it is raised by the render task only after its frame timer exists.
assert "native_outputs_ready=outputs_ready" not in startup
timer_ready = led.index("if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)")
renderer_ready = led.index("native_outputs_ready=true;")
assert timer_ready < renderer_ready < led.index("while(1){")
assert "if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK)" in led
print("PASS: CORE OTA image confirmation requires first completed multi-output frame")
