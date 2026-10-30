#!/usr/bin/env python3
"""Effects-owned static guard for the corrected Firmware #49 frame pipeline.
This verifies source ordering/ownership, not GPIO timing, ISR race behavior
or physical LED output; independent host C/OEM tests cover RGB composition.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
output=(root/"main/output_manager.c").read_text()
effects=(root/"main/effects_1d.c").read_text()
overlay=(root/"main/fx_overlay_engine.c").read_text()
render_start=core.index("static void led_task(")
render=core[render_start:]
def ordered(blob,items):
    at=-1
    for token in items:
        idx=blob.find(token,at+1)
        assert idx>=0, f"Missing or reordered token: {token}"
        at=idx

# Saved/staged NVS swaps never access live palette data inside FX/LED loops.
ordered(render,[
    "portENTER_CRITICAL(&state_lock);",
    "memcpy(local_outputs,om_outputs,sizeof(local_outputs));",
    "if(local_fx_enabled)memcpy(&local_fx_bank,&fx_bank,sizeof(local_fx_bank));",
    "portEXIT_CRITICAL(&state_lock);",
    "ot_palette_runtime_snapshot(palette_ids,OT_OM_OUTPUTS*OT_OM_ZONES,",
    "ot_wled_palette_global_tick(",
    "ot_effects_1d_render(",
    "ot_fx_overlay_apply(",
    "ot_om_trigger_overlay(",
    "ot_frame_snapshot_pixel(",
    "ot_om_swizzle(",
    "led_strip_set_pixel("
])
assert render.count("ot_palette_runtime_snapshot(")==1
assert render.count("ot_wled_palette_global_tick(")==1
assert "750u,true,palette_random_byte,NULL" in render
assert "ot_fx_slot_active(slot,frame_ms)" in render
assert "prior_fx_generation[ch][z]!=slot->generation" in render
assert "ot_effects_1d_reset(&fx_state[ch][z]," in render

# The physical safety override is performed after all decorative FX and
# before logical RGB capture. Decorative calibration must stay strictly
# inside the branch that applies only when NO physical trigger won.
pixel_start=render.index("for(unsigned px=0;px<output->active_pixels;++px)")
pixel_end=render.index("led_strip_refresh_async(strip[ch])",pixel_start)
pixel=render[pixel_start:pixel_end]
ordered(pixel,[
    "bool trigger_here=ot_om_trigger_overlay(",
    "if(!trigger_here){",
    "if(decorated)c=ot_om_calibrate(c,output);",
    "ot_frame_snapshot_pixel(",
    "ot_om_swizzle(",
    "led_strip_set_pixel("
])
assert pixel.count("ot_om_calibrate(")==1
assert pixel.count("ot_om_trigger_overlay(")==1
assert pixel.count("ot_frame_snapshot_pixel(")==1
assert pixel.count("ot_om_swizzle(")==1
assert "if(hit&&color)*color=ot_om_trigger_color(role,active,brightness);" in output
assert "if(cfg->gamma_enabled)" in output
assert "if(!*safety && decorated)" not in output  # test-only utility
assert "if(!ot_palette_1d_supported(" in effects
assert "if(!cfg->resolved_palette" in effects
assert "dst->r=max8(dst->r,r)" in overlay
assert "if(cfg->style==OT_FX_OVERLAY_OFF||!effect_brightness)return true;" in overlay

# STOP must invalidate retained effect frames and not credit incomplete RMT
# writes as stopped/complete. Triggers may override an active show without
# resetting the remaining zone's effect phase.
ordered(render,[
    "bool native_rendering=decision.show_requested;",
    "if(native_rendering && local_fx_enabled){",
    "for(int z=0;z<OT_OM_ZONES;z++)prior_fx_active[ch][z]=false;",
    "if(!native_rendering && full_frame){",
    "if(local_stop_generation>stop_render_generation)"
])
assert "if(frame_captured[ch])ot_frame_snapshot_complete(false);" in render
assert "frame_submitted[ch]=false;" in render
assert "ot_arbitrate(combined," in render
assert "ot_fx_slot_active(&local_fx_bank.zones[ch][zone],frame_ms)" in render
print("PASS: corrected #49 Effects frame ownership, palette -> FX -> overlays -> OEM override -> logical RGB -> wire; physical colors bypass gamma, show/STOP and zone guards (source contract)")
