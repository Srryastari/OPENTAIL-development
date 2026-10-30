#include "palette_profile.h"
#include <string.h>
uint16_t ot_palette_crc16(const uint8_t *p,size_t n){
    uint16_t crc=0xffffu;
    if(!p&&n)return 0u;
    for(size_t i=0;i<n;i++){
        crc^=(uint16_t)p[i]<<8;
        for(unsigned b=0;b<8;b++)
            crc=(uint16_t)((crc<<1)^((crc&0x8000u)?0x1021u:0u));
    }
    return crc;
}
bool ot_palette_id_to_index(uint8_t id,uint8_t *out){
    if(!out||id<73u||id>200u)return false;
    *out=(uint8_t)(200u-id);
    return true;
}
/* All fields are checksummed in explicit byte order, never struct padding. */
static uint16_t record_crc(const ot_palette_record_t *r){
    uint8_t h[14u+OT_PALETTE_BYTES_MAX]={0};
    h[0]=(uint8_t)r->magic;h[1]=(uint8_t)(r->magic>>8);
    h[2]=(uint8_t)(r->magic>>16);h[3]=(uint8_t)(r->magic>>24);
    h[4]=(uint8_t)r->version;h[5]=(uint8_t)(r->version>>8);
    h[6]=r->index;h[7]=r->count;
    h[8]=(uint8_t)r->byte_length;h[9]=(uint8_t)(r->byte_length>>8);
    for(unsigned i=0;i<4;i++)h[10+i]=(uint8_t)(r->revision>>(i*8u));
    memcpy(h+14,r->stops,OT_PALETTE_BYTES_MAX);
    return ot_palette_crc16(h,sizeof(h));
}
bool ot_palette_record_build(ot_palette_record_t *r,uint8_t index,uint8_t count,
                              uint32_t revision,const uint8_t *stops){
    if(!r||!stops||index>=OT_PALETTE_SLOTS||count<2u||
       count>OT_PALETTE_STOPS_MAX||revision==0u)return false;
    ot_wled_palette16_t palette;
    if(!ot_wled_palette_from_gradient_stops(stops,count,&palette))return false;
    ot_palette_record_t candidate={0};
    candidate.magic=OT_PALETTE_RECORD_MAGIC;
    candidate.version=OT_PALETTE_RECORD_VERSION;
    candidate.index=index;candidate.count=count;
    candidate.byte_length=(uint16_t)count*4u;
    candidate.revision=revision;
    memcpy(candidate.stops,stops,candidate.byte_length);
    candidate.crc=record_crc(&candidate);
    *r=candidate;
    return true;
}
bool ot_palette_record_valid(const ot_palette_record_t *r,
                              ot_wled_palette16_t *palette){
    if(!r||!palette||r->magic!=OT_PALETTE_RECORD_MAGIC||
       r->version!=OT_PALETTE_RECORD_VERSION||r->index>=OT_PALETTE_SLOTS||
       r->count<2u||r->count>OT_PALETTE_STOPS_MAX||
       r->byte_length!=(uint16_t)r->count*4u||r->revision==0u||
       r->crc!=record_crc(r))return false;
    for(size_t i=r->byte_length;i<OT_PALETTE_BYTES_MAX;i++)
        if(r->stops[i])return false;
    return ot_wled_palette_from_gradient_stops(r->stops,r->count,palette);
}
bool ot_palette_entry_from_record(ot_palette_entry_t *entry,
                                  const ot_palette_record_t *record){
    if(!entry)return false;
    ot_wled_palette16_t decoded;
    if(!ot_palette_record_valid(record,&decoded))return false;
    *entry=(ot_palette_entry_t){.palette=decoded,.revision=record->revision,
           .stops_crc=ot_palette_crc16(record->stops,record->byte_length),
           .count=record->count,.present=true};
    return true;
}
