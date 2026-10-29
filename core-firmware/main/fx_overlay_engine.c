#include "fx_overlay_engine.h"

static uint32_t hash32(uint32_t x){
    x^=x>>16;x*=0x7feb352du;
    x^=x>>15;x*=0x846ca68bu;
    x^=x>>16;return x;
}
static uint8_t max8(uint8_t a,uint8_t b){return a>b?a:b;}
bool ot_fx_overlay_valid(const ot_fx_overlay_config_t *cfg){
    return cfg && cfg->style<=OT_FX_OVERLAY_SPARKS_FLARES &&
           cfg->peak<=32u && cfg->radius<=12u &&
           (cfg->fade_ms==0u||(cfg->fade_ms>=40u&&cfg->fade_ms<=1000u));
}
static void put(ot_wled_rgb_t *dst,ot_wled_rgb_t color,uint32_t energy){
    /* The overlay never raises a channel above the established bench cap. */
    const uint8_t r=(uint8_t)(color.r*energy/255u);
    const uint8_t g=(uint8_t)(color.g*energy/255u);
    const uint8_t b=(uint8_t)(color.b*energy/255u);
    dst->r=max8(dst->r,r);
    dst->g=max8(dst->g,g);
    dst->b=max8(dst->b,b);
}
bool ot_fx_overlay_apply(const ot_fx_overlay_config_t *cfg,
                          uint32_t seed,uint32_t elapsed_ms,
                          ot_wled_rgb_t *pixels,size_t count,
                          uint8_t effect_brightness){
    if(!ot_fx_overlay_valid(cfg)||!pixels||!count||count>65535u)return false;
    if(cfg->style==OT_FX_OVERLAY_OFF||!effect_brightness)return true;
    const uint32_t period=70u+(uint32_t)(255u-cfg->density)*530u/255u;
    const uint32_t life=cfg->fade_ms?cfg->fade_ms:120u;
    const uint32_t maxpeak=cfg->peak?cfg->peak:32u;
    const unsigned radius=cfg->radius?cfg->radius:4u;
    const ot_wled_rgb_t color=(cfg->color.r||cfg->color.g||cfg->color.b)
        ?cfg->color:(ot_wled_rgb_t){255u,255u,255u};
    /* At most 15 overlapping events when fade=1000 and period=70.
     * Their absolute time slots remove frame-rate-dependent RNG/particles. */
    const uint32_t epoch=elapsed_ms/period;
    for(unsigned behind=0u;behind<15u;behind++){
        const uint32_t lag=behind*period;
        if(lag>elapsed_ms)break;
        const uint32_t age=elapsed_ms%period+lag;
        if(age>=life)break;
        const uint32_t event=epoch-behind;
        const uint32_t hash=hash32(seed^(event*0x9e3779b9u)^0xa5c33d97u);
        const size_t center=(size_t)(hash%(uint32_t)count);
        const uint32_t peak=(maxpeak*(life-age)+life/2u)/life;
        if(!peak)continue;
        if(cfg->style&OT_FX_OVERLAY_FLARES){
            const size_t start=center>radius?center-radius:0u;
            const size_t end=(count-1u-center)<radius?count-1u:center+radius;
            for(size_t i=start;i<=end;i++){
                const size_t distance=i>center?i-center:center-i;
                const uint32_t energy=peak*(radius+1u-(uint32_t)distance)/(radius+1u);
                put(&pixels[i],color,energy);
            }
        }
        if(cfg->style&OT_FX_OVERLAY_SPARKS){
            put(&pixels[center],color,peak);
            /* A tiny pale glint to either side, without wrapping zones. */
            if(center>0)put(&pixels[center-1],color,peak/4u);
            if(center+1u<count)put(&pixels[center+1],color,peak/4u);
        }
    }
    return true;
}
