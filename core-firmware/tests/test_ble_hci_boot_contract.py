#!/usr/bin/env python3
"""P0 real N16R8 BLE HCI boot guard — read-only host source/config assertions.

Physical COM9 log at 815f52d1: renderer READY, Wi-Fi SoftAP READY,
then NimBLE 'hci inits failed' while internal free=47023, largest=31744
and minimum-ever free=272. These checks guard the BLE-first memory budget
and diagnostics. They do NOT claim actual BLE advertising or HCI allocation
success; an owner reboot/serial trace is mandatory.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
core = (root / "main/opentail_core.c").read_text(encoding="utf-8")
ble = (root / "main/ble_status.c").read_text(encoding="utf-8")
defaults = (root / "sdkconfig.defaults").read_text(encoding="utf-8")
boot = core.split("void app_main(void) {", 1)[1]
ble_start = ble.split("bool ot_ble_start(void){", 1)[1].split("#else", 1)[0]
adv = ble.split("static void advertise(void){", 1)[1].split(
    "static int gap_event(", 1)[0]

def at(token):
    assert token in boot, "missing startup step: " + token
    return boot.index(token)

renderer = at('led_task,"ot_led",OT_RENDER_TASK_STACK_BYTES,NULL,8,NULL')
mutex = at("om_write_mutex=xSemaphoreCreateMutex();")
ble_init = at("ble_startup_ready=ot_ble_start();")
promoter = at("xTaskCreate(config_activation_task")
wifi = at("const bool wifi_started=wifi_ap_init();")
http = at("http_startup_ready=wifi_started&&http_start();")
assert at("input_init()") < at("strip_init()") < renderer
assert renderer < mutex < ble_init < promoter < wifi < http, (
    "BLE primary HCI memory must be allocated before Wi-Fi and config worker")
assert at('ot_boot_heap_snapshot("pre-ble")') < ble_init
assert ble_init < at('ot_boot_heap_snapshot("post-ble")')
assert "esp_timer_get_time()+2000000LL" in boot
assert "while(!ot_ble_radio_ever_ready()" in boot
assert "vTaskDelay(pdMS_TO_TICKS(10));" in boot
assert 'BLE-primary advertising confirmed before Wi-Fi allocation' in boot
assert 'BLE HCI started but advertising not confirmed in 2000ms; Wi-Fi diagnostics still starting' in boot
assert ble_init < at("if(ble_startup_ready){") < promoter
assert at("if(ble_startup_ready){") < wifi
assert at('ot_boot_heap_snapshot("pre-wifi")') < wifi
assert wifi < at('ot_boot_heap_snapshot("post-wifi")')
assert at('ot_boot_heap_snapshot("post-wifi")') < http
assert "if(!ble_startup_ready)ot_st_set_fault(\"BLE GATT startup failed\")" in boot
assert 'if(!wifi_started)ot_st_set_fault("Wi-Fi diagnostic AP startup failed")' in boot
assert "if(!http_startup_ready)ot_st_set_fault(" in boot
assert "if(om_write_mutex)" in boot
assert "ot_sw_idf_watch_inputs()" in boot and "ot_sw_idf_watch_render()" in core
assert "sampler_ready&&psram_ready&&!config_recovery_fault&&strip_init()" in boot

# Inspect BOTH internal heap and DMA-capable internal heap at the HCI call,
# and record the function/capability/size of a failed allocation without
# calling ESP_LOG or malloc from inside the allocation callback itself.
for token in (
    '#include "esp_heap_caps.h"',
    '#include "esp_bt.h"',
    "heap_caps_register_failed_alloc_callback(ble_alloc_failed)",
    "ble_hci_heap_snapshot(\"before-nimble-port-init\")",
    "ble_hci_heap_snapshot(err==ESP_OK?",
    "esp_bt_controller_get_status()",
    "MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA",
    "MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT",
    "heap_caps_get_largest_free_block(dma)",
    "heap_caps_get_minimum_free_size(internal)",
    "ble_alloc_fail_count",
    "ble_alloc_fail_size",
    "ble_alloc_fail_caps",
    "ble_alloc_fail_caller",
):
    assert token in ble, token
hook = ble.split("static void IRAM_ATTR ble_alloc_failed(", 1)[1].split(
    "static void ble_hci_heap_snapshot(", 1)[0]
assert "ESP_LOG" not in hook and "malloc(" not in hook and "portENTER_CRITICAL" not in hook
assert ble_start.index("heap_caps_register_failed_alloc_callback") < ble_start.index(
    "nimble_port_init()")
assert ble_start.index('ble_hci_heap_snapshot("before-nimble-port-init")') < (
    ble_start.index("nimble_port_init()"))
assert ble_start.index("nimble_port_init()") < ble_start.index(
    'ble_hci_heap_snapshot(err==ESP_OK?')
assert "NimBLE HCI/controller init failed" in ble
assert "BLE advertising ACTIVE: OpenTail CORE" in adv
assert adv.index("ble_gap_adv_start(addr_type") < adv.index(
    "__atomic_store_n(&ble_radio_ever_ready,true")
assert "ble_hs_cfg.sync_cb=on_sync;" in ble_start
assert "nimble_port_freertos_init(host_task)" in ble_start
assert "owner_valid=owner_load();" in ble_start
assert "owner_security_config();ble_store_config_init();" in ble_start

config = dict(re.findall(r"^(CONFIG_[A-Z0-9_]+)=(\S+)$", defaults, re.M))
for key, expected in {
    "CONFIG_SPIRAM": "y",
    "CONFIG_SPIRAM_MODE_OCT": "y",
    "CONFIG_SPIRAM_MEMTEST": "y",
    "CONFIG_SPIRAM_USE_MALLOC": "y",
    "CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP": "y",
    "CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL": "1024",
    "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL": "32768",
    "CONFIG_FREERTOS_PLACE_TASK_STACKS_IN_EXT_RAM": "n",
    "CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM": "6",
    "CONFIG_ESP_WIFI_RX_BA_WIN": "6",
    "CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM": "6",
    "CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM": "16",
    "CONFIG_ESP_WIFI_IRAM_OPT":"n",
    "CONFIG_ESP_WIFI_RX_IRAM_OPT":"n",
    "CONFIG_LWIP_IRAM_OPTIMIZATION":"n",
    "CONFIG_BT_NIMBLE_ENABLED": "y",
    "CONFIG_BT_BLUEDROID_ENABLED": "n",
    "CONFIG_ESP_TASK_WDT_PANIC": "y",
    "CONFIG_ESPTOOLPY_FLASHSIZE_16MB": "y",
}.items():
    assert config.get(key) == expected, (key, config.get(key), expected)
assert int(config["CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM"]) >= int(
    config["CONFIG_ESP_WIFI_RX_BA_WIN"])
assert "conf.ap.max_connection=2;" in core
assert "esp_wifi_start()" in core and "httpd_register_uri_handler(" in core
assert "OT_RENDER_TASK_STACK_BYTES 8192u" in core
print("PASS P0 BLE startup: renderer→HCI→config→Wi-Fi→HTTP, allocation failure/DMA diagnostics, BLE advertising, bounded secondary SoftAP with protected internal stacks; HOST SOURCE CONTRACT ONLY")
