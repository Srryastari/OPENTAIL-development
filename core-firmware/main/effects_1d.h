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
} ot_effects_1d_state_t;
size_t ot_effects_1d_supported_ids(uint16_t *dst,size_t capacity);
bool ot_effects_1d_metadata(uint16_t id,ot_wled_fx_metadata_t *meta);
void ot_effects_1d_reset(ot_effects_1d_state_t *s,uint32_t seed);
bool ot_effects_1d_render(const ot_wled_fx_config_t *cfg,
    ot_effects_1d_state_t *s,uint32_t ms,ot_wled_rgb_t *frame,size_t count);
