#include "ot_fps_pacer.h"
#include <stddef.h>

/* 800 kbit/s RGB data is 24 bits * 1.25 us = 30 us per pixel.
 * Add reset/latch allowance; this is a protocol bound, not a vehicle count,
 * a measured RMT duration, or a power/safety guarantee. */
#define OT_RGB800_WIRE_US_PER_PIXEL 30u
#define OT_RGB800_LATCH_US 300u
#define OT_FPS_UPGRADE_CONFIRM_FRAMES 16u

static uint32_t larger(uint32_t a,uint32_t b){return a>b?a:b;}
static uint16_t desired_fps(uint32_t estimated_us){
    /* Reserve 10% headroom for scheduler and bus/render variability.
     * Choose any integer fps in the requested adaptive range; 60 is not
     * forced as a ceiling, and 120 is used only when the load allows. */
    const uint32_t guarded=estimated_us+estimated_us/10u;
    if(!guarded)return OT_FPS_PREFERRED_HIGH;
    uint32_t fps=1000000u/guarded;
    if(fps>OT_FPS_PREFERRED_HIGH)fps=OT_FPS_PREFERRED_HIGH;
    if(fps<OT_FPS_PREFERRED_LOW)fps=OT_FPS_PREFERRED_LOW;
    return (uint16_t)fps;
}
void ot_fps_pacer_init(ot_fps_pacer_t *p){
    if(!p)return;
    *p=(ot_fps_pacer_t){.target_fps=OT_FPS_PREFERRED_HIGH};
}
void ot_fps_pacer_observe(ot_fps_pacer_t *p,uint32_t work_us,
                          uint16_t longest_output_pixels){
    if(!p)return;
    /* Bound pathological timer samples; never let corrupt telemetry
     * overflow the accumulator or silently claim excessive throughput. */
    if(work_us>2000000u)work_us=2000000u;
    if(!p->mean_work_us)p->mean_work_us=work_us;
    else p->mean_work_us=(p->mean_work_us*7u+work_us+4u)/8u;
    const uint32_t wire_us=(uint32_t)longest_output_pixels*
                            OT_RGB800_WIRE_US_PER_PIXEL+OT_RGB800_LATCH_US;
    const uint16_t desired=desired_fps(larger(p->mean_work_us,wire_us));
    if(p->last_longest_output_pixels!=longest_output_pixels){
        p->last_longest_output_pixels=longest_output_pixels;
        /* Installation changed: reevaluate immediately instead of dragging
         * the previous strip's target into the new profile. */
        p->mean_work_us=work_us;
        p->target_fps=desired_fps(larger(work_us,wire_us));
        p->upgrade_streak=0u;
    }else if(desired<p->target_fps){
        p->target_fps=desired;
        p->upgrade_streak=0u;
    }else if(desired>p->target_fps){
        if(++p->upgrade_streak>=OT_FPS_UPGRADE_CONFIRM_FRAMES){
            const uint16_t delta=(uint16_t)(desired-p->target_fps);
            p->target_fps=(uint16_t)(p->target_fps+(delta>4u?delta/4u:1u));
            p->upgrade_streak=0u;
        }
    }else p->upgrade_streak=0u;
}
uint32_t ot_fps_pacer_period_us(const ot_fps_pacer_t *p){
    const uint32_t fps=(p&&p->target_fps>=OT_FPS_PREFERRED_LOW&&
                        p->target_fps<=OT_FPS_PREFERRED_HIGH)
                       ?p->target_fps:OT_FPS_PREFERRED_HIGH;
    return 1000000u/fps;
}
uint32_t ot_fps_pacer_wait_us(const ot_fps_pacer_t *p,uint32_t work_us){
    const uint32_t period=ot_fps_pacer_period_us(p);
    return work_us<period?period-work_us:0u;
}
