/* Source-proven Issue102 tertiary capability. No JNI, BLE, overlay
 * authorship, or vehicle behavior is modified by this host fixture. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "wled_fx_compat.h"
#include "effects_1d.h"
static int eq(ot_wled_rgb_t a,ot_wled_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
int main(void){
    ot_wled_fx_config_t a={.speed=128u,.intensity=255u,
        .brightness=255u,.frame_time_ms=16u,.palette_id=0u,
        .foreground={255,0,0},.background={0,255,0},
        .tertiary={0,0,255},.tertiary_set=true};
    ot_wled_fx_config_t b=a;
    b.tertiary=(ot_wled_rgb_t){255,255,0};
    const uint16_t unsupported[]={10u,11u,40u,48u,60u,91u};
    const uint16_t proven_before[]={28u,29u,34u,50u,52u,54u,
                                     55u,56u,84u,87u,103u};
    ot_wled_rgb_t p[258]={0},q[258]={0};
    const ot_wled_rgb_t guard={13,17,19};
    for(unsigned i=0;i<6u;i++){
        const uint16_t id=unsupported[i];
        ot_wled_fx_color_role_t roles[3];
        assert(ot_wled_fx_color_roles(id,roles)&&roles[2].enabled);
        assert(!ot_wled_fx_tertiary_supported(id));
        a.effect_id=id;b.effect_id=id;
        ot_wled_fx_state_t sa={0},sb={0};
        memset(p,0,257u*sizeof(*p));
        memset(q,0,257u*sizeof(*q));
        for(uint32_t t=0;t<600u;t+=16u){
            p[257]=guard;q[257]=guard;
            assert(ot_wled_fx_render(&a,&sa,t,p,257u));
            assert(ot_wled_fx_render(&b,&sb,t,q,257u));
            assert(memcmp(p,q,257u*sizeof(*p))==0);
            assert(eq(p[257],guard)&&eq(q[257],guard));
        }
        ot_wled_fx_reset(&sa);ot_wled_fx_reset(&sb);
    }
    for(unsigned i=0;i<11u;i++)assert(ot_wled_fx_tertiary_supported(proven_before[i]));
    for(unsigned j=0;j<2u;j++){
        const uint16_t id=j?95u:38u;
        assert(ot_wled_fx_tertiary_supported(id));
        a.effect_id=id;b.effect_id=id;
        /* Aurora palette 4 explicitly consumes Color 3; default palette
         * 50 remains untouched and must not claim its gradient is RGB3. */
        a.palette_id=j?0u:4u;b.palette_id=a.palette_id;
        a.speed=128u;b.speed=128u;a.intensity=255u;b.intensity=255u;
        ot_wled_fx_state_t sa={0},sb={0};
        memset(p,0,257u*sizeof(*p));
        memset(q,0,257u*sizeof(*q));
        unsigned differences=0u,third_pixels=0u;
        for(uint32_t t=0;t<25000u;t+=16u){
            assert(ot_wled_fx_render(&a,&sa,t,p,257u));
            assert(ot_wled_fx_render(&b,&sb,t,q,257u));
            for(unsigned k=0;k<257u;k++){
                if(!eq(p[k],q[k]))differences++;
                if(eq(p[k],a.tertiary))third_pixels++;
            }
        }
        assert(differences>0u);
        /* Popcorn's exact RGB3 spark is direct; Aurora is a blended
         * palette where the third color can be interpolated. */
        if(j)assert(third_pixels>0u);
        ot_wled_fx_reset(&sa);ot_wled_fx_reset(&sb);
    }
    /* Both new modes also preserve old two-color behavior when tertiary
     * is unconfigured, regardless of stale bytes in that unused field. */
    for(unsigned j=0;j<2;j++){
        a.effect_id=j?95u:38u;a.palette_id=j?0u:4u;
        b=a;a.tertiary_set=false;b.tertiary_set=false;
        a.tertiary=(ot_wled_rgb_t){0,0,255};
        b.tertiary=(ot_wled_rgb_t){255,255,0};
        ot_wled_fx_state_t sa={0},sb={0};
        memset(p,0,257u*sizeof(*p));
        memset(q,0,257u*sizeof(*q));
        for(uint32_t t=0;t<3000u;t+=16u){
            assert(ot_wled_fx_render(&a,&sa,t,p,257u));
            assert(ot_wled_fx_render(&b,&sb,t,q,257u));
            assert(memcmp(p,q,257u*sizeof(*p))==0);
        }
        ot_wled_fx_reset(&sa);ot_wled_fx_reset(&sb);
    }
    /* Aurora palette 5 is a three-color palette. Explicit black must
     * remain a real third stop instead of collapsing to the legacy 2-color
     * profile when the RGB value itself is zero. */
    a=(ot_wled_fx_config_t){
        .effect_id=38u,.speed=128u,.intensity=255u,.brightness=255u,
        .frame_time_ms=16u,.palette_id=5u,
        .foreground={255,0,0},.background={0,255,0},
        .tertiary={0,0,0},.tertiary_set=true
    };
    b=a;
    b.tertiary_set=false;
    b.tertiary=(ot_wled_rgb_t){255,255,0}; /* stale bytes must be ignored */
    ot_wled_fx_state_t aurora_black={0},aurora_legacy={0};
    unsigned aurora_black_differences=0u;
    for(uint32_t t=0u;t<60000u;t+=16u){
        assert(ot_wled_fx_render(&a,&aurora_black,t,p,257u));
        assert(ot_wled_fx_render(&b,&aurora_legacy,t,q,257u));
        for(unsigned k=0u;k<257u;k++)
            if(!eq(p[k],q[k]))aurora_black_differences++;
    }
    assert(aurora_black_differences>0u);
    ot_wled_fx_reset(&aurora_black);ot_wled_fx_reset(&aurora_legacy);

    /* Popcorn's source-drawn third slot must emit exact black when enabled;
     * the same seed with tertiary_set=false maps that source slot to FG. */
    a=(ot_wled_fx_config_t){
        .effect_id=95u,.speed=128u,.intensity=255u,.brightness=255u,
        .frame_time_ms=16u,.palette_id=0u,
        .foreground={255,0,0},.background={0,255,0},
        .tertiary={0,0,0},.tertiary_set=true
    };
    b=a;
    b.tertiary_set=false;
    b.tertiary=(ot_wled_rgb_t){255,255,0};
    ot_wled_fx_state_t popcorn_black={0},popcorn_legacy={0};
    unsigned popcorn_black_pixels=0u;
    unsigned max_enabled_green_pixels=0u;
    unsigned max_legacy_green_pixels=0u;
    for(uint32_t t=0u;t<60000u;t+=16u){
        assert(ot_wled_fx_render(&a,&popcorn_black,t,p,257u));
        assert(ot_wled_fx_render(&b,&popcorn_legacy,t,q,257u));
        unsigned enabled_green_pixels=0u,legacy_green_pixels=0u;
        for(unsigned k=0u;k<257u;k++){
            if(eq(p[k],(ot_wled_rgb_t){0,0,0}) &&
               eq(q[k],a.foreground))popcorn_black_pixels++;
            if(eq(p[k],a.background))enabled_green_pixels++;
            if(eq(q[k],a.background))legacy_green_pixels++;
        }
        if(enabled_green_pixels>max_enabled_green_pixels)
            max_enabled_green_pixels=enabled_green_pixels;
        if(legacy_green_pixels>max_legacy_green_pixels)
            max_legacy_green_pixels=legacy_green_pixels;
    }
    assert(popcorn_black_pixels>0u);
    /* Green may appear only on at most 21 authored particles when RGB3 is
     * active; the empty field must be black. The legacy path still fills
     * the field with its configured green background. */
    assert(max_enabled_green_pixels<=21u);
    assert(max_legacy_green_pixels>200u);
    ot_wled_fx_reset(&popcorn_black);ot_wled_fx_reset(&popcorn_legacy);

    /* Solid Glitter's legacy default remains white; explicit RGB black is
     * an authored GL color and must never be raised to a minimum brightness. */
    a=(ot_wled_fx_config_t){
        .effect_id=103u,.speed=128u,.intensity=255u,.brightness=255u,
        .frame_time_ms=16u,.palette_id=0u,
        .foreground={255,0,0},.background={0,255,0},
        .tertiary={0,0,0},.tertiary_set=true
    };
    b=a;
    b.tertiary_set=false;
    b.tertiary=(ot_wled_rgb_t){0,0,0};
    ot_wled_fx_state_t glitter_black={0},glitter_legacy={0};
    unsigned glitter_black_pixels=0u;
    for(uint32_t t=0u;t<512u;t+=16u){
        assert(ot_wled_fx_render(&a,&glitter_black,t,p,257u));
        assert(ot_wled_fx_render(&b,&glitter_legacy,t,q,257u));
        for(unsigned k=0u;k<257u;k++)
            if(eq(p[k],(ot_wled_rgb_t){0,0,0}) &&
               eq(q[k],(ot_wled_rgb_t){255,255,255}))glitter_black_pixels++;
    }
    assert(glitter_black_pixels>0u);
    ot_wled_fx_reset(&glitter_black);ot_wled_fx_reset(&glitter_legacy);

    uint16_t ids[160]={0};
    assert(ot_effects_1d_supported_ids(ids,160u)==140u);
    puts("PASS Issue102: 13 source-proven third-color roles, 6 unsupported metadata slots, explicit black Aurora/Popcorn/Solid Glitter and all 140 IDs (host)");
    return 0;
}
