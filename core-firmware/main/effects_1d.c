#include "effects_1d.h"
#include <string.h>
/* Shared list: source-derived WLED implementations first, private original
 * Ember recipe last. No "Particle Engine" category or duplicate effect. */
size_t ot_effects_1d_supported_ids(uint16_t *dst,size_t capacity){
    size_t upstream=ot_wled_fx_supported_ids(dst,capacity);
    if(dst && capacity>upstream)dst[upstream]=OT_FX_EMBERS;
    return upstream+1u;
}
bool ot_effects_1d_metadata(uint16_t id,ot_wled_fx_metadata_t *meta){
    if(!meta)return false;
    if(id==OT_FX_EMBERS){
        *meta=(ot_wled_fx_metadata_t){OT_FX_EMBERS,"Embers",
            "Embers@Speed,Density;Color;!;1",
            true,false,true,true,false};
        return true;
    }
    return ot_wled_fx_metadata(id,meta);
}
void ot_effects_1d_reset(ot_effects_1d_state_t *s,uint32_t seed){
    if(!s)return;
    memset(s,0,sizeof(*s));
    ot_particles_1d_reset(&s->particles,seed);
}
static uint32_t rng(ot_particles_1d_t *s){
    uint32_t x=s->seed? s->seed:1u;
    x^=x<<13; x^=x>>17; x^=x<<5;s->seed=x;
    return x;
}
bool ot_effects_1d_render(const ot_wled_fx_config_t *cfg,
    ot_effects_1d_state_t *s,uint32_t ms,ot_wled_rgb_t *frame,size_t count){
    if(!cfg||!s||!frame||count==0||count>65535u||cfg->frame_time_ms==0||
        cfg->palette_id!=0)return false;
    if(cfg->effect_id!=OT_FX_EMBERS)
        return ot_wled_fx_render(cfg,&s->wled,ms,frame,count);
    if(!s->particles.seed)ot_particles_1d_reset(&s->particles,1u);
    if(!s->initialized){s->initialized=true;s->previous_ms=ms;}
    uint32_t delta=ms-s->previous_ms;
    if(delta>100u)delta=100u; /* bounded catch-up, never burn arbitrarily large CPU */
    s->previous_ms=ms;
    if(delta>0){
        ot_particles_1d_step(&s->particles,(uint16_t)delta,(uint16_t)count,
                             0,OT_PARTICLE_WRAP);
        /* 1..33 births/second depending on density, with hard pool limit. */
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
                lifetime,cfg->foreground);
            s->emit_accumulator-=1000u;
            emitted++;
        }
        if(s->emit_accumulator>=1000u)s->emit_accumulator%=1000u;
    }
    const ot_wled_rgb_t bg={0,0,0};
    for(size_t i=0;i<count;i++)frame[i]=bg;
    ot_particles_1d_render_smooth(&s->particles,frame,count,cfg->brightness,false);
    return true;
}
