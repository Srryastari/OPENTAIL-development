#include "core_logic.h"
#include "arbitration_logic.h"
#include <stddef.h>

static ot_rgb_t rgb(unsigned r, unsigned g, unsigned b) {return (ot_rgb_t){r,g,b};}
static ot_rgb_t scaled(ot_rgb_t c, unsigned scale) {
    return rgb((c.r*scale+127)/255, (c.g*scale+127)/255, (c.b*scale+127)/255);
}
bool ot_layout_valid(const ot_layout_t *cfg) {
    if (!cfg || !cfg->chain_pixels || cfg->brightness_cap > 64) return false;
    unsigned total=0;
    for (int i=0; i<OT_SECTION_COUNT; ++i) {total += cfg->sections[i];}
    return total==cfg->chain_pixels;
}
int ot_section_for_pixel(const ot_layout_t *cfg, unsigned pixel) {
    if (!cfg || pixel>=cfg->chain_pixels) return -1;
    unsigned from=0;
    for (int section=0; section<OT_SECTION_COUNT; ++section) {
        unsigned to=from+cfg->sections[section];
        if (pixel<to) return section;
        from=to;
    }
    return -1;
}
uint8_t ot_debounce(ot_inputs_t *in, const bool levels[OT_IN_COUNT], unsigned stable_ticks) {
    if (!in || !levels) return 0;
    if (stable_ticks<1) stable_ticks=1;
    for (int i=0;i<OT_IN_COUNT;++i) {
        if (levels[i]!=in->pending[i]) {in->pending[i]=levels[i];in->ticks[i]=1;}
        else if (in->ticks[i]<stable_ticks) in->ticks[i]++;
        if (in->ticks[i]>=stable_ticks) in->settled[i]=in->pending[i];
    }
    uint8_t mask=0;
    for (int i=0;i<OT_IN_COUNT;++i) if(in->settled[i]) mask|=(1u<<i);
    return mask;
}

void ot_signal_debouncer_init(ot_signal_debouncer_t *state, bool initial_asserted,
                              uint32_t initial_edge_sequence) {
    if (!state) return;
    *state=(ot_signal_debouncer_t){
        .asserted=initial_asserted,
        .last_edge_sequence=initial_edge_sequence,
        .last_raw_asserted=initial_asserted
    };
}

bool ot_signal_debouncer_update(ot_signal_debouncer_t *state, bool raw_asserted,
                                uint32_t now_us, uint32_t raw_edge_us,
                                uint32_t raw_edge_sequence, unsigned stable_ticks) {
    if (!state) return false;
    /* A mismatched snapshot must never make unsigned elapsed time look huge. */
    const uint32_t edge_ahead=(uint32_t)(raw_edge_us-now_us);
    if (edge_ahead!=0u && edge_ahead<0x80000000u) return state->asserted;
    if (stable_ticks<1u) stable_ticks=1u;
    if (stable_ticks>UINT8_MAX) stable_ticks=UINT8_MAX;
    const bool new_edge=(raw_edge_sequence!=state->last_edge_sequence);
    state->last_edge_sequence=raw_edge_sequence;
    state->last_raw_asserted=raw_asserted;

    if (raw_asserted) {
        if (state->release_pending) {
            const uint32_t inactive_us=(uint32_t)(raw_edge_us-state->inactive_since_us);
            if (inactive_us>=OT_TURN_RELEASE_HOLD_US) {
                /* The previous assertion expired; this is a new edge and
                 * must pass the normal assertion qualification again. */
                state->asserted=false;
                state->assertion_ticks=0u;
            } else {
                /* Reassertion before the deadline bridges the flash gap and
                 * never publishes a false falling edge to the renderer. */
                state->release_pending=false;
                return state->asserted;
            }
            state->release_pending=false;
        }
        if (state->asserted) return true;
        if (state->assertion_ticks<(uint8_t)stable_ticks) state->assertion_ticks++;
        if (state->assertion_ticks>=(uint8_t)stable_ticks) state->asserted=true;
        return state->asserted;
    }

    state->assertion_ticks=0u;
    if (!state->asserted) {
        state->release_pending=false;
        return false;
    }
    /* A newer falling edge means the signal reasserted and dropped again
     * between sampler wakes. Restart the continuous-inactive interval. */
    if (!state->release_pending || new_edge) {
        state->release_pending=true;
        state->inactive_since_us=raw_edge_us;
    }
    if ((uint32_t)(now_us-state->inactive_since_us)>=OT_TURN_RELEASE_HOLD_US) {
        state->asserted=false;
        state->release_pending=false;
    }
    return state->asserted;
}
ot_rgb_t ot_pixel_color(const ot_layout_t *cfg, int chain, unsigned pixel, uint8_t signal_mask,
                        const ot_demo_t *demo, uint32_t tick) {
    const ot_rgb_t off={0,0,0};
    if (!ot_layout_valid(cfg) || chain<0 || chain>=OT_CHAIN_COUNT) return off;
    const int section=ot_section_for_pixel(cfg,pixel);
    /* Compatibility-only helper keeps its older vehicle-over-demo contract.
     * Actual CORE output rendering now composites input overlays per configured
     * Output Manager zone and permits Parking underneath live Show Modes. */
    const ot_arbitration_t arbitration=ot_arbitrate(signal_mask,false,false);
    const ot_zone_mode_t zone=ot_arb_zone(&arbitration,chain,section,0,0);
    ot_rgb_t c=off;
    if(zone==OT_ZONE_BRAKE)c=rgb(255,0,0);
    else if(zone==OT_ZONE_PARK)c=rgb(55,0,0);
    else if(zone==OT_ZONE_LEFT_TURN||zone==OT_ZONE_RIGHT_TURN)c=rgb(255,150,0);
    else if(zone==OT_ZONE_REVERSE)c=rgb(255,255,255);
    // Vehicle signals override demos completely, not only on their assigned zone.
    if (!(signal_mask & OT_USED_INPUTS) && demo && demo->mode!=OT_DEMO_OFF) {
        c=demo->color;
        if(demo->mode==OT_DEMO_CHASE && ((pixel/3+tick/3)%8)>2) c=off;
        return scaled(c,(unsigned)(demo->brightness<cfg->brightness_cap?demo->brightness:cfg->brightness_cap));
    }
    return scaled(c,cfg->brightness_cap);
}
