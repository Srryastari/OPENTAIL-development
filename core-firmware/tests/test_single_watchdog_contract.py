#!/usr/bin/env python3
"""Issue #113: structural PROGRESS-only TWDT source gate, not physical proof."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
adapter=(root/"main/ot_single_watchdog_idf.c").read_text()
head=(root/"main/ot_single_watchdog.h").read_text()
idfhead=(root/"main/ot_single_watchdog_idf.h").read_text()
cmake=(root/"main/CMakeLists.txt").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
partition=(root/"partitions_ota.csv").read_text()
host=(root/"tests/run-host-tests.sh").read_text()
def part(src,a,b):
    assert a in src and b in src
    return src.split(a,1)[1].split(b,1)[0]
assert all(x in cmake for x in ('"ot_single_watchdog.c"','"ot_single_watchdog_idf.c"'))
assert 'test_single_watchdog.c' in host
for token in ("CONFIG_ESP_TASK_WDT_EN=y","CONFIG_ESP_TASK_WDT_INIT=y",
              "CONFIG_ESP_TASK_WDT_PANIC=y","CONFIG_ESP_INT_WDT=y",
              "CONFIG_BOOTLOADER_WDT_ENABLE=y"):
    assert token in cfg,token
assert "esp_task_wdt_reconfigure(&settings)" in adapter
assert ".timeout_ms=15000u" in adapter and ".idle_core_mask=3u" in adapter
assert ".trigger_panic=true" in adapter
assert 'esp_task_wdt_add_user("ot-input-sampler"' in adapter
assert 'esp_task_wdt_add_user("ot-render-progress"' in adapter
assert "esp_task_wdt_reset_user(input_user)" in adapter
assert "esp_task_wdt_reset_user(render_user)" in adapter
assert "esp_task_wdt_deinit(" not in adapter
assert "RTC_NOINIT_ATTR ot_sw_rtc_record_t rtc_record" in adapter
assert "nvs_set_" not in adapter and "nvs_commit(" not in adapter
assert "esp_restart(" not in adapter and "gpio_set_level(" not in adapter
assert "esp_reset_reason()" in adapter
assert "ESP_RST_TASK_WDT" in adapter and "ESP_RST_INT_WDT" in adapter
assert "ESP_RST_BROWNOUT" in adapter and "ESP_RST_CPU_LOCKUP" in adapter
assert "OT_SW_BOOTLOOP_LIMIT 3u" in head
assert "OT_SW_STABLE_MS 60000u" in head
boot=part(core,"void app_main(void) {","\n}")
sampler=part(core,"static void input_sampler_task(void *arg){","static void frame_wake_callback")
render=part(core,"static void led_task(void *p) {","void app_main(void) {")
assert boot.index("ot_sw_idf_boot_record();") < boot.index("nvs_flash_init()")
assert boot.index("const bool watchdog_ready=ot_sw_idf_configure();") < boot.index("input_init()")
assert "live_input_mask=tm_gpio_input_mask();" in boot
assert boot.index("live_input_mask=tm_gpio_input_mask();") < boot.index("xTaskCreate(input_sampler_task")
assert "if(ot_sw_idf_boot_loop_seen())" in boot and 'ot_st_set_fault(' in boot
assert "const bool sampler_ready=input_watched &&" in boot
assert "ot_sw_idf_watch_render()" in render
assert render.index("ot_sw_idf_watch_render()") < render.index("native_outputs_ready=true;")
assert "const uint8_t boot_physical=live_input_mask;" in sampler
assert "sampler.settled[i]=asserted;" in sampler
assert "sampler.pending[i]=asserted;" in sampler
assert "sampler.ticks[i]=OT_DEBOUNCE_TICKS;" in sampler
assert sampler.index("sampler.settled[i]=asserted;") < sampler.index("gpio_get_level(input_pin[i])")
assert sampler.index("gpio_get_level(input_pin[i])") < sampler.index(
    "ot_debounce(&sampler,levels,OT_DEBOUNCE_TICKS)")
assert sampler.index("live_input_mask=physical") < sampler.index("ot_sw_idf_input_sampled()")
assert sampler.index("ot_sw_idf_input_sampled()") < sampler.index("vTaskDelayUntil(")
assert render.index("led_strip_refresh_async(strip[ch])") < render.index(
    "led_strip_refresh_async_done(strip[ch])")
assert render.index("led_strip_refresh_async_done(strip[ch])") < render.index(
    "any_output_completed=true;")
assert render.index("any_output_completed=true;") < render.index(
    "ot_sw_idf_render_cycle(full_frame,any_output_completed,")
assert "if(full_frame && ot_sw_idf_stable() &&" in render
assert render.index("ot_sw_idf_render_cycle(") < render.index("ot_ota_confirm_boot(true)")
assert 'if(ot_sw_idf_decorative_inhibited())' in render
for token in ("ot_fx_bank_stop_all(&fx_bank)","native_show.mode=OT_SHOW_OFF",
              "ble_fx_enabled=false","ble_owns_show=false"):
    assert token in render
assert "ot_om_trigger_overlay(role,combined," in render
assert "live_input_mask=physical" in sampler
assert "ot_sw_idf_status_json(wd_json,sizeof(wd_json))" in core
assert r'\"watchdog\":%s' in core and r'\"hardware_failover\":false' in core
assert "nvs,      data, nvs,   0x9000,   0x6000" in partition
assert "otadata,  data, ota,   0xf000,   0x2000" in partition
# Old dual-MCU failover policy remains inert: no hardware takeover.
assert 'ot_failover_essential.c' in cmake
print("PASS issue113: one ESP, config guards, RTC-only resets, TRUE sampled GPIO/RMT progress,"
      " bootloop mode, no NVS/UI change, OTA stability gate; PHYSICAL NOT TESTED")
