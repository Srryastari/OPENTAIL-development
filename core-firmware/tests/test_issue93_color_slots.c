/* Issue #93 authoritative shared RGB slot contract.
 * Host-only: no JNI, UI, BLE, GPIO or frame crossfades.
 * Compare source color roles, exact output colors and all 140 modes.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "effects_1d.h"
#include "ot_auto_effects.h"

static int same(ot_wled_rgb_t a,ot_wled_rgb_t b) {
    return a.r==b.r && a.g==b.g && a.b==b.b;
}
static int found(const ot_wled_rgb_t *frame,size_t count,ot_wled_rgb_t color) {
    for(size_t i=0;i<count;i++)if(same(frame[i],color))return 1;
    return 0;
}
static void expect_roles(uint16_t id,const char *r0,const char *r1,
                         const char *r2) {
    ot_wled_fx_color_role_t roles[3];
    assert(ot_effects_1d_color_roles(id,roles));
    const char *expected[3]={r0,r1,r2};
    for(unsigned j=0;j<3;j++){
        assert(roles[j].enabled==(expected[j]!=NULL));
        if(expected[j])assert(strcmp(roles[j].label,expected[j])==0);
    }
}
int main(void){
    static ot_wled_rgb_t pixels[260];
    const ot_wled_rgb_t red={255,0,0},green={0,255,0},
                       blue={0,0,255},white={255,255,255},
                       black={0,0,0},guard={29,47,83};
    ot_wled_fx_config_t cfg={
        .speed=128,.intensity=255,.brightness=255,.palette_id=0,
        .frame_time_ms=16,.foreground={255,0,0},.background={0,255,0},
        .tertiary={0,0,255},.tertiary_set=true
    };
    ot_wled_fx_state_t st={0};
    expect_roles(OT_WLED_TRICOLOR_CHASE,"1","2","3");
    expect_roles(OT_WLED_TRICOLOR_WIPE,"1","2","3");
    expect_roles(OT_WLED_TRICOLOR_FADE,"1","2","3");
    expect_roles(OT_WLED_SOLID_GLITTER,"BG",NULL,"GL");
    expect_roles(OT_WLED_RAILWAY,"1","2",NULL);
    assert(!ot_wled_fx_color_roles(65535u,(ot_wled_fx_color_role_t[3]){{0}}));

    cfg.effect_id=OT_WLED_TRICOLOR_CHASE;
    assert(ot_wled_fx_render(&cfg,&st,0u,pixels,97u));
    assert(found(pixels,97u,red)&&found(pixels,97u,green)&&found(pixels,97u,blue));
    ot_wled_fx_reset(&st);
    cfg.effect_id=OT_WLED_TRICOLOR_WIPE;
    const uint32_t wipe_cycle=1000u+(255u-(uint32_t)cfg.speed)*200u;
    assert(ot_wled_fx_render(&cfg,&st,wipe_cycle/2u,pixels,97u));
    assert(found(pixels,97u,blue)&&found(pixels,97u,green));
    assert(ot_wled_fx_render(&cfg,&st,(wipe_cycle*5u)/6u,pixels,97u));
    assert(found(pixels,97u,blue)&&found(pixels,97u,red));
    ot_wled_fx_reset(&st);
    cfg.effect_id=OT_WLED_TRICOLOR_FADE;cfg.speed=0u;
    assert(ot_wled_fx_render(&cfg,&st,0u,pixels,97u));
    assert(same(pixels[0],red));
    assert(ot_wled_fx_render(&cfg,&st,21846u,pixels,97u));
    assert(same(pixels[0],green));
    assert(ot_wled_fx_render(&cfg,&st,43691u,pixels,97u));
    assert(same(pixels[0],blue));
    ot_wled_fx_reset(&st);

    cfg.effect_id=OT_WLED_SOLID_GLITTER;
    cfg.intensity=255u;
    int saw_glitter=0;
    for(unsigned k=0;k<16u;k++){
        assert(ot_wled_fx_render(&cfg,&st,k*16u,pixels,97u));
        for(size_t i=0;i<97u;i++)
            assert(same(pixels[i],red)||same(pixels[i],blue));
        saw_glitter|=found(pixels,97u,blue);
    }
    assert(saw_glitter);
    cfg.tertiary=black;
    assert(ot_wled_fx_render(&cfg,&st,512u,pixels,97u));
    for(size_t i=0;i<97u;i++)
        assert(same(pixels[i],red)||same(pixels[i],black));
    /* Legacy calls with zero-initialized tertiary retain white glitter. */
    cfg.tertiary_set=false;
    saw_glitter=0;
    for(unsigned k=0;k<16u;k++){
        assert(ot_wled_fx_render(&cfg,&st,1024u+k*16u,pixels,97u));
        for(size_t i=0;i<97u;i++)
            assert(same(pixels[i],red)||same(pixels[i],white));
        saw_glitter|=found(pixels,97u,white);
    }
    assert(saw_glitter);
    ot_wled_fx_reset(&st);

    uint16_t ids[160]={0};
    const size_t total=ot_effects_1d_supported_ids(ids,160u);
    assert(total==140u);
    /* Metadata/slot availability, 0/255 absolute brightness, 5 Speed
     * extremes and 3 Intensity profiles for every registered effect. A
     * rejected render or overflow is never counted as a PASS. */
    const uint8_t speeds[]={0u,1u,128u,254u,255u};
    const uint8_t intensities[]={0u,128u,255u};
    uint32_t matrix=0u;
    for(size_t i=0;i<total;i++){
        ot_wled_fx_metadata_t meta;
        ot_wled_fx_color_role_t roles[3];
        assert(ot_effects_1d_metadata(ids[i],&meta));
        assert(meta.id==ids[i]);
        assert(ot_effects_1d_color_roles(ids[i],roles));
        for(unsigned j=0;j<3;j++){
            if(roles[j].enabled)assert(roles[j].label[0]!='\0');
            else assert(roles[j].label[0]=='\0');
        }
        /* Only metadata can advertise palette capability. No unsupported
         * 73-255 fallback swatches are fabricated. */
        cfg.effect_id=ids[i];cfg.tertiary=blue;
        cfg.tertiary_set=false;cfg.palette_id=0u;
        for(unsigned x=0;x<5u;x++){
            for(unsigned y=0;y<3u;y++){
                ot_effects_1d_state_t state={0};
                ot_effects_1d_reset(&state,0x12345678u);
                cfg.speed=speeds[x];cfg.intensity=intensities[y];
                cfg.brightness=0u;
                pixels[257]=guard;
                assert(ot_effects_1d_render(&cfg,&state,0u,pixels,257u));
                for(size_t k=0;k<257u;k++)assert(same(pixels[k],black));
                assert(same(pixels[257],guard));
                cfg.brightness=255u;
                if(!ot_effects_1d_render(&cfg,&state,17u,pixels,257u)){
                    fprintf(stderr,"Issue93 unsupported mode ID=%u Speed=%u Intensity=%u\\n",
                        (unsigned)ids[i],cfg.speed,cfg.intensity);
                    assert(0 && "full catalog frame render admission");
                }
                assert(same(pixels[257],guard));
                ot_effects_1d_reset(&state,0u);
                matrix++;
            }
        }
    }
    assert(matrix==2100u);
    printf("PASS Issue93 shared color roles + source rendering: 4 named effects, 140 IDs, %u speed/intensity profiles; no unconfigured slot 3/true-black lifting (host only)\n",matrix);
    return 0;
}
