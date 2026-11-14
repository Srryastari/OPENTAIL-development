#include "panel_zone_diag.h"
#include <stddef.h>
#include <string.h>

void ot_pz_diag_clear(ot_pz_diag_state_t *state){
    if(state)memset(state,0,sizeof(*state));
}
static bool diag_color(uint8_t color,ot_rgb_t *out){
    if(!out)return false;
    switch(color){
        case 0u:*out=(ot_rgb_t){0,0,0};return true;
        case 1u:*out=(ot_rgb_t){255,255,255};return true;
        case 2u:*out=(ot_rgb_t){0,255,255};return true;
        default:return false;
    }
}
bool ot_pz_diag_probe(ot_pz_diag_state_t *state,const uint8_t *p,size_t n,
                      bool authenticated,bool capability_seen,
                      const uint16_t lengths[2],uint32_t now_ms){
    if(!state||!p||!lengths||!authenticated||!capability_seen||
       n!=OT_PZ_DIAG_PACKET_BYTES||p[0]!=OT_PZ_DIAG_PROBE_OPCODE||
       (p[1]!=1u&&p[1]!=2u)||p[4]>2u)return false;
    const unsigned lane=(unsigned)p[1]-1u;
    const uint16_t index=(uint16_t)(((uint16_t)p[2]<<8)|(uint16_t)p[3]);
    if(lengths[lane]==0u||index>=lengths[lane])return false;
    ot_rgb_t ignored;
    if(!diag_color(p[4],&ignored))return false;
    *state=(ot_pz_diag_state_t){
        .mode=OT_PZ_DIAG_PROBE,.lane=(uint8_t)lane,.color=p[4],
        .endpoint=index,.expires_ms=now_ms+OT_PZ_DIAG_LEASE_MS
    };
    return true;
}
bool ot_pz_diag_fill(ot_pz_diag_state_t *state,unsigned lane,uint16_t endpoint,
                     uint8_t color,uint16_t length,uint32_t now_ms){
    if(!state||lane>=2u||length==0u||endpoint>=length||
       (color!=1u&&color!=2u))return false;
    *state=(ot_pz_diag_state_t){
        .mode=OT_PZ_DIAG_FILL,.lane=(uint8_t)lane,.color=color,
        .endpoint=endpoint,.expires_ms=now_ms+OT_PZ_DIAG_LEASE_MS
    };
    return true;
}
uint16_t ot_pz_diag_fill_count(const ot_pz_diag_state_t *state){
    return state&&state->mode==OT_PZ_DIAG_FILL?
           (uint16_t)(state->endpoint+1u):0u;
}
bool ot_pz_diag_expire(ot_pz_diag_state_t *state,uint32_t now_ms){
    if(!state||state->mode==OT_PZ_DIAG_NONE)return false;
    if((int32_t)(now_ms-state->expires_ms)<0)return false;
    ot_pz_diag_clear(state);return true;
}
bool ot_pz_diag_render(const ot_pz_diag_state_t *state,unsigned lane,
                       uint16_t pixel,uint32_t now_ms,bool *owned,ot_rgb_t *color){
    if(!owned||!color)return false;
    *owned=false;*color=(ot_rgb_t){0,0,0};
    if(!state||state->mode==OT_PZ_DIAG_NONE||
       (int32_t)(now_ms-state->expires_ms)>=0||lane!=state->lane)return false;
    if(state->mode==OT_PZ_DIAG_PROBE){
        if(pixel!=state->endpoint)return true;
        *owned=true;
        return diag_color(state->color,color);
    }
    if(state->mode==OT_PZ_DIAG_FILL){
        *owned=true;
        if(pixel<=state->endpoint)return diag_color(state->color,color);
        *color=(ot_rgb_t){0,0,0};
        return true;
    }
    return false;
}
