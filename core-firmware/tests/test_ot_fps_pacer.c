#include "ot_fps_pacer.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    ot_fps_pacer_t p={0};
    ot_fps_pacer_init(&p);
    assert(p.target_fps==120u);
    assert(ot_fps_pacer_period_us(&p)==8333u);
    ot_fps_pacer_observe(&p,1000u,30u);
    assert(p.target_fps==120u);
    assert(ot_fps_pacer_wait_us(&p,1000u)>7000u);
    /* Continuous intermediate rates, not a fixed 53/60/120 switch. */
    ot_fps_pacer_observe(&p,9000u,350u);
    assert(p.target_fps>60u&&p.target_fps<120u);
    ot_fps_pacer_observe(&p,15000u,500u);
    assert(p.target_fps>=53u&&p.target_fps<=60u);
    ot_fps_pacer_observe(&p,22000u,900u);
    assert(p.target_fps>0u && p.target_fps<53u);
    /* Overloaded output does not sleep to make an impossible FPS claim. */
    assert(ot_fps_pacer_wait_us(&p,35000u)==0u);
    assert(ot_fps_pacer_wait_us(&p,0u)>0u);
    /* Shrinking a zone must eventually recover to 120, with hysteresis. */
    ot_fps_pacer_observe(&p,1000u,1u);
    assert(p.target_fps==120u);
    for(unsigned i=0;i<16u;i++)ot_fps_pacer_observe(&p,1000u,1u);
    assert(p.target_fps==120u);
    /* 1246 pixels take >=37.68ms on an 800kHz RGB wire, so 53 FPS is impossible. */
    ot_fps_pacer_observe(&p,35000u,1246u);
    assert(p.target_fps>=1u && p.target_fps<=24u);
    assert(ot_fps_pacer_period_us(&p)>=41666u);
    /* Extreme overload still produces a nonzero, bounded rate and period. */
    ot_fps_pacer_observe(&p,2000000u,65535u);
    assert(p.target_fps==1u);
    assert(ot_fps_pacer_period_us(&p)==1000000u);
    /* 30ms fixed task timeouts truncated long adaptive frame waits.
     * Delays convert with a ceiling at BOTH typical RTOS tick rates. */
    assert(ot_fps_pacer_wait_ticks(0u,100u)==0u);
    assert(ot_fps_pacer_wait_ticks(1000u,0u)==0u);
    assert(ot_fps_pacer_wait_ticks(500u,100u)==2u);
    assert(ot_fps_pacer_wait_ticks(30000u,100u)==4u);
    assert(ot_fps_pacer_wait_ticks(75000u,100u)==9u);
    assert(ot_fps_pacer_wait_ticks(1000000u,100u)==101u);
    assert(ot_fps_pacer_wait_ticks(33333u,1000u)==35u);
    assert(ot_fps_pacer_wait_ticks(1000000u,1000u)==1001u);
    printf("Adaptive FPS pacer tests passed: preferred 53..120, truthful overload fallback, no bench defaults\n");
    return 0;
}
