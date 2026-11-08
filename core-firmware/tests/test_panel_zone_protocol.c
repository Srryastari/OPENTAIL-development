#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "panel_zone_protocol.h"

static void make_packet(uint8_t *p,unsigned count,uint32_t seq){
    memset(p,0,43u);
    p[0]=OT_PZ_TX_MAGIC;p[1]=OT_PZ_TX_OP_UPDATE;p[2]=0u;
    p[3]=0x78u;p[4]=0x56u;p[5]=0x34u;p[6]=0x12u;
    p[7]=(uint8_t)seq;p[8]=(uint8_t)(seq>>8);
    p[9]=(uint8_t)(seq>>16);p[10]=(uint8_t)(seq>>24);
    for(unsigned i=0;i<count;i++){
        uint8_t *d=p+11u+i*8u;
        d[0]=1u;d[1]=(uint8_t)(2u+i);d[2]=0u;
        d[3]=(uint8_t)(i*10u);d[4]=0u;
        d[5]=10u;d[6]=0u;d[7]=(uint8_t)OT_OM_DECORATIVE;
    }
}
int main(void){
    uint8_t packet[43];ot_pz_tx_update_packet_t parsed={0};
    make_packet(packet,1u,1u);
    assert(ot_pz_tx_parse_update(packet,19u,23u,&parsed));
    assert(parsed.lane==0u&&parsed.session_id==0x12345678u&&parsed.sequence==1u);
    assert(parsed.descriptor_count==1u&&parsed.descriptors[0].start==0u);
    assert(parsed.descriptors[0].count==10u);
    make_packet(packet,2u,2u);
    assert(ot_pz_tx_parse_update(packet,27u,31u,&parsed));
    assert(!ot_pz_tx_parse_update(packet,27u,23u,&parsed));
    make_packet(packet,4u,3u);
    assert(ot_pz_tx_parse_update(packet,43u,46u,&parsed));
    assert(!ot_pz_tx_parse_update(packet,43u,45u,&parsed));
    assert(!ot_pz_tx_parse_update(packet,43u,23u,&parsed));
    assert(!ot_pz_tx_parse_update(packet,42u,46u,&parsed));
    assert(!ot_pz_tx_parse_update(packet,51u,247u,&parsed));
    packet[0]=0u;assert(!ot_pz_tx_parse_update(packet,43u,46u,&parsed));
    make_packet(packet,1u,0u);assert(!ot_pz_tx_parse_update(packet,19u,23u,&parsed));
    make_packet(packet,1u,1u);packet[2]=2u;
    assert(!ot_pz_tx_parse_update(packet,19u,23u,&parsed));

    assert(ot_pz_tx_sequence_check(false,0u,0u,1u,11u)==OT_PZ_TX_SEQUENCE_NEW);
    assert(ot_pz_tx_sequence_check(false,0u,0u,101u,11u)==OT_PZ_TX_SEQUENCE_STALE);
    assert(ot_pz_tx_sequence_check(true,101u,11u,101u,11u)==OT_PZ_TX_SEQUENCE_DUPLICATE);
    assert(ot_pz_tx_sequence_check(true,101u,11u,101u,12u)==OT_PZ_TX_SEQUENCE_CONFLICT);
    assert(ot_pz_tx_sequence_check(true,101u,11u,103u,13u)==OT_PZ_TX_SEQUENCE_NEW);
    assert(ot_pz_tx_sequence_check(true,103u,13u,102u,12u)==OT_PZ_TX_SEQUENCE_STALE);
    assert(ot_pz_tx_sequence_check(true,UINT32_MAX-1u,1u,1u,2u)==OT_PZ_TX_SEQUENCE_NEW);
    assert(ot_pz_tx_sequence_check(true,10u,1u,0u,2u)==OT_PZ_TX_SEQUENCE_STALE);
    return 0;
}
