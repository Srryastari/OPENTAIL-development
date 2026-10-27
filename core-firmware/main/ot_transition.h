#pragma once
/* Pure, allocation-free transition math. No hardware side effects.
 * OFF is exact RGB(0,0,0), including at completion.
 * This module is not yet wired into the production renderer.
 */
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint8_t r,g,b; } ot_transition_rgb_t;
typedef enum { OT_TRANSITION_SMOOTH=0, OT_TRANSITION_SNAP=1 } ot_transition_mode_t;
static inline uint8_t ot_transition_lerp8(uint8_t a,uint8_t b,uint16_t weight){
    if(weight>=1000u)return b;
    return (uint8_t)(((uint32_t)a*(1000u-weight)+(uint32_t)b*weight+500u)/1000u);
}
static inline uint16_t ot_transition_progress(uint32_t elapsed_ms,uint32_t duration_ms,ot_transition_mode_t mode){
    if(duration_ms==0u||elapsed_ms>=duration_ms)return 1000u;
    if(mode==OT_TRANSITION_SNAP)return 0u;
    /* Smoothstep in 0..1000, integer-only, bounded to avoid overflow. */
    uint32_t t=(uint32_t)(((uint64_t)elapsed_ms*1000u)/duration_ms);
    return (uint16_t)((t*t*(3000u-2u*t)+500000u)/1000000u);
}
static inline ot_transition_rgb_t ot_transition_blend(ot_transition_rgb_t from,ot_transition_rgb_t to,uint16_t progress){
    ot_transition_rgb_t out={ot_transition_lerp8(from.r,to.r,progress),
        ot_transition_lerp8(from.g,to.g,progress),
        ot_transition_lerp8(from.b,to.b,progress)};
    return out;
}
static inline ot_transition_rgb_t ot_transition_off(ot_transition_rgb_t from,uint32_t elapsed_ms,uint32_t duration_ms,ot_transition_mode_t mode){
    const ot_transition_rgb_t black={0,0,0};
    return ot_transition_blend(from,black,ot_transition_progress(elapsed_ms,duration_ms,mode));
}
