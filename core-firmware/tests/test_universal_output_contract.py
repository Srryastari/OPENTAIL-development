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
assert "for(unsigned px=0;px<output->active_pixels;++px)" in core
assert "if(frame_ready[ch])ESP_ERROR_CHECK(led_strip_refresh_async_done(strip[ch]))" in core
assert "pixel_buffer_budget_per_output" in core
assert "max_configurable_pixels_per_output" in core
assert "configured_output_zones" in core
assert 'active_pixels_per_output' in core
assert "OT_TARGET_FPS" not in core, "Do not revive a bench-specific fixed target FPS"
assert 'fps_policy' in core and 'uncapped_measured' in core
assert 'output_mask' in core
assert 'unknown legacy chain name' in core
assert "legacy_example_zone_local" not in core

# Historical physical examples remain only in explicit test fixtures.
for production in (output, board, core):
    for obsolete in ("universal-1246-bench", "30-pixel-bench",
                     "1..623 active pixels", "Driver reserves 623 pixels",
                     "#define OT_OM_MAX_PIXELS_PER_OUTPUT 623"):
        assert obsolete not in production, obsolete

print("PASS: universal user-configured pixel counts; board budget only; dynamic per-output RMT lengths")
