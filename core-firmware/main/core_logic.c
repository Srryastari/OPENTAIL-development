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
ot_rgb_t ot_pixel_color(const ot_layout_t *cfg, int chain, unsigned pixel, uint8_t signal_mask,
                        const ot_demo_t *demo, uint32_t tick) {
    const ot_rgb_t off={0,0,0};
    if (!ot_layout_valid(cfg) || chain<0 || chain>=OT_CHAIN_COUNT) return off;
    const int section=ot_section_for_pixel(cfg,pixel);
    const ot_arbitration_t arbitration=ot_arbitrate(signal_mask,demo&&demo->mode!=OT_DEMO_OFF,false);
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
