#include "ble_config.h"
#include <string.h>
uint16_t ot_cfg_crc16(const uint8_t *p,size_t n){
    uint16_t c=0xffff;
    for(size_t i=0;i<n;i++){
        c^=(uint16_t)p[i]<<8;
        for(int b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);
    }
    return c;
}
void ot_cfg_encode(const ot_om_output_t *s,uint8_t p[OT_CFG_WIRE_BYTES]){
    memset(p,0,OT_CFG_WIRE_BYTES);size_t k=0;
    p[k++]=OT_CFG_WIRE_VERSION;p[k++]=(uint8_t)s->active_pixels;p[k++]=(uint8_t)(s->active_pixels>>8);
    p[k++]=s->color_order;p[k++]=s->zone_count;
    memcpy(p+k,s->name,OT_OM_NAME_LEN);k+=OT_OM_NAME_LEN;
    memcpy(p+k,s->rgb_gain,3);k+=3;memcpy(p+k,s->white_gain,3);k+=3;
    p[k++]=s->gamma_enabled?1:0;
    for(unsigned z=0;z<OT_OM_ZONES;z++){
        const ot_om_zone_t *zone=&s->zones[z];
        memcpy(p+k,zone->name,OT_OM_NAME_LEN);k+=OT_OM_NAME_LEN;
        p[k++]=(uint8_t)zone->pixels;p[k++]=(uint8_t)(zone->pixels>>8);p[k++]=zone->role;
    }
}
bool ot_cfg_decode(const uint8_t p[OT_CFG_WIRE_BYTES],ot_om_output_t *dest){
    if(!p||!dest||p[0]!=OT_CFG_WIRE_VERSION)return false;
    ot_om_output_t s={0};size_t k=1;
    s.active_pixels=(uint16_t)(p[k]|((unsigned)p[k+1]<<8));k+=2;
    s.color_order=p[k++];s.zone_count=p[k++];
    memcpy(s.name,p+k,OT_OM_NAME_LEN);k+=OT_OM_NAME_LEN;
    if(!memchr(s.name,0,OT_OM_NAME_LEN))return false;
    memcpy(s.rgb_gain,p+k,3);k+=3;memcpy(s.white_gain,p+k,3);k+=3;
    if(p[k]>1)return false;
    s.gamma_enabled=p[k++]!=0;
    for(unsigned z=0;z<OT_OM_ZONES;z++){
        ot_om_zone_t *zone=&s.zones[z];
        memcpy(zone->name,p+k,OT_OM_NAME_LEN);k+=OT_OM_NAME_LEN;
        zone->pixels=(uint16_t)(p[k]|((unsigned)p[k+1]<<8));k+=2;zone->role=p[k++];
        if(z<s.zone_count&&!memchr(zone->name,0,OT_OM_NAME_LEN))return false;
    }
    if(k!=OT_CFG_WIRE_BYTES||!ot_om_valid(&s))return false;
    *dest=s;return true;
}
