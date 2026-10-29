/*
 * OpenTail original one-shot startup/shutdown effects.
 * Independent of upstream WLED and all hardware output drivers. Runtime is
 * bounded by caller-selected universal output/zone pixels, without allocations.
 * This bench preview does NOT turn the controller off, arm ignition/ACC inputs,
 * persist automatic events, or bypass physical trigger arbitration.
 */
#include "ot_auto_effects.h"

static const uint16_t ids[] = {
    OT_FX_MATERIALIZE_SPARKS, OT_FX_RANDOM_MATERIALIZE,
    OT_FX_SMOOTH_FADE, OT_FX_PLASMA_SWEEP, OT_FX_SMOOTH_FADE_OUT
};

bool ot_auto_fx_id(uint16_t id) {
    for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);i++)
        if(ids[i]==id)return true;
    return false;
}

size_t ot_auto_fx_ids(uint16_t *dst,size_t capacity){
    const size_t n=sizeof(ids)/sizeof(ids[0]);
    if(dst)for(size_t i=0;i<n&&i<capacity;i++)dst[i]=ids[i];
    return n;
}
bool ot_auto_fx_metadata(uint16_t id,ot_wled_fx_metadata_t *out){
    if(!out)return false;
    const char *name=NULL;
    switch(id){
      case OT_FX_MATERIALIZE_SPARKS:name="Materialize Sparks";break;
      case OT_FX_RANDOM_MATERIALIZE:name="Random Materialize";break;
      case OT_FX_SMOOTH_FADE:name="Smooth Fade";break;
      case OT_FX_PLASMA_SWEEP:name="Plasma Sweep";break;
      case OT_FX_SMOOTH_FADE_OUT:name="Smooth Fade Out";break;
      default:return false;
    }
    *out=(ot_wled_fx_metadata_t){id,name,name,true,true,true,true,false};
    return true;
}

/* 350-3920 ms in a single shot; common speed 180 -> 1400 ms. */
uint32_t ot_auto_fx_duration_ms(uint8_t speed){
    return 350u+(255u-(uint32_t)speed)*14u;
}
static uint32_t hash32(uint32_t x){
    x^=x>>16;x*=0x7feb352du;
    x^=x>>15;x*=0x846ca68bu;
    x^=x>>16;return x;
}
static uint8_t mix8(uint8_t a,uint8_t b,uint8_t amount){
    return (uint8_t)(((uint32_t)a*(255u-amount)+(uint32_t)b*amount+127u)/255u);
}
static ot_wled_rgb_t blend(ot_wled_rgb_t a,ot_wled_rgb_t b,uint8_t amt){
    return (ot_wled_rgb_t){
        mix8(a.r,b.r,amt),mix8(a.g,b.g,amt),mix8(a.b,b.b,amt)
    };
}
static ot_wled_rgb_t scale(ot_wled_rgb_t c,uint8_t bri){
    return (ot_wled_rgb_t){
        (uint8_t)(((uint32_t)c.r*bri+127u)/255u),
        (uint8_t)(((uint32_t)c.g*bri+127u)/255u),
        (uint8_t)(((uint32_t)c.b*bri+127u)/255u)
    };
}
static ot_wled_rgb_t wheel(uint8_t v){
    const unsigned sector=(unsigned)v*6u;
    const uint8_t k=(uint8_t)((sector%256u)*255u/256u);
    switch(sector>>8){
      case 0:return (ot_wled_rgb_t){255,k,0};
      case 1:return (ot_wled_rgb_t){(uint8_t)(255u-k),255,0};
      case 2:return (ot_wled_rgb_t){0,255,k};
      case 3:return (ot_wled_rgb_t){0,(uint8_t)(255u-k),255};
      case 4:return (ot_wled_rgb_t){k,0,255};
      default:return (ot_wled_rgb_t){255,0,(uint8_t)(255u-k)};
    }
}
static uint8_t smoothstep(uint32_t p){
    /* smoothstep, with exact 0/255 endpoints and no floating-point state. */
    if(p>=255u)return 255u;
    uint32_t q=p*p*(765u-2u*p);
    return (uint8_t)((q+32512u)/65025u);
}
bool ot_auto_fx_render(const ot_wled_fx_config_t *cfg,uint32_t seed,
                       uint32_t ms,ot_wled_rgb_t *pixels,size_t count){
    if(!cfg||!pixels||!ot_auto_fx_id(cfg->effect_id)||count==0||
       count>65535u||cfg->frame_time_ms==0u||cfg->palette_id!=0u)
        return false;
    const uint32_t duration=ot_auto_fx_duration_ms(cfg->speed);
    const uint32_t elapsed=ms>=duration?duration:ms;
    const uint32_t phase=(elapsed*255u)/duration;
    const ot_wled_rgb_t fg=cfg->foreground,bg=cfg->background;
    if(cfg->effect_id==OT_FX_SMOOTH_FADE ||
       cfg->effect_id==OT_FX_SMOOTH_FADE_OUT){
        const uint8_t k=smoothstep(phase);
        const ot_wled_rgb_t from=cfg->effect_id==OT_FX_SMOOTH_FADE?bg:fg;
        const ot_wled_rgb_t to=cfg->effect_id==OT_FX_SMOOTH_FADE?fg:bg;
        const ot_wled_rgb_t out=scale(blend(from,to,k),cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=out;
        return true;
    }
    if(ms>=duration){
        const ot_wled_rgb_t out=scale(fg,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=out;
        return true;
    }
    for(size_t i=0;i<count;i++){
        const uint32_t h=hash32((uint32_t)i ^ (seed?seed:1u) ^ 0x9e3779b9u);
        ot_wled_rgb_t color=bg;
        if(cfg->effect_id==OT_FX_PLASMA_SWEEP){
            const uint32_t front=(elapsed*(uint32_t)count)/duration;
            if(i<=front){
                const uint8_t gap=(uint8_t)((front-i)*21u+(elapsed/12u));
                const uint8_t wave=(gap&128u)?(uint8_t)(255u-(uint8_t)(gap<<1)):
                                             (uint8_t)(gap<<1);
                color=blend(bg,fg,(uint8_t)(128u+(wave>>1)));
            }
        } else {
            /* Hash-order progressive reveal: when completed every pixel is
             * foreground. Spark lives 120ms, with a minimum useful 40ms
             * to preserve a visible glow at high speed. */
            const uint32_t reveal=(h%duration);
            if(elapsed>=reveal){
                const uint32_t age=elapsed-reveal;
                const uint32_t sparkle_ms=120u;
                color=fg;
                if(age<sparkle_ms && cfg->intensity){
                    const uint8_t fade=(uint8_t)(((sparkle_ms-age)*cfg->intensity)/sparkle_ms);
                    ot_wled_rgb_t spark=fg;
                    if(cfg->effect_id==OT_FX_RANDOM_MATERIALIZE){
                        spark=(h>>24)>239u?(ot_wled_rgb_t){255,255,255}:
                             wheel((uint8_t)(h>>8));
                    }
                    color=blend(fg,spark,fade);
                }
            }
        }
        pixels[i]=scale(color,cfg->brightness);
    }
    return true;
}
