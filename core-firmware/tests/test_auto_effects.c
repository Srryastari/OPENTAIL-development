#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ot_auto_effects.h"
#include "effects_1d.h"

static int same(ot_wled_rgb_t a,ot_wled_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
int main(void){
    uint16_t ids[8]={0};
    assert(ot_auto_fx_ids(ids,8)==5u);
    assert(ids[0]==OT_FX_MATERIALIZE_SPARKS);
    assert(ids[1]==OT_FX_RANDOM_MATERIALIZE);
    assert(ids[2]==OT_FX_SMOOTH_FADE);
    assert(ids[3]==OT_FX_PLASMA_SWEEP);
    assert(ids[4]==OT_FX_SMOOTH_FADE_OUT);
    assert(ot_auto_fx_duration_ms(255u)==350u);
    assert(ot_auto_fx_duration_ms(0u)==3920u);
    assert(!ot_auto_fx_id(OT_FX_EMBERS));
    assert(!ot_auto_fx_metadata(999u,NULL));
    ot_wled_fx_metadata_t meta={0};
    for(unsigned i=0;i<5;i++){
        assert(ot_auto_fx_metadata(ids[i],&meta));
        assert(meta.id==ids[i]);
        assert(meta.supports_foreground&&meta.supports_background);
    }

    ot_wled_fx_config_t c={.effect_id=OT_FX_SMOOTH_FADE,
       .speed=180,.intensity=255,.brightness=24,.palette_id=0,
       .frame_time_ms=23,.foreground={255,40,0},.background={0,0,0}};
    ot_wled_rgb_t p[66]={0};
    ot_wled_rgb_t out[66]={0};
    const uint32_t dur=ot_auto_fx_duration_ms(c.speed);
    assert(dur==1400u);
    p[64]=(ot_wled_rgb_t){5,6,7};
    p[65]=(ot_wled_rgb_t){8,9,10};
    assert(ot_auto_fx_render(&c,99,0,p,64));
    for(unsigned i=0;i<64;i++)assert(same(p[i],(ot_wled_rgb_t){0,0,0}));
    assert(ot_auto_fx_render(&c,99,dur/2,p,64));
    assert(p[0].r>=10&&p[0].r<=14);
    assert(ot_auto_fx_render(&c,99,dur,p,64));
    for(unsigned i=0;i<64;i++)assert(same(p[i],(ot_wled_rgb_t){24,4,0}));
    assert(same(p[64],(ot_wled_rgb_t){5,6,7}));
    assert(same(p[65],(ot_wled_rgb_t){8,9,10}));
    c.effect_id=OT_FX_SMOOTH_FADE_OUT;
    assert(ot_auto_fx_render(&c,99,0,p,64));
    for(unsigned i=0;i<64;i++)assert(same(p[i],(ot_wled_rgb_t){24,4,0}));
    assert(ot_auto_fx_render(&c,99,dur/2,p,64));
    assert(p[0].r>=10&&p[0].r<=14);
    assert(ot_auto_fx_render(&c,99,dur,p,64));
    for(unsigned i=0;i<64;i++)assert(same(p[i],(ot_wled_rgb_t){0,0,0}));
    assert(ot_auto_fx_render(&c,99,dur+10000u,p,64));
    assert(same(p[0],(ot_wled_rgb_t){0,0,0}));
    assert(!ot_auto_fx_render(&c,99,0,p,0));
    assert(!ot_auto_fx_render(&c,99,0,NULL,64));
    c.palette_id=2;assert(!ot_auto_fx_render(&c,99,0,p,64));c.palette_id=0;
    c.frame_time_ms=0;assert(!ot_auto_fx_render(&c,99,0,p,64));c.frame_time_ms=23;

    for(unsigned ididx=0;ididx<5;ididx++){
        c.effect_id=ids[ididx];
        c.intensity=230;
        ot_effects_1d_state_t state;
        ot_effects_1d_reset(&state,1234);
        assert(ot_effects_1d_metadata(c.effect_id,&meta));
        assert(ot_effects_1d_render(&c,&state,0,p,64));
        memcpy(out,p,sizeof(p));
        ot_effects_1d_state_t repeat;
        ot_effects_1d_reset(&repeat,1234);
        assert(ot_effects_1d_render(&c,&repeat,0,p,64));
        assert(memcmp(p,out,sizeof(p))==0);
        assert(ot_effects_1d_render(&c,&state,dur/2,p,64));
        for(unsigned i=0;i<64;i++){
            assert(p[i].r<=24&&p[i].g<=24&&p[i].b<=24);
        }
        assert(ot_effects_1d_render(&c,&state,dur,p,64));
        for(unsigned i=0;i<64;i++){
            if(c.effect_id==OT_FX_SMOOTH_FADE_OUT)
                assert(same(p[i],(ot_wled_rgb_t){0,0,0}));
            else assert(same(p[i],(ot_wled_rgb_t){24,4,0}));
        }
    }
    /* The capability list contains all original IDs in addition to WLED
       compatibility modes. This must remain within BLE's 127-ID payload. */
    uint16_t catalog[128]={0};
    size_t n=ot_effects_1d_supported_ids(catalog,128);
    assert(n==126u);
    assert(catalog[120]==OT_FX_EMBERS);
    for(unsigned i=0;i<5;i++)assert(catalog[121+i]==ids[i]);
    printf("CORE startup/shutdown effects: five native one-shot frames, all limits, endpoints, BLE capability IDs passed.\n");
    return 0;
}
