#pragma once
/* Unified CORE local 1D effect registry. One list; particle physics is private.
 * IDs 0.. are source-pinned WLED adapters. 0x8000 is a distinct original
 * OpenTail "Embers" recipe; NOT represented as upstream WLED parity.
 */
#include "wled_fx_compat.h"
#include "particle_system_1d.h"
#define OT_FX_EMBERS 32768u
typedef struct {
    ot_wled_fx_state_t wled;
    ot_particles_1d_t particles;
    uint32_t previous_ms;
    uint32_t emit_accumulator;
    bool initialized;
    /* Continuous original phase has its own clock so Speed edits never
     * reinterpret past time or restart a running animation. */
    bool original_clock_initialized;
    uint32_t original_last_ms;
    uint32_t original_phase_q32;
} ot_effects_1d_state_t;
size_t ot_effects_1d_supported_ids(uint16_t *dst,size_t capacity);
bool ot_effects_1d_metadata(uint16_t id,ot_wled_fx_metadata_t *meta);
/* Authoritative three-position color role contract for every catalog ID.
 * Disabled intermediate slots are not compacted; no BLE/UI ABI changes. */
bool ot_effects_1d_color_roles(uint16_t id,ot_wled_fx_color_role_t roles[3]);
/* Return the prior renderer-state index for a stable Panel/Zone/effect
 * identity. Inputs must be immutable snapshots for the whole render pass. */
int ot_effects_1d_find_prior_zone_state(uint16_t panel_id,uint16_t zone_id,
    uint32_t generation,const uint16_t *prior_panel_ids,
    const uint16_t *prior_zone_ids,const uint32_t *prior_generations,
    const bool *prior_active,size_t count);
void ot_effects_1d_reset(ot_effects_1d_state_t *s,uint32_t seed);
bool ot_effects_1d_render(const ot_wled_fx_config_t *cfg,
    ot_effects_1d_state_t *s,uint32_t ms,ot_wled_rgb_t *frame,size_t count);
