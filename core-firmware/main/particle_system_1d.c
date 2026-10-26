#include "particle_system_1d.h"
#include <limits.h>
#include <string.h>
static int32_t saturate_i32(int64_t n){
    return n<INT32_MIN?INT32_MIN:n>INT32_MAX?INT32_MAX:(int32_t)n;
}
void ot_particles_1d_reset(ot_particles_1d_t *sys,uint32_t seed){
    if(!sys)return;
    memset(sys,0,sizeof(*sys));
    sys->seed=seed?seed:1u;
}
size_t ot_particles_1d_count(const ot_particles_1d_t *sys){
    if(!sys)return 0;
    size_t count=0;
    for(size_t i=0;i<OT_PARTICLES_1D_MAX;i++)if(sys->particles[i].alive)count++;
    return count;
}
bool ot_particles_1d_emit(ot_particles_1d_t *sys,int32_t x_q8,int32_t v_q8,
                          uint16_t life_ms,ot_wled_rgb_t color){
    if(!sys || life_ms==0)return false;
    for(size_t i=0;i<OT_PARTICLES_1D_MAX;i++)if(!sys->particles[i].alive){
        sys->particles[i]=(ot_particle_1d_t){x_q8,v_q8,0,life_ms,color,true};
        return true;
    }
    return false;
}
void ot_particles_1d_step(ot_particles_1d_t *sys,uint16_t dt_ms,
                          uint16_t led_count,int32_t acceleration_q8,
                          ot_particle_edge_t edge){
    if(!sys)return;
    if(!led_count){for(size_t i=0;i<OT_PARTICLES_1D_MAX;i++)sys->particles[i].alive=false;return;}
    const int64_t limit=(int64_t)led_count*256;
    for(size_t i=0;i<OT_PARTICLES_1D_MAX;i++){
        ot_particle_1d_t *p=&sys->particles[i];
        if(!p->alive)continue;
        if(dt_ms>=(uint16_t)(p->life_ms-p->age_ms)){p->alive=false;continue;}
        p->age_ms=(uint16_t)(p->age_ms+dt_ms);
        p->v_q8=saturate_i32((int64_t)p->v_q8+(int64_t)acceleration_q8*dt_ms/1000);
        int64_t x=(int64_t)p->x_q8+(int64_t)p->v_q8*dt_ms/1000;
        if(edge==OT_PARTICLE_WRAP){
            x=((x%limit)+limit)%limit;
        }else if(edge==OT_PARTICLE_BOUNCE){
            const int64_t max=limit-1;
            if(max<=0){x=0;p->v_q8=0;}
            else{
                const int64_t period=max*2;
                int64_t folded=((x%period)+period)%period;
                if(folded>max){folded=period-folded;p->v_q8=saturate_i32(-(int64_t)p->v_q8);}
                x=folded;
            }
        }else if(x<0 || x>=limit){
            p->alive=false;continue;
        }
        p->x_q8=saturate_i32(x);
    }
}
static uint8_t add_scaled(uint8_t existing,uint8_t component,uint32_t scale){
    uint32_t value=existing+((uint32_t)component*scale)/255u;
    return (uint8_t)(value>255?255:value);
}
void ot_particles_1d_render(const ot_particles_1d_t *sys,
                            ot_wled_rgb_t *pixels,size_t led_count,
                            uint8_t brightness,bool reverse){
    if(!sys||!pixels||led_count==0)return;
    for(size_t i=0;i<OT_PARTICLES_1D_MAX;i++){
        const ot_particle_1d_t *p=&sys->particles[i];
        if(!p->alive || p->x_q8<0 || !p->life_ms)continue;
        size_t index=(size_t)p->x_q8/256u;
        if(index>=led_count)continue;
        if(reverse)index=led_count-1u-index;
        const uint32_t scale=(uint32_t)((uint64_t)brightness*(p->life_ms-p->age_ms)/p->life_ms);
        pixels[index].r=add_scaled(pixels[index].r,p->color.r,scale);
        pixels[index].g=add_scaled(pixels[index].g,p->color.g,scale);
        pixels[index].b=add_scaled(pixels[index].b,p->color.b,scale);
    }
}
