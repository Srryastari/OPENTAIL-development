#!/usr/bin/env python3
"""Regression fence: vehicle/bench pixel counts never become firmware limits.

This is a source contract, not a substitute for ESP-IDF compilation, NVS
migration checks, measured FPS, output timing or physical hardware acceptance.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "main"
output = (root / "output_manager.h").read_text(encoding="utf-8")
board = (root / "board_config.h").read_text(encoding="utf-8")
core = (root / "opentail_core.c").read_text(encoding="utf-8")
kconfig = (root / "Kconfig.projbuild").read_text(encoding="utf-8")

# Hardware/build allocation limits are independent of vehicle examples.
assert "#define OT_OM_OUTPUTS OT_CHAIN_COUNT" in output
assert "#define OT_OM_MAX_PIXELS_PER_OUTPUT ((unsigned)CONFIG_OPENTAIL_BUFFER_PIXELS_PER_OUTPUT)" in output
assert "config OPENTAIL_BUFFER_PIXELS_PER_OUTPUT" in kconfig
assert "#define OT_CHAIN_PIXELS OT_OM_MAX_PIXELS_PER_OUTPUT" in board
assert "OT_SMALL_BENCH_LAYOUT" not in board
assert "OT_LAYOUT_SECTIONS {6,6,6,6,6" not in board

# Runtime driver length must follow each saved output, not a global max.
assert ".max_leds=pixels" in core
assert "strip_configure_channel(ch,om_outputs[ch].active_pixels)" in core
assert "strip_configure_channel(ch,local_outputs[ch].active_pixels)" in core
# Resize must not delete a live RMT output when blackout fails.
resize = core.split("static bool strip_configure_channel(",1)[1].split("static bool strip_init(",1)[0]
assert "(void)led_strip_clear(strip[ch]);" not in resize
assert "esp_err_t e=led_strip_clear(strip[ch]);" in resize
assert 'ot_st_set_fault("external pixel RMT clear failed");return false;' in resize
assert resize.index('if(e!=ESP_OK){') < resize.index('e=led_strip_del(strip[ch]);')
assert resize.index('e=led_strip_del(strip[ch]);') < resize.index('strip[ch]=NULL;strip_capacity[ch]=0;')

assert "for(unsigned px=0;px<output->active_pixels;++px)" in core
assert "if(frame_submitted[ch])ESP_ERROR_CHECK(led_strip_refresh_async_done(strip[ch]))" in core
assert "ot_frame_all_submitted(frame_submitted,OT_CHAIN_COUNT)" in core
assert "if(!native_rendering && full_frame)" in core
assert "if(full_frame)frame_window_frames++" in core
assert "incomplete_frames" in core
assert "pixel_buffer_budget_per_output" in core
assert "max_configurable_pixels_per_output" in core
assert "configured_output_zones" in core
assert 'active_pixels_per_output' in core
assert "OT_TARGET_FPS" not in core, "Do not revive a bench-specific fixed target FPS"
assert 'fps_policy' in core and 'adaptive_load_53_to_120' in core
assert 'ot_fps_pacer_observe(&pacer,work_us,longest_pixels)' in core
assert 'ot_fps_pacer_wait_us(&pacer,work_us)' in core
assert 'input_sampler_task' in core and 'vTaskDelayUntil(&last_wake,period)' in core
assert 'frame_time_ms=(uint16_t)frame_delta_ms' in core
assert 'output_mask' in core
# All firmware preview endpoints select generic physical outputs, not car sides.
assert "static bool preview_output_mask(" in core
assert core.count("preview_output_mask(obj,&chain_mask)") == 2
preview = core.split("static esp_err_t fx_preview_post(",1)[1].split("static esp_err_t auto_preview_post(",1)[0]
auto = core.split("static esp_err_t auto_preview_post(",1)[1].split("static esp_err_t ",1)[0]
assert "uint8_t chain_mask=3u;" not in preview
assert "uint8_t chain_mask=3u;" not in auto
assert 'selection->valueint>all' in core
assert 'selection&&legacy' in core  # ambiguous legacy/canonical selectors rejected
assert '.chain_mask=(uint8_t)((1u<<OT_CHAIN_COUNT)-1u)' in core
assert 'unknown legacy chain name' in core
assert "legacy_example_zone_local" not in core

# NVS load must recover the previously saved profile even when v2 exists
# but is invalid. Boot migration must not write over either record.
load = core.split("static void om_load(void){",1)[1].split("static bool om_save(",1)[0]
assert 'const char *const keys[]={"profile_v2","profile"}' in load
assert 'attempt<sizeof(keys)/sizeof(keys[0])&&!om_persistent' in load
assert 'nvs_get_blob(h,key,NULL,&n)' in load
assert 'nvs_set_blob' not in load and 'nvs_erase' not in load
assert 'valid=ot_om_valid(&stored.outputs[i])' in load

# Historical physical examples remain only in explicit test fixtures.
for production in (output, board, core):
    for obsolete in ("universal-1246-bench", "30-pixel-bench",
                     "1..623 active pixels", "Driver reserves 623 pixels",
                     "#define OT_OM_MAX_PIXELS_PER_OUTPUT 623"):
        assert obsolete not in production, obsolete

print("PASS: universal user-configured pixel counts; board budget only; dynamic per-output RMT lengths")
