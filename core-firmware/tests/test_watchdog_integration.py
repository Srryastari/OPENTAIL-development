#!/usr/bin/env python3
"""Guard owner boundaries and checkpoint placement. Structural, not bench proof."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
core = (root / "main/opentail_core.c").read_text()
watch = (root / "main/ot_watchdog.c").read_text()
startup = core.split("void app_main(void) {", 1)[1]
sampler = core.split("static void input_sampler_task(void *arg){", 1)[1].split("static void frame_wake_callback", 1)[0]
renderer = core.split("static void led_task(void *p) {", 1)[1].split("void app_main(void) {", 1)[0]
assert startup.index("ot_watchdog_init();") < startup.index("nvs_flash_init()")
assert "nvs_flash_erase" not in core + watch
assert startup.index("input_sampler_primed,__ATOMIC_ACQUIRE") < startup.index("strip_init()")
assert sampler.index("ot_debounce(") < sampler.index("ot_watchdog_progress(OT_WATCH_INPUTS)") < sampler.index("input_sampler_primed,true")
assert renderer.index("led_strip_refresh_async_done(strip[ch])") < renderer.index("ot_watchdog_progress(OT_WATCH_RENDER)")
assert renderer.index("OT_PHASE_SUBMIT_O1") < renderer.index("led_strip_refresh_async(strip[ch])")
assert renderer.index("OT_PHASE_COMPLETE_O1") < renderer.index("led_strip_refresh_async_done(strip[ch])")
assert "else if(!config_recovery_fault){" in startup
assert "sampler_ready&&!config_recovery_fault&&strip_init()" in startup
assert "config_recovery_fault=nvs_ready!=ESP_OK || ot_watchdog_quarantined()" in startup
assert "esp_task_wdt_add_user(" in watch and "esp_task_wdt_reset_user(" in watch
assert "esp_task_wdt_reconfigure(" in watch and ".trigger_panic=true" in watch
assert ".idle_core_mask=(1u<<portNUM_PROCESSORS)-1u" in watch
for forbidden in ("esp_task_wdt_deinit(", "esp_int_wdt_disable(", "nvs_set_", "nvs_commit("):
    assert forbidden not in watch
assert "vTaskDelay(1);" in renderer and "last_idle_yield_ms" in renderer
cfg = (root / "main/Kconfig.projbuild").read_text()
assert 'config OPENTAIL_WATCHDOG_TEST_FAULT' in cfg and 'default 0' in cfg
assert "No remote fault-injection API" in cfg
print("PASS: startup/sample/output checkpoint order, held-input priming, NVS preservation, quarantine isolation, system protections and disabled bench injection (source only)")
