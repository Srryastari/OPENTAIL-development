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
fxbank=(root/"main/fx_zone_bank.c").read_text()
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
    "if(pz_preview.active&&physical==0u)",
    "memcpy(local_panel,pz_preview.candidate.outputs,sizeof(local_panel));",
    "memcpy(local_panel,pz_outputs.outputs,sizeof(local_panel));",
    "if(local_fx_enabled)memcpy(&local_fx_bank,&fx_bank,sizeof(local_fx_bank));",
    "portEXIT_CRITICAL(&state_lock);",
    "ot_om_defaults(default_outputs);",
    "ot_pz_to_fx_view(&local_panel[ch],ch,&local_outputs[ch])",
    "ot_palette_runtime_snapshot(palette_ids,OT_OM_OUTPUTS*OT_OM_ZONES,",
    "ot_wled_palette_global_tick(",
    "ot_effects_1d_render(",
    "ot_fx_overlay_apply(",
    "ot_om_trigger_overlay(",
    "ot_om_limit_brightness(",
    "ot_frame_snapshot_pixel(",
    "ot_om_swizzle(",
    "led_strip_set_pixel("
])
assert render.count("ot_palette_runtime_snapshot(")==1
assert render.count("ot_wled_palette_global_tick(")==1
assert "750u,true,palette_random_byte,NULL" in render
assert "ot_fx_slot_active(slot,frame_ms)" in render
assert "static ot_effects_1d_state_t prior_fx_state[OT_OM_ZONES]" in render
assert "memcpy(prior_fx_state,fx_state[ch],sizeof(prior_fx_state))" in render
assert "memset(fx_state[ch],0,sizeof(fx_state[ch]))" in render
assert "ot_effects_1d_move_state(&fx_state[ch][z]" in render
assert "ot_effects_1d_reset(&prior_fx_state[old_z],0u)" in render
assert "for(unsigned old_z=0;old_z<OT_OM_ZONES;old_z++)" in render
assert "memcpy(old_generation,prior_fx_generation[ch],sizeof(old_generation))" in render
assert "ot_effects_1d_find_prior_zone_state(" in render
assert "int ot_effects_1d_find_prior_zone_state(" in effects
assert "void ot_effects_1d_move_state(" in effects
assert "memset(source,0,sizeof(*source))" in effects
assert "prior_panel_ids[i]==panel_id" in effects
assert "prior_zone_ids[i]==zone_id" in effects
assert "prior_generations[i]==generation" in effects
assert "old_z==(uint8_t)prior_index" in render
assert "ot_fx_bank_resolve_zone_identity(&local_panel[ch],&local_fx_bank" in render
assert "resolved_fx_bank_index" in render
assert "bool ot_fx_bank_resolve_zone_identity(" in fxbank
assert "slot->owner_panel_id==zone_cfg->panel_id" in fxbank
assert "slot->owner_zone_id==zone_cfg->id" in fxbank
assert "slot->owner_panel_id=panel->zones[zone].panel_id" in fxbank
assert "if(same_identity)" in render
assert "ot_effects_1d_reset(&fx_state[ch][z]," in render

# All public live mutations resolve current logical zone masks to stable bank slots.
assert "ot_fx_bank_apply_configured_identity(&fx_bank,req,om_outputs," in core
assert "ot_fx_bank_apply_continuous_tertiary_identity(&fx_bank,req," in core
assert "ot_fx_bank_set_overlay_identity(&fx_bank,om_outputs," in core
assert "fx_bank.zones[o][resolved[o][z]].spark=*spark" in core
assert "fx_bank.zones[o][resolved[o][z]].overlay=*overlay" in core
assert "ot_fx_bank_apply_configured_identity(&replacement,&targeted," in core
assert "ot_fx_bank_prune_orphans(&fx_bank,pz_outputs.outputs)" in core
assert "bool ot_fx_bank_apply_configured_identity(" in fxbank
assert "bool ot_fx_bank_apply_continuous_tertiary_identity(" in fxbank
assert "bool ot_fx_bank_set_overlay_identity(" in fxbank
assert "void ot_fx_bank_prune_orphans(" in fxbank

# The physical safety override is performed after all decorative FX and
# before logical RGB capture. Decorative calibration must stay strictly
# inside the branch that applies only when NO physical trigger won.
frame_start=render.index("frame_captured[ch]=ot_frame_snapshot_begin(")
pixel_start=render.index("for(unsigned px=0;px<render_pixels[ch];++px)",frame_start)
pixel_end=render.index("led_strip_refresh_async(strip[ch])",pixel_start)
pixel=render[pixel_start:pixel_end]
ordered(pixel,[
    "bool trigger_here=false;",
    "ot_om_trigger_overlay(",
    "if(!trigger_here){",
    "if(decorated)c=ot_om_calibrate(c,output);",
    "ot_om_limit_brightness(",
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
    "if(native_rendering&&local_fx_enabled){",
    "for(unsigned z=0;z<OT_OM_ZONES;z++)prior_fx_active[ch][z]=false;",
    "if(!native_rendering && full_frame){",
    "if(local_stop_generation>stop_render_generation)"
])
assert "if(frame_captured[ch])ot_frame_snapshot_complete(false);" in render
assert "frame_submitted[ch]=false;" in render
assert "ot_arbitrate(combined," in render
assert "ot_fx_slot_active(&local_fx_bank.zones[ch][bank_z],frame_ms)" in render
# FX-PZ continuity: stable Panel/Zone identity owns both effect slots and
# renderer phase across index shifts, while the old pixel map gates frame carry.
assert "old_zone_id[old_z]==zone_cfg->id" in render
assert "old_panel_id[old_z]==zone_cfg->panel_id" in render
assert "const uint8_t old_z=prior_pixel_zone_map[ch][px];" in render
assert "fx_zone_scratch[logical]=fx_frame[ch][px];" in render
reset_pos=render.index("ot_effects_1d_reset(&fx_state[ch][z],")
assert render.index("if(same_identity)") < reset_pos
prior_map_copy=render.index("memcpy(prior_pixel_zone_map[ch],pixel_zone_map[ch],")
assert prior_map_copy > render.index("if(frame_captured[ch])ot_frame_snapshot_complete(true);")
print("PASS: corrected #49 Effects frame ownership, palette -> FX -> overlays -> OEM override -> logical RGB -> wire; physical colors bypass gamma, show/STOP and zone guards (source contract)")
