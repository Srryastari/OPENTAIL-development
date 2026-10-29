/* Tests the deterministic WLED Blink/Strobe timing adaptation. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "wled_fx_compat.h"

static int eq(ot_wled_rgb_t a,ot_wled_rgb_t b) {
    return a.r==b.r && a.g==b.g && a.b==b.b;
}

int main(void) {
    uint16_t ids[10]={0};
    assert(ot_wled_fx_supported_ids(ids,10)==9);
    assert(ids[0]==0 && ids[1]==1 && ids[2]==3 && ids[3]==6 &&
           ids[4]==10 && ids[5]==11 && ids[6]==12 &&
           ids[7]==13 && ids[8]==23);
    ot_wled_fx_metadata_t meta={0};
    assert(ot_wled_fx_metadata(0,&meta) && !strcmp(meta.name,"Solid") && !meta.supports_background);
    assert(ot_wled_fx_metadata(1,&meta) && !strcmp(meta.name,"Blink") && meta.supports_intensity);
    assert(ot_wled_fx_metadata(23,&meta) && !strcmp(meta.name,"Strobe") && !meta.supports_intensity);
    assert(!ot_wled_fx_metadata(255,&meta));
    assert(!ot_wled_fx_metadata(1,NULL));

    ot_wled_fx_config_t c={
        .effect_id=OT_WLED_BLINK,.speed=128,.intensity=128,.brightness=255,
        .palette_id=0,.frame_time_ms=23,
        .foreground={200,20,5},.background={0,3,40}
    };
    ot_wled_fx_state_t s={0};
    ot_wled_rgb_t p[31]={0};
    /* (255-128)*20=2540; 2540+23*2=2586; on=23+(2540*128>>8)=1293 */
    assert(ot_wled_fx_render(&c,&s,0,p,31) && eq(p[0],c.foreground));
    assert(eq(p[0],p[30]));
    assert(ot_wled_fx_render(&c,&s,1293,p,31) && eq(p[2],c.foreground));
    assert(ot_wled_fx_render(&c,&s,1294,p,31) && eq(p[2],c.background));
    assert(ot_wled_fx_render(&c,&s,2585,p,31) && eq(p[30],c.background));
    assert(ot_wled_fx_render(&c,&s,2586,p,31) && eq(p[15],c.foreground));

    ot_wled_fx_reset(&s);
    /* Missing the opening frame still flashes for a single frame on new iteration,
       matching WLED's SEGENV.step protection. */
    assert(ot_wled_fx_render(&c,&s,3880,p,31) && eq(p[0],c.foreground));
    assert(ot_wled_fx_render(&c,&s,3881,p,31) && eq(p[0],c.background));

    c.effect_id=OT_WLED_STROBE;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,23,p,31) && eq(p[8],c.foreground));
    assert(ot_wled_fx_render(&c,&s,24,p,31) && eq(p[8],c.background));

    c.effect_id=OT_WLED_SOLID;c.brightness=127;
    assert(ot_wled_fx_render(&c,&s,1200,p,31));
    assert(eq(p[0],(ot_wled_rgb_t){100,10,2}));

    /* Pinned upstream WLED FX.cpp deterministic 1D golden math:
     * FADE triangular wave period, Wipe/Sweep in/out positions, and
     * single-pixel upstream fallback to Solid (no palette enabled). */
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    c.brightness=255;c.speed=0;c.intensity=0;
    c.effect_id=OT_WLED_FADE;
    assert(ot_wled_fx_metadata(OT_WLED_FADE,&meta) && !meta.supports_intensity);
    assert(ot_wled_fx_render(&c,&s,0,p,4) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,3277,p,4) && eq(p[0],c.foreground));
    assert(eq(p[0],p[3]));
    assert(ot_wled_fx_render(&c,&s,6554,p,4) && p[0].r<2 && p[0].b>253);
    /* Wipe has a full cycle of 750 ms at speed=255. */
    c.effect_id=OT_WLED_WIPE;c.speed=255;
    assert(ot_wled_fx_metadata(OT_WLED_WIPE,&meta) && meta.supports_intensity);
    assert(ot_wled_fx_render(&c,&s,0,p,4));
    for(int k=0;k<4;k++)assert(eq(p[k],c.background));
    assert(ot_wled_fx_render(&c,&s,375,p,4));
    for(int k=0;k<4;k++)assert(eq(p[k],c.foreground));
    assert(ot_wled_fx_render(&c,&s,562,p,4));
    assert(eq(p[0],c.background) && eq(p[3],c.foreground));
    c.effect_id=OT_WLED_SWEEP;
    assert(ot_wled_fx_metadata(OT_WLED_SWEEP,&meta));
    assert(ot_wled_fx_render(&c,&s,562,p,4));
    assert(eq(p[0],c.foreground) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,562,p,1) && eq(p[0],c.foreground));
    /* Nontrivial fade at the partially covered wipe pixel. */
    c.effect_id=OT_WLED_WIPE;c.intensity=255;
    assert(ot_wled_fx_render(&c,&s,188,p,4));
    assert(eq(p[0],c.foreground) && p[2].r>0 && p[2].r<255);
    /* Pure metadata/renderer feature gating: unsupported palette fails. */
    c.palette_id=3;assert(!ot_wled_fx_render(&c,&s,188,p,4));
    c.palette_id=0;
    assert(!ot_wled_fx_render(&c,&s,188,p,65536u));

    /*
     * Pinned upstream FX.cpp scan(bool): with speed=255, intensity=0,
     * one illuminated pixel makes a round trip over a 4-pixel zone.
     * Dual Scan mirrors that exact logical position.
     */
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    c.brightness=255;c.speed=255;c.intensity=0;
    c.effect_id=OT_WLED_SCAN;
    assert(ot_wled_fx_metadata(OT_WLED_SCAN,&meta) && meta.supports_background);
    assert(ot_wled_fx_render(&c,&s,0,p,4));
    assert(eq(p[3],c.foreground) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,375,p,4));
    assert(eq(p[1],c.foreground) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,376,p,4));
    assert(eq(p[0],c.foreground) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,749,p,4));
    assert(eq(p[2],c.foreground));
    c.effect_id=OT_WLED_DUAL_SCAN;
    assert(ot_wled_fx_render(&c,&s,0,p,4));
    assert(eq(p[0],c.foreground) && eq(p[3],c.foreground));
    assert(eq(p[1],c.background) && eq(p[2],c.background));
    assert(ot_wled_fx_render(&c,&s,375,p,4));
    assert(eq(p[1],c.foreground) && eq(p[2],c.foreground));
    assert(eq(p[0],c.background) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,10,p,1) && eq(p[0],c.foreground));

    /* Pinned upstream running(..., theatre=true): render before advancing
     * SEGENV.aux0. Skipping an interval advances only one step per frame. */
    c.effect_id=OT_WLED_THEATER;c.intensity=0;c.speed=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[0],c.foreground) && eq(p[3],c.foreground));
    assert(eq(p[1],c.background));
    assert(ot_wled_fx_render(&c,&s,50,p,7));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background));
    assert(s.step==1 && s.aux0==1);
    assert(ot_wled_fx_render(&c,&s,51,p,7));
    assert(eq(p[1],c.foreground) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,250,p,7));
    assert(eq(p[1],c.foreground) && s.aux0==2);
    ot_wled_fx_reset(&s);
    assert(s.step==0 && s.aux0==0);
    /* Brightness and a nonzero intensity (wider scan dot) stay zone-local. */
    c.effect_id=OT_WLED_SCAN;c.intensity=255;c.brightness=127;
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[6],(ot_wled_rgb_t){127,0,0}));
    assert(eq(p[0],(ot_wled_rgb_t){0,0,127}));

    c.effect_id=42;assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.effect_id=OT_WLED_BLINK;c.palette_id=1;
    assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.palette_id=0;c.frame_time_ms=0;assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.frame_time_ms=23;assert(!ot_wled_fx_render(&c,&s,0,p,0));

    /* High-speed edge case: cycle remains nonzero even at speed 255. */
    c.speed=255;c.intensity=255;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,45,p,31));
    assert(ot_wled_fx_render(&c,&s,46,p,31));
    puts("CORE source-derived effects: 9 modes, timing, zone limits and validation passed.");
    return 0;
}
