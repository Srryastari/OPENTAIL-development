#include <assert.h>
#include <string.h>
#include "fx_overlay_engine.h"

static size_t lit(const ot_wled_rgb_t *p,size_t n){
    size_t total=0;
    for(size_t i=0;i<n;i++)if(p[i].r||p[i].g||p[i].b)total++;
    return total;
}
int main(void){
    ot_fx_overlay_config_t cfg={
        .style=OT_FX_OVERLAY_SPARKS,.peak=32,.density=0,
        .radius=3,.fade_ms=120,.color={255,255,255}};
    assert(ot_fx_overlay_valid(&cfg));
    ot_wled_rgb_t frame[623]={0};
    ot_wled_rgb_t repeat[623]={0};
    assert(ot_fx_overlay_apply(&cfg,42,0,frame,623,24));
    assert(lit(frame,623)>0);
    for(size_t i=0;i<623;i++){
        assert(frame[i].r<=32&&frame[i].g<=32&&frame[i].b<=32);
    }
    assert(ot_fx_overlay_apply(&cfg,42,0,repeat,623,24));
    assert(memcmp(frame,repeat,sizeof(frame))==0);
    memset(repeat,0,sizeof(repeat));
    assert(ot_fx_overlay_apply(&cfg,42,60,repeat,623,24));
    assert(lit(repeat,623)>0);
    memset(repeat,0,sizeof(repeat));
    assert(ot_fx_overlay_apply(&cfg,42,120,repeat,623,24));
    assert(lit(repeat,623)==0); /* same epoch; one-shot fade completed */
    memset(repeat,0,sizeof(repeat));
    assert(ot_fx_overlay_apply(&cfg,42,0,repeat,623,0));
    assert(lit(repeat,623)==0); /* brightness 0 must stay truly black */
    cfg.style=OT_FX_OVERLAY_FLARES;
    memset(repeat,0,sizeof(repeat));
    assert(ot_fx_overlay_apply(&cfg,42,0,repeat,623,24));
    assert(lit(repeat,623)>1);
    cfg.style=OT_FX_OVERLAY_SPARKS_FLARES;
    ot_wled_rgb_t colored[5]={0};
    cfg.color=(ot_wled_rgb_t){0,255,16};
    cfg.density=255;
    assert(ot_fx_overlay_apply(&cfg,1234,75,colored,5,24));
    for(size_t i=0;i<5;i++){
        assert(colored[i].r==0);
        assert(colored[i].g<=32&&colored[i].b<=32);
    }
    /* Never overrun caller's zone. No assumptions about left/right or counts. */
    struct {ot_wled_rgb_t before, zone[1], after;} guarded={{5,4,3},{{0,0,0}},{6,7,8}};
    assert(ot_fx_overlay_apply(&cfg,11,0,guarded.zone,1,24));
    assert(guarded.before.r==5&&guarded.after.b==8);
    assert(ot_fx_overlay_apply(&cfg,11,0,guarded.zone,1,24));
    assert(guarded.zone[0].g<=32);
    /* Never darken existing frame; especially critical under trigger overlays. */
    ot_wled_rgb_t base[12];
    for(size_t i=0;i<12;i++)base[i]=(ot_wled_rgb_t){30,5,7};
    assert(ot_fx_overlay_apply(&cfg,99,0,base,12,24));
    for(size_t i=0;i<12;i++){
        assert(base[i].r>=30&&base[i].g>=5&&base[i].b>=7);
    }
    cfg.radius=13;
    assert(!ot_fx_overlay_valid(&cfg));
    assert(!ot_fx_overlay_apply(&cfg,9,0,frame,623,24));
    cfg.radius=4;cfg.fade_ms=39;
    assert(!ot_fx_overlay_valid(&cfg));
    cfg.fade_ms=1001;
    assert(!ot_fx_overlay_valid(&cfg));
    cfg.fade_ms=120;cfg.peak=33;
    assert(!ot_fx_overlay_valid(&cfg));
    cfg.peak=32;cfg.style=4;
    assert(!ot_fx_overlay_valid(&cfg));
    cfg.style=OT_FX_OVERLAY_OFF;
    memset(frame,0,sizeof(frame));
    assert(ot_fx_overlay_apply(&cfg,11,0,frame,623,24));
    assert(!lit(frame,623));
    assert(!ot_fx_overlay_apply(&cfg,11,0,frame,0,24));
    return 0;
}
