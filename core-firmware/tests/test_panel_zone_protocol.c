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
static void role_sentinel_semantics(void){
    ot_pz_controller_t committed;
    ot_pz_defaults(&committed);
    committed.outputs[0].active_pixels=100u;
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,&committed,0u,1u,1u,false,100u));
    const ot_pz_range_update_t unassigned={
        .panel_id=1u,.zone_id=2u,.range_id=0u,.start=0u,.count=10u,
        .role_flags=0x0fu
    };
    assert(ot_pz_preview_apply_updates(&preview,0u,&unassigned,1u,
                                       1u,1u,false,101u));
    assert(preview.candidate.outputs[0].zone_count==1u);
    assert(preview.candidate.outputs[0].zones[0].role==OT_PZ_ROLE_UNASSIGNED);
    ot_pz_preview_cancel(&preview);

    memset(&preview,0,sizeof(preview));
    assert(ot_pz_preview_begin(&preview,&committed,0u,1u,1u,false,100u));
    ot_pz_range_update_t off=unassigned;
    off.panel_id=3u;off.zone_id=4u;off.role_flags=0x00u;
    assert(ot_pz_preview_apply_updates(&preview,0u,&off,1u,
                                       1u,1u,false,101u));
    assert(preview.candidate.outputs[0].zone_count==1u);
    assert(preview.candidate.outputs[0].zones[0].role==OT_OM_OFF);
    ot_pz_preview_cancel(&preview);
}
static void control_packets(void){
    ot_pz_tx_control_packet_t parsed={0};
    const uint8_t begin[8]={0xfe,0x01,1u,0x34,0x12,0x80,0x00,0x03};
    assert(ot_pz_tx_parse_control(begin,sizeof(begin),&parsed));
    assert(parsed.opcode==OT_PZ_TX_OP_BEGIN&&parsed.lane==1u);
    assert(parsed.base_revision==0x1234u&&parsed.pixel_capacity==128u);
    assert(parsed.schema_version==3u);

    const uint8_t commit[11]={0xfe,0x03,0u,0x78,0x56,0x34,0x12,
                              0xef,0xcd,0xab,0x90};
    assert(ot_pz_tx_parse_control(commit,sizeof(commit),&parsed));
    assert(parsed.opcode==OT_PZ_TX_OP_COMMIT&&parsed.lane==0u);
    assert(parsed.session_id==0x12345678u&&parsed.candidate_crc32==0x90abcdefu);

    const uint8_t cancel[7]={0xfe,0x04,1u,0x04,0x03,0x02,0x01};
    assert(ot_pz_tx_parse_control(cancel,sizeof(cancel),&parsed));
    assert(parsed.opcode==OT_PZ_TX_OP_CANCEL&&parsed.session_id==0x01020304u);
    const uint8_t heartbeat[7]={0xfe,0x05,0u,0x04,0x03,0x02,0x01};
    assert(ot_pz_tx_parse_control(heartbeat,sizeof(heartbeat),&parsed));
    assert(parsed.opcode==OT_PZ_TX_OP_HEARTBEAT&&parsed.lane==0u);

    uint8_t bad[12]={0};
    memcpy(bad,begin,sizeof(begin));bad[0]=0u;
    assert(!ot_pz_tx_parse_control(bad,sizeof(begin),&parsed));
    assert(!ot_pz_tx_parse_control(begin,7u,&parsed));
    memcpy(bad,commit,sizeof(commit));
    assert(!ot_pz_tx_parse_control(bad,10u,&parsed));
    memcpy(bad,cancel,sizeof(cancel));
    assert(!ot_pz_tx_parse_control(bad,8u,&parsed));
    memcpy(bad,begin,sizeof(begin));bad[2]=OT_PZ_OUTPUTS;
    assert(!ot_pz_tx_parse_control(bad,sizeof(begin),&parsed));
    memcpy(bad,begin,sizeof(begin));bad[3]=0u;bad[4]=0u;
    assert(!ot_pz_tx_parse_control(bad,sizeof(begin),&parsed));
    memcpy(bad,begin,sizeof(begin));bad[5]=0u;bad[6]=0u;
    assert(!ot_pz_tx_parse_control(bad,sizeof(begin),&parsed));
    memcpy(bad,begin,sizeof(begin));bad[7]=0u;
    assert(!ot_pz_tx_parse_control(bad,sizeof(begin),&parsed));
    memcpy(bad,cancel,sizeof(cancel));memset(bad+3,0,4u);
    assert(!ot_pz_tx_parse_control(bad,sizeof(cancel),&parsed));
    memcpy(bad,heartbeat,sizeof(heartbeat));bad[1]=OT_PZ_TX_OP_UPDATE;
    assert(!ot_pz_tx_parse_control(bad,sizeof(heartbeat),&parsed));
    memcpy(bad,heartbeat,sizeof(heartbeat));bad[1]=0x06u;
    assert(!ot_pz_tx_parse_control(bad,sizeof(heartbeat),&parsed));
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
    assert(ot_pz_tx_sequence_check(false,0u,0u,101u,11u)==OT_PZ_TX_SEQUENCE_GAP);
    assert(ot_pz_tx_sequence_check(true,101u,11u,101u,11u)==OT_PZ_TX_SEQUENCE_DUPLICATE);
    assert(ot_pz_tx_sequence_check(true,101u,11u,101u,12u)==OT_PZ_TX_SEQUENCE_CONFLICT);
    assert(ot_pz_tx_sequence_check(true,101u,11u,102u,13u)==OT_PZ_TX_SEQUENCE_NEW);
    assert(ot_pz_tx_sequence_check(true,101u,11u,103u,13u)==OT_PZ_TX_SEQUENCE_GAP);
    assert(ot_pz_tx_sequence_check(true,103u,13u,102u,12u)==OT_PZ_TX_SEQUENCE_OUT_OF_ORDER);
    assert(ot_pz_tx_sequence_check(true,UINT32_MAX-1u,1u,1u,2u)==OT_PZ_TX_SEQUENCE_GAP);
    assert(ot_pz_tx_sequence_check(true,UINT32_MAX,1u,1u,2u)==OT_PZ_TX_SEQUENCE_NEW);
    assert(ot_pz_tx_sequence_check(true,10u,1u,0u,2u)==OT_PZ_TX_SEQUENCE_STALE);

    /* A delayed packet, duplicate, gap or reorder arriving after COMMIT is
     * accepted must be rejected before replay classification or mutation. */
    assert(ot_pz_tx_update_admit(true,false,true,101u,11u,102u,12u)==
           OT_PZ_TX_UPDATE_REJECT_COMMITTING); /* delayed next packet */
    assert(ot_pz_tx_update_admit(true,false,true,101u,11u,101u,11u)==
           OT_PZ_TX_UPDATE_REJECT_COMMITTING); /* duplicate */
    assert(ot_pz_tx_update_admit(true,false,true,101u,11u,103u,13u)==
           OT_PZ_TX_UPDATE_REJECT_COMMITTING); /* missing sequence */
    assert(ot_pz_tx_update_admit(true,false,true,103u,13u,102u,12u)==
           OT_PZ_TX_UPDATE_REJECT_COMMITTING); /* delayed/out of order */
    assert(ot_pz_tx_update_admit(false,true,true,101u,11u,102u,12u)==
           OT_PZ_TX_UPDATE_REJECT_SYNC_REQUIRED);
    assert(ot_pz_tx_update_admit(false,false,true,101u,11u,102u,12u)==
           OT_PZ_TX_UPDATE_ACCEPT);
    assert(ot_pz_tx_update_admit(false,false,true,101u,11u,101u,11u)==
           OT_PZ_TX_UPDATE_REJECT_DUPLICATE);
    assert(ot_pz_tx_update_admit(false,false,true,101u,11u,103u,13u)==
           OT_PZ_TX_UPDATE_REJECT_GAP);
    assert(ot_pz_tx_update_admit(false,false,true,103u,13u,102u,12u)==
           OT_PZ_TX_UPDATE_REJECT_OUT_OF_ORDER);
    assert(ot_pz_tx_update_admit(false,false,true,101u,11u,101u,12u)==
           OT_PZ_TX_UPDATE_REJECT_CONFLICT);
    control_packets();
    role_sentinel_semantics();
    return 0;
}
