#!/usr/bin/env python3
"""Effects #110 renderer / Firmware #115 watchdog exact-source compatibility.
Static source-order and immutable-source guard; hardware timing is not claimed.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
FW115 = "f97bceec7445a958f735df17ca0842b97bf8e062"
EFFECTS110 = "6180e2a5ccbfbf8567513504669c6840b68c045b"

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

def section(source, start, end):
    assert start in source and end in source, (start, end)
    return source.split(start, 1)[1].split(end, 1)[0]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--effects-dir", type=Path, required=True)
    args = parser.parse_args()
    fxroot = args.effects_dir.resolve()
    assert subprocess.check_output(["git", "-C", str(fxroot), "rev-parse", "HEAD"],
                                   text=True).strip() == EFFECTS110
    assert git("merge-base", "--is-ancestor", FW115, "HEAD") == ""

    # This is a no-production-code sign-off. Only this evidence, workflow and
    # host-only test may be added on top of the exact Firmware #115 candidate.
    allowed = {
        "core-firmware/tests/test_effect_watchdog_compat.c",
        "scripts/tests/test_effects_watchdog_compatibility.py",
        "docs/EFFECTS-FIRMWARE115-COMPATIBILITY-SIGNOFF.md",
        ".github/workflows/verify-effects-firmware115-compatibility.yml",
    }
    changed = set(git("diff", "--name-only", FW115, "HEAD").splitlines())
    assert changed == allowed, ("Unexpected product/source changes", sorted(changed - allowed))

    renderer = (
        "wled_fx_compat.c", "wled_fx_compat.h",
        "effects_1d.c", "effects_1d.h",
        "wled_1d.c", "wled_1d.h",
        "palette_1d.c", "palette_1d.h",
        "particle_system_1d.c", "particle_system_1d.h",
        "ot_auto_effects.c", "ot_auto_effects.h",
    )
    for name in renderer:
        current = ROOT / "core-firmware/main" / name
        original = fxroot / "core-firmware/main" / name
        assert current.read_bytes() == original.read_bytes(), name

    # The pure C Android/JNI renderer is still the same byte-for-byte source
    # as CORE, with the existing SHA256 manifest and 13 supported BLE RGB3 modes.
    jni = ROOT / "android/app/src/main/cpp/core"
    for name in renderer:
        assert (jni / name).read_bytes() == (ROOT / "core-firmware/main" / name).read_bytes(), name
    manifest = json.loads((jni / "SOURCE.json").read_text())
    assert manifest["source_commit"] == EFFECTS110
    assert manifest["seamless_loop_source_commit"] == EFFECTS110
    assert manifest["color_slot_source_commit"] == EFFECTS110
    assert manifest["tertiary_source_proven_modes"] == 13
    assert manifest["tertiary_transport_admitted_modes"] == 13

    stress = (ROOT / "core-firmware/tests/test_effect_watchdog_compat.c").read_text()
    assert "render_work_ms" in stress
    assert "completed_at - last_feed_ms >= timeout_ms" in stress
    assert "14900u" in stress and "14940u" in stress
    assert "HOST_SIMULATED" in stress

    core = (ROOT / "core-firmware/main/opentail_core.c").read_text()
    adapter = (ROOT / "core-firmware/main/ot_single_watchdog_idf.c").read_text()
    policy = (ROOT / "core-firmware/main/ot_single_watchdog.c").read_text()
    pacer = (ROOT / "core-firmware/main/ot_fps_pacer.c").read_text()
    output_header = (ROOT / "core-firmware/main/output_manager.h").read_text()

    # Watchdog configuration is generous and retains both ESP-IDF idle tasks.
    assert ".timeout_ms=15000u" in adapter
    assert ".idle_core_mask=3u" in adapter
    assert ".trigger_panic=true" in adapter
    assert "esp_task_wdt_reset_user(render_user)" in adapter
    assert "return all_lanes_completed||any_lane_completed;" in policy

    # Production feed is reached only after successful asynchronous completion.
    render = section(core, "static void led_task(void *p) {", "void app_main(void) {")
    submit = render.index("led_strip_refresh_async(strip[ch])")
    submitted = render.index("frame_submitted[ch]=true;", submit)
    both_submitted = render.index("/* Start both output transfers before waiting")
    wait = render.index("led_strip_refresh_async_done(strip[ch])")
    any_done = render.index("any_output_completed=true;", wait)
    cycle = render.index("ot_sw_idf_render_cycle(full_frame,any_output_completed,")
    assert submit < submitted < both_submitted < wait < any_done < cycle
    failed_wait = render[wait:any_done]
    assert "if(e!=ESP_OK)" in failed_wait
    assert "frame_submitted[ch]=false;" in failed_wait
    assert "continue;" in failed_wait
    assert "if(!ot_sw_render_progress_eligible(all_outputs_completed,any_output_completed))" in adapter
    feed_gate = adapter.index("if(!ot_sw_render_progress_eligible(all_outputs_completed,any_output_completed))")
    feed_call = adapter.index("esp_task_wdt_reset_user(render_user)")
    assert feed_gate < feed_call
    adapter_cycle = section(adapter, "void ot_sw_idf_render_cycle(", "bool ot_sw_idf_decorative_inhibited")
    assert "const bool healthy=all_outputs_completed" in adapter_cycle
    assert "(uint32_t)(now-last_input)<100u" in adapter_cycle
    assert "!diagnosed_fault && !configuration_fault" in adapter_cycle

    # Both output DMA transfers overlap; a normal frame's longest 800-kHz RGB
    # wire time is far below the 15s TWDT. This is a protocol bound, not a
    # measured MCU render time or proof of physical RMT completion.
    default = re.search(r"#define CONFIG_OPENTAIL_BUFFER_PIXELS_PER_OUTPUT\s+(\d+)", output_header)
    assert default, "Missing explicit per-output memory default"
    max_pixels = int(default.group(1))
    assert "OT_RGB800_WIRE_US_PER_PIXEL 30u" in pacer
    assert "OT_RGB800_LATCH_US 300u" in pacer
    wire_us = max_pixels * 30 + 300
    assert wire_us < 15_000_000
    assert "ot_sw_idf_render_cycle(full_frame,any_output_completed," in render

    print(
        "PASS Effects #110 / Firmware #115 exact-source compatibility: "
        "12 renderer files and JNI byte identity; 140-ID/13-RGB3 manifest; "
        "successful async_done before render WDT feed; independent O1/O2 "
        "submission; one-lane progress vs full-frame stability; "
        f"{max_pixels}-pixel 800kHz wire bound {wire_us}us < 15000000us TWDT. "
        "Host/source evidence only; physical RMT/TWDT NOT_RUN."
    )

if __name__ == "__main__":
    main()
