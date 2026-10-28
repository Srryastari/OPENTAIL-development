#include "effects_1d.h"
#include "palette_1d.h"
#include "wled_1d.h"
#include <string.h>
/* Unified registry: source-pinned WLED adapters first, then the 1D
 * compatibility catalog, then the original OpenTail Embers effect. */
size_t ot_effects_1d_supported_ids(uint16_t *dst,size_t capacity){
    const size_t count=ot_wled_1d_count();
    for(size_t i=0;dst&&i<count&&i<capacity;i++)
        dst[i]=ot_wled_1d_at(i)->id;
    if(dst&&capacity>count)dst[count]=OT_FX_EMBERS;
    return count+1u;
}
bool ot_effects_1d_metadata(uint16_t id,ot_wled_fx_metadata_t *meta){
    if(!meta)return false;
    if(id==OT_FX_EMBERS){
        *meta=(ot_wled_fx_metadata_t){OT_FX_EMBERS,"Embers",
            "Embers@Speed,Density;Color;!;1",
            true,false,true,true,false};
        return true;
    }
    /* Prefer source-derived WLED implementations where they exist. */
    if(ot_wled_fx_metadata(id,meta))return true;
    const ot_wled_1d_meta_t *compat=ot_wled_1d_by_id(id);
    if(!compat)return false;
    *meta=(ot_wled_fx_metadata_t){compat->id,compat->name,compat->name,
        true,true,true,true,false};
    return true;
}
void ot_effects_1d_reset(ot_effects_1d_state_t *s,uint32_t seed){
    if(!s)return;
    memset(s,0,sizeof(*s));
    ot_particles_1d_reset(&s->particles,seed);
}
static uint32_t rng(ot_particles_1d_t *s){
    uint32_t x=s->seed?s->seed:1u;
    x^=x<<13;x^=x>>17;x^=x<<5;s->seed=x;
    return x;
}
static ot_wled_rgb_t scale_compat(ot_rgb_t c,uint8_t brightness){
    return (ot_wled_rgb_t){
        (uint8_t)((uint16_t)c.r*brightness/255u),
        (uint8_t)((uint16_t)c.g*brightness/255u),
        (uint8_t)((uint16_t)c.b*brightness/255u)};
}
bool ot_effects_1d_render(const ot_wled_fx_config_t *cfg,
    ot_effects_1d_state_t *s,uint32_t ms,ot_wled_rgb_t *frame,size_t count){
    if(!cfg||!s||!frame||count==0||count>65535u||cfg->frame_time_ms==0||
       !ot_palette_1d_supported(cfg->effect_id,cfg->palette_id))return false;
    if(cfg->effect_id!=OT_FX_EMBERS){
        /* Six effects use source-derived WLED adapters. The remainder use
         * bounded deterministic 1D recipes; catalog names are not a claim of
         * pixel-identical parity with the complete upstream WLED renderer. */
        ot_wled_fx_metadata_t source_meta;
        if(ot_wled_fx_metadata(cfg->effect_id,&source_meta))
            return ot_wled_fx_render(cfg,&s->wled,ms,frame,count);
        const ot_wled_1d_meta_t *compat=ot_wled_1d_by_id(cfg->effect_id);
        if(!compat||cfg->palette_id!=0u)return false;
        for(size_t px=0;px<count;px++){
            ot_rgb_t c=ot_wled_1d_pixel(cfg->effect_id,(unsigned)px,
                (unsigned)count,ms,cfg->speed,cfg->intensity,
                (ot_rgb_t){cfg->foreground.r,cfg->foreground.g,cfg->foreground.b},
                (ot_rgb_t){cfg->background.r,cfg->background.g,cfg->background.b},
                (ot_rgb_t){cfg->foreground.r,cfg->foreground.g,cfg->foreground.b});
            frame[px]=scale_compat(c,cfg->brightness);
        }
        return true;
    }
    if(!s->particles.seed)ot_particles_1d_reset(&s->particles,1u);
    if(!s->initialized){s->initialized=true;s->previous_ms=ms;}
    uint32_t delta=ms-s->previous_ms;
    if(delta>100u)delta=100u;
    s->previous_ms=ms;
    if(delta>0){
        ot_particles_1d_step(&s->particles,(uint16_t)delta,(uint16_t)count,
                             0,OT_PARTICLE_WRAP);
        uint32_t density=1u+((uint32_t)cfg->intensity*32u/255u);
        s->emit_accumulator+=delta*density;
        unsigned emitted=0;
        while(s->emit_accumulator>=1000u&&emitted<4u){
            uint32_t random=rng(&s->particles);
            uint32_t index=random%(uint32_t)count;
            int32_t velocity=(int32_t)(30u+cfg->speed*3u);
            if(random&0x10000u)velocity=-velocity;
            uint16_t lifetime=(uint16_t)(350u+((random>>12)%1150u));
            ot_particles_1d_emit(&s->particles,(int32_t)(index*256u),velocity,
                lifetime,ot_palette_1d_sample(cfg->palette_id,
                    cfg->foreground,cfg->background,(uint8_t)(random>>8)));
            s->emit_accumulator-=1000u;emitted++;
        }
        if(s->emit_accumulator>=1000u)s->emit_accumulator%=1000u;
    }
    const ot_wled_rgb_t bg={0,0,0};
    for(size_t i=0;i<count;i++)frame[i]=bg;
    ot_particles_1d_render_smooth(&s->particles,frame,count,cfg->brightness,false);
    return true;
}
