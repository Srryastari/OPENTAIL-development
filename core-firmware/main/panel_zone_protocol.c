#include "panel_zone_protocol.h"
#include <string.h>

static uint32_t tx_crc32(const uint8_t *data,size_t length){
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<length;i++){
        crc^=data[i];
        for(unsigned bit=0;bit<8u;bit++)
            crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
bool ot_pz_tx_parse_update(const uint8_t *p,size_t n,uint16_t mtu,
                           ot_pz_tx_update_packet_t *out){
    if(!p||!out||mtu<OT_PZ_TX_MIN_ATT_MTU||
       n<OT_PZ_TX_UPDATE_HEADER_BYTES+OT_PZ_TX_DESCRIPTOR_BYTES||
       n>(size_t)mtu-3u||p[0]!=OT_PZ_TX_MAGIC||p[1]!=OT_PZ_TX_OP_UPDATE||
       p[2]>=OT_PZ_OUTPUTS||
       (p[3]==0u&&p[4]==0u&&p[5]==0u&&p[6]==0u))
        return false;
    const size_t descriptor_bytes=n-OT_PZ_TX_UPDATE_HEADER_BYTES;
    if(descriptor_bytes%OT_PZ_TX_DESCRIPTOR_BYTES!=0u)return false;
    const size_t count=descriptor_bytes/OT_PZ_TX_DESCRIPTOR_BYTES;
    const size_t mtu_count=((size_t)mtu-3u-OT_PZ_TX_UPDATE_HEADER_BYTES)/
                           OT_PZ_TX_DESCRIPTOR_BYTES;
    if(count==0u||count>OT_PZ_TX_MAX_DESCRIPTORS||count>mtu_count)return false;
    ot_pz_tx_update_packet_t parsed={0};
    parsed.lane=p[2];
    parsed.session_id=(uint32_t)p[3]|((uint32_t)p[4]<<8)|
        ((uint32_t)p[5]<<16)|((uint32_t)p[6]<<24);
    parsed.sequence=(uint32_t)p[7]|((uint32_t)p[8]<<8)|
        ((uint32_t)p[9]<<16)|((uint32_t)p[10]<<24);
    if(parsed.session_id==0u||parsed.sequence==0u)return false;
    parsed.packet_crc32=tx_crc32(p,n);
    parsed.descriptor_count=count;
    for(size_t i=0;i<count;i++){
        const uint8_t *d=p+OT_PZ_TX_UPDATE_HEADER_BYTES+
                         i*OT_PZ_TX_DESCRIPTOR_BYTES;
        ot_pz_range_update_t *u=&parsed.descriptors[i];
        u->panel_id=d[0];u->zone_id=d[1];u->range_id=d[2];
        u->start=(uint16_t)(d[3]|((uint16_t)d[4]<<8));
        u->count=(uint16_t)(d[5]|((uint16_t)d[6]<<8));
        u->role_flags=d[7];
    }
    *out=parsed;
    return true;
}
ot_pz_tx_sequence_result_t ot_pz_tx_sequence_check(bool has_last,
    uint32_t last_sequence,uint32_t last_crc,uint32_t sequence,uint32_t crc){
    if(sequence==0u)return OT_PZ_TX_SEQUENCE_STALE;
    if(!has_last)return sequence==1u?OT_PZ_TX_SEQUENCE_NEW:
                                      OT_PZ_TX_SEQUENCE_STALE;
    const uint32_t delta=sequence-last_sequence;
    if(delta==0u)return crc==last_crc?OT_PZ_TX_SEQUENCE_DUPLICATE:
                                      OT_PZ_TX_SEQUENCE_CONFLICT;
    if(delta>=0x80000000u)return OT_PZ_TX_SEQUENCE_STALE;
    return OT_PZ_TX_SEQUENCE_NEW;
}
