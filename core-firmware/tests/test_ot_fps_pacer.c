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
    assert(p.target_fps==53u);
    /* Overloaded output does not sleep to make an impossible FPS claim. */
    assert(ot_fps_pacer_wait_us(&p,30000u)==0u);
    assert(ot_fps_pacer_wait_us(&p,0u)>0u);
    /* Shrinking a zone must eventually recover to 120, with hysteresis. */
    ot_fps_pacer_observe(&p,1000u,1u);
    assert(p.target_fps==120u);
    for(unsigned i=0;i<16u;i++)ot_fps_pacer_observe(&p,1000u,1u);
    assert(p.target_fps==120u);
    ot_fps_pacer_observe(&p,2000000u,65535u);
    assert(p.target_fps==53u);
    assert(ot_fps_pacer_period_us(&p)==18867u);
    printf("Adaptive FPS pacer tests passed: variable 53..120, actual-load fallback, no bench counts\n");
    return 0;
}
