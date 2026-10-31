#include "preview.h"
#include <stdlib.h>
#include "core/palette_1d.h"
struct ot_preview {
    ot_effects_1d_state_t state;
    ot_wled_palette_random_t palette;
    uint32_t palette_seed;
    uint16_t effect;
    size_t count;
    ot_wled_rgb_t pixels[];
};
static uint8_t random_byte(void *context) {
    uint32_t *seed=context;
    *seed^=*seed<<13;*seed^=*seed>>17;*seed^=*seed<<5;
    return (uint8_t)*seed;
}
ot_preview *ot_preview_create(uint16_t effect,size_t count) {
    ot_wled_fx_metadata_t meta;
    if(!count||count>65535u||!ot_effects_1d_metadata(effect,&meta))return NULL;
    ot_preview *p=calloc(1,sizeof(*p)+sizeof(p->pixels[0])*count);
    if(!p)return NULL;
    p->effect=effect;p->count=count;p->palette_seed=0x5eed1234u;
    ot_effects_1d_reset(&p->state,0x5eed1234u);
    if(!ot_wled_palette_global_init(&p->palette,random_byte,&p->palette_seed)){
        ot_preview_free(p);return NULL;
    }
    return p;
}
void ot_preview_free(ot_preview *p) {
    if(p){ot_wled_fx_reset(&p->state.wled);free(p);}
}
static ot_wled_rgb_t rgb(uint32_t argb) {
    return (ot_wled_rgb_t){(uint8_t)(argb>>16),(uint8_t)(argb>>8),(uint8_t)argb};
}
bool ot_preview_render(ot_preview *p,uint32_t ms,uint16_t frame_ms,
    uint8_t speed,uint8_t intensity,uint8_t brightness,uint8_t palette,
    uint32_t fg,uint32_t bg,const uint8_t *stops,size_t stop_count,
    const ot_wled_audio_peak_t *audio,uint32_t *argb) {
    if(!p||!argb||!frame_ms)return false;
    ot_wled_palette16_t resolved;
    const ot_wled_palette16_t *selected=NULL;
    // Same single global producer as CORE; this local preview is one segment.
    if(!ot_wled_palette_global_tick(&p->palette,ms,5u,
        frame_ms<8u?8u:frame_ms>1000u?1000u:frame_ms,
        750u,true,random_byte,&p->palette_seed))return false;
    if(palette==1u)selected=&p->palette.current;
    else if(palette>72u) {
        if(!stops||!ot_wled_palette_from_gradient_stops(stops,stop_count,&resolved))return false;
        selected=&resolved;
    }
    ot_wled_fx_config_t cfg={.effect_id=p->effect,.speed=speed,
        .intensity=intensity,.brightness=brightness,.palette_id=palette,
        .frame_time_ms=frame_ms,.foreground=rgb(fg),.background=rgb(bg),
        .audio_peak=audio,.resolved_palette=selected};
    if(!ot_effects_1d_render(&cfg,&p->state,ms,p->pixels,p->count))return false;
    for(size_t i=0;i<p->count;i++)
        argb[i]=0xff000000u|((uint32_t)p->pixels[i].r<<16)|
            ((uint32_t)p->pixels[i].g<<8)|p->pixels[i].b;
    return true;
}
