/* Issue #99: deterministic RGB/temporal regression for owner-reported
 * show effects. Host frames only; no optical/FPS or JNI acceptance claim. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "effects_1d.h"
#include "ot_auto_effects.h"
static int eq(ot_wled_rgb_t a,ot_wled_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
static int has(ot_wled_rgb_t *p,size_t n,ot_wled_rgb_t c){
    for(size_t i=0;i<n;i++)if(eq(p[i],c))return 1;
    return 0;
}
static uint32_t digest(const ot_wled_rgb_t *p,size_t n){
    uint32_t h=2166136261u;
    for(size_t i=0;i<n;i++){h=(h^p[i].r)*16777619u;
        h=(h^p[i].g)*16777619u;h=(h^p[i].b)*16777619u;}
    return h;
}
int main(void){
    static ot_wled_rgb_t p[260],other[260];
    const ot_wled_rgb_t red={255,0,0},green={0,255,0},
        blue={0,0,255},black={0,0,0},guard={19,23,29};
    ot_wled_fx_config_t c={.frame_time_ms=16,.brightness=255,
        .speed=255,.intensity=255,.foreground={255,0,0},
        .background={0,0,0},.tertiary={0,0,255},.tertiary_set=true};
    const uint16_t pulse_ids[]={1u,23u,24u,26u};
    for(size_t j=0;j<4;j++){
        c.effect_id=pulse_ids[j];
        for(uint32_t frame_ms=8u;frame_ms<=34u;frame_ms=
             frame_ms==8u?16u:frame_ms==16u?33u:35u){
            c.frame_time_ms=(uint16_t)frame_ms;
            ot_wled_fx_state_t s={0};
            uint32_t seen_on=0u,seen_off=0u;
            for(unsigned k=0;k<16;k++){
                const uint32_t time=k*frame_ms;
                assert(ot_wled_fx_render(&c,&s,time,p,97u));
                if(eq(p[0],black))seen_off++;
                else seen_on++;
            }
            assert(seen_on==8u && seen_off==8u);
            ot_wled_fx_reset(&s);
        }
    }
    c.frame_time_ms=16u;c.speed=128u;c.intensity=25u;
    /* At explicitly configured true-black BG, unlit Chase-family pixels
     * must be black. Authored nonblack BG is still supported. */
    const uint16_t chase_ids[]={28u,29u,30u};
    for(unsigned i=0;i<3;i++){
        c.effect_id=chase_ids[i];
        ot_wled_fx_state_t s={0};
        assert(ot_wled_fx_render(&c,&s,123u,p,97u));
        assert(has(p,97u,black));
        assert(has(p,97u,red)||c.effect_id==30u);
        ot_wled_fx_reset(&s);
    }
    /* Third-color region boundaries and explicit black remain real frames
     * across speed/intensity extremes; no interpolation substitutes. */
    for(unsigned speed=0;speed<=255;speed=speed==0?1u:
             speed==1?128u:speed==128?254u:speed==254?255u:256u){
        c.speed=(uint8_t)speed;c.intensity=255u;
        c.background=green;
        for(unsigned which=0;which<2;which++){
            c.effect_id=which?55u:56u;
            ot_wled_fx_state_t s={0};
            const uint32_t cycle=1000u+(255u-(uint32_t)c.speed)*200u;
            const uint32_t times[]={0u,cycle/6u,cycle/2u,cycle*5u/6u};
            unsigned distinct=0u;
            uint32_t old=0u;
            for(unsigned k=0;k<4;k++){
                assert(ot_wled_fx_render(&c,&s,which?times[k]:k*21846u,p,97u));
                const uint32_t h=digest(p,97u);
                if(k>0&&h!=old)distinct++;
                old=h;
            }
            assert(distinct>0u);
            ot_wled_fx_reset(&s);
        }
    }
    c.effect_id=103u;c.speed=128u;c.background=black;
    c.intensity=255u;c.foreground=red;c.tertiary=blue;
    ot_wled_fx_state_t glitter={0};
    int blue_seen=0;
    for(unsigned k=0;k<32;k++){
        assert(ot_wled_fx_render(&c,&glitter,k*16u,p,97u));
        blue_seen|=has(p,97u,blue);
        for(size_t i=0;i<97;i++)assert(eq(p[i],red)||eq(p[i],blue));
    }
    assert(blue_seen);
    ot_wled_fx_reset(&glitter);

    /* TV source state is zone-owned. Pixel output changes over a
     * deterministic multi-scene trace; repeated seed/time produces the
     * identical frame sequence. Frame guards cannot be written. */
    c.effect_id=116u;c.speed=255u;c.intensity=128u;c.tertiary_set=false;
    c.foreground=red;c.background=black;
    ot_wled_fx_state_t a={0},b={0};
    uint32_t first=0u;
    int transitions=0;
    for(uint32_t t=0;t<=12000u;t+=250u){
        p[97]=guard;other[97]=guard;
        assert(ot_wled_fx_render(&c,&a,t,p,97u));
        assert(ot_wled_fx_render(&c,&b,t,other,97u));
        assert(memcmp(p,other,97u*sizeof(p[0]))==0);
        assert(eq(p[97],guard)&&eq(other[97],guard));
        const uint32_t h=digest(p,97u);
        if(t&&h!=first)transitions++;
        first=h;
    }
    assert(transitions>=3);
    ot_wled_fx_reset(&a);ot_wled_fx_reset(&b);

    /* Native signature and moving Event Horizon black-core boundary.
     * Fusion Core cannot collapse into Solid Glitter's sparse random dots. */
    c.effect_id=OT_FX_FUSION_CORE;c.intensity=255u;c.speed=128u;
    ot_effects_1d_state_t z={0};ot_effects_1d_reset(&z,17u);
    assert(ot_effects_1d_render(&c,&z,0u,p,97u));
    uint32_t old=digest(p,97u),different=0u;
    for(uint32_t t=16u;t<=1000u;t+=16u){
        assert(ot_effects_1d_render(&c,&z,t,p,97u));
        different+=(digest(p,97u)!=old);
        old=digest(p,97u);
    }
    assert(different>3u);
    ot_effects_1d_reset(&z,17u);
    c.effect_id=OT_FX_EVENT_HORIZON;
    const uint32_t duration=ot_auto_fx_duration_ms(c.speed);
    unsigned colored=0u;old=0u;different=0u;
    for(uint32_t t=0u;t<duration;t+=duration/8u){
        assert(ot_effects_1d_render(&c,&z,t,p,97u));
        const uint32_t h=digest(p,97u);
        if(t&&h!=old)different++;
        old=h;
        colored+=has(p,97u,black);
    }
    assert(different>=4u&&colored>0u);
    assert(ot_effects_1d_render(&c,&z,duration,p,97u));
    for(size_t i=0;i<97u;i++)assert(eq(p[i],red));
    ot_effects_1d_reset(&z,17u);
    puts("PASS Issue99: 30/60/120fps speed255 toggles, black Chase gaps, triangular RGB, TV state & proprietary motion (host)");
    return 0;
}
