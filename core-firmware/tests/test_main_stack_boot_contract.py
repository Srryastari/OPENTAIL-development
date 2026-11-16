#!/usr/bin/env python3
"""Guard the exact v0.1.32 ESP32-S3 main-task stack bootloop regression.

A passing host test does NOT establish physical boot. Serial must confirm
app_main advances past SPI status LED initialization and starts services.
"""
from pathlib import Path
import re

core = Path(__file__).resolve().parents[1]
defaults = (core / "sdkconfig.defaults").read_text(encoding="utf-8")
source = (core / "main/opentail_core.c").read_text(encoding="utf-8")
found = re.findall(r"^CONFIG_ESP_MAIN_TASK_STACK_SIZE=(\d+)$", defaults, re.M)
assert len(found) == 1, "main task stack size must be explicit"
assert int(found[0]) >= 8192, "ESP-IDF default main stack caused physical reboot loop"
assert "#if CONFIG_ESP_MAIN_TASK_STACK_SIZE < 8192" in source
boot = source.split("void app_main(void) {", 1)[1]
assert boot.index("om_journal_load()") < boot.index("ot_st_init()")
assert boot.index("tm_journal_load()") < boot.index("ot_st_init()")
assert boot.index("ot_st_init()") < boot.index("uxTaskGetStackHighWaterMark(NULL)")
assert boot.index("uxTaskGetStackHighWaterMark(NULL)") < boot.index("input_init()")
assert boot.index("input_init()") < boot.index("ot_ble_start()")
print("PASS: ESP32-S3 main task startup stack explicit, guarded, and observable")

# P0 real N16R8 device evidence: xTaskCreate(ot_led) failed after Wi-Fi,
# NimBLE, HTTP and the configuration worker started. Firmware host build did
# not exercise those memory allocations. Prove the fixed initialization order,
# guarded PSRAM configuration, and multi-capability heap observations.
required_config={
    "CONFIG_SPIRAM":"y",
    "CONFIG_SPIRAM_MODE_OCT":"y",
    "CONFIG_SPIRAM_SPEED_80M":"y",
    "CONFIG_SPIRAM_BOOT_INIT":"y",
    "CONFIG_SPIRAM_MEMTEST":"y",
    "CONFIG_SPIRAM_USE_MALLOC":"y",
    "CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP":"y",
    "CONFIG_FREERTOS_PLACE_TASK_STACKS_IN_EXT_RAM":"n",
    "CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL":"1024",
    "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL":"32768",
    "CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM":"6",
    "CONFIG_ESP_WIFI_RX_BA_WIN":"6",
    "CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM":"6",
    "CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM":"16",
    "CONFIG_ESP_WIFI_IRAM_OPT":"n",
    "CONFIG_ESP_WIFI_RX_IRAM_OPT":"n",
    "CONFIG_LWIP_IRAM_OPTIMIZATION":"n",
    "CONFIG_ESPTOOLPY_FLASHSIZE_16MB":"y",
}
for option,value in required_config.items():
    assert re.search(r"^"+option+r"="+value+r"$",defaults,re.M), (
        "N16R8 expected PSRAM/memory configuration missing: "+option)
assert '#include "esp_heap_caps.h"' in source
assert '#include "esp_psram.h"' in source
assert "N16R8 requires initialized octal PSRAM" in source
assert "OpenTail FreeRTOS stacks must stay in internal RAM" in source
assert "esp_psram_is_initialized()" in source
assert "esp_psram_get_size()" in source
assert "OT_N16R8_PSRAM_BYTES (8u * 1024u * 1024u)" in source
for fn in ("heap_caps_get_free_size", "heap_caps_get_largest_free_block",
           "heap_caps_get_minimum_free_size"):
    assert fn+"(internal_caps)" in source, "missing internal heap telemetry: "+fn
    assert fn+"(MALLOC_CAP_SPIRAM)" in source, "missing PSRAM telemetry: "+fn
for stage in ('early-boot','pre-render-task','post-render-task','pre-ble',
              'pre-wifi','post-wifi',
              'post-ble','post-http','render-create-failed'):
    assert 'ot_boot_heap_snapshot("'+stage+'")' in boot, stage
# Real pixel rendering is the priority: after GPIO ISR, sampler and configured
# RMT lanes, before service heap usage. It must not be launched in a
# network-service readiness branch or demoted below other tasks.
assert boot.index("input_init()") < boot.index("xTaskCreate(input_sampler_task")
assert boot.index("xTaskCreate(input_sampler_task") < boot.index("strip_init()")
assert boot.index("strip_init()") < boot.index('ot_boot_heap_snapshot("pre-render-task")')
assert boot.index('ot_boot_heap_snapshot("pre-render-task")') < boot.index("xTaskCreate(",
    boot.index('ot_boot_heap_snapshot("pre-render-task")'))
renderer_call=boot.index('led_task,"ot_led",OT_RENDER_TASK_STACK_BYTES,NULL,8,NULL')
assert boot.index("xTaskCreate(",boot.index('ot_boot_heap_snapshot("pre-render-task")')) < renderer_call
for other in ("xTaskCreate(config_activation_task","wifi_ap_init()",
              "ot_ble_start()","http_start()"):
    assert renderer_call < boot.index(other),(other,"allocated before render stack")
assert "#define OT_RENDER_TASK_STACK_BYTES 8192u" in source
assert "ot_boot_psram_ready()" in boot
assert "sampler_ready&&psram_ready&&!config_recovery_fault&&strip_init()" in boot
assert 'Renderer task READY: frame timer and single-ESP render watchdog registered' in source
assert "if(!ot_sw_idf_watch_render())" in source
assert boot.index("ble_startup_ready=ot_ble_start();") < boot.index(
    "xTaskCreate(config_activation_task")
assert boot.index("ble_startup_ready=ot_ble_start();") < boot.index(
    "const bool wifi_started=wifi_ap_init();")
assert "ot_sw_idf_watch_inputs()" in boot
print("PASS P0 N16R8 boot: octal 8MB PSRAM malloc + internal-only stacks, exact heap telemetry, renderer-before-BLE-before-Wi-Fi allocation order, fail-closed PSRAM guard, watchdog and stack sizes retained; HOST SOURCE CONTRACT (physical log required)")
