#include "ot_config_journal.h"
#include <stddef.h>
#include <string.h>

/* IEEE CRC-32 over the fixed schema, including both recovery and requested
 * snapshots. Not a security primitive: BLE authorization remains separate. */
static uint32_t journal_crc(const void *buffer,size_t size){
    const uint8_t *p=(const uint8_t *)buffer;
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<size;i++){
        crc^=p[i];
        for(unsigned bit=0;bit<8u;bit++)
            crc=(crc>>1)^(0xedb88320u&((uint32_t)-(int32_t)(crc&1u)));
    }
    return ~crc;
}
static bool outputs_valid(const ot_om_output_t outputs[OT_OM_OUTPUTS]){
    if(!outputs)return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)
        if(!ot_om_valid(&outputs[i]))return false;
    return true;
}
bool ot_cfg_tm_pack(const ot_tm_profile_t *previous,
                    const ot_tm_profile_t *requested,ot_cfg_tm_journal_t *out){
    if(!out||!ot_tm_valid(previous)||!ot_tm_valid(requested))return false;
    memset(out,0,sizeof(*out));
    out->magic=OT_CFG_TM_MAGIC;
    out->version=OT_CFG_JOURNAL_VERSION;
    out->previous=*previous;
    out->requested=*requested;
    out->crc=journal_crc(out,offsetof(ot_cfg_tm_journal_t,crc));
    return true;
}
bool ot_cfg_tm_unpack(const ot_cfg_tm_journal_t *in,
                      ot_tm_profile_t *previous,ot_tm_profile_t *requested){
    if(!in||!previous||!requested||in->magic!=OT_CFG_TM_MAGIC||
       in->version!=OT_CFG_JOURNAL_VERSION||
       in->crc!=journal_crc(in,offsetof(ot_cfg_tm_journal_t,crc))||
       !ot_tm_valid(&in->previous)||!ot_tm_valid(&in->requested))return false;
    *previous=in->previous;
    *requested=in->requested;
    return true;
}
bool ot_cfg_om_pack(const ot_om_output_t previous[OT_OM_OUTPUTS],
                    const ot_om_output_t requested[OT_OM_OUTPUTS],
                    ot_cfg_om_journal_t *out){
    if(!out||!outputs_valid(previous)||!outputs_valid(requested))return false;
    memset(out,0,sizeof(*out));
    out->magic=OT_CFG_OM_MAGIC;
    out->version=OT_CFG_JOURNAL_VERSION;
    memcpy(out->previous,previous,sizeof(out->previous));
    memcpy(out->requested,requested,sizeof(out->requested));
    out->crc=journal_crc(out,offsetof(ot_cfg_om_journal_t,crc));
    return true;
}
bool ot_cfg_om_unpack(const ot_cfg_om_journal_t *in,
                      ot_om_output_t previous[OT_OM_OUTPUTS],
                      ot_om_output_t requested[OT_OM_OUTPUTS]){
    if(!in||!previous||!requested||in->magic!=OT_CFG_OM_MAGIC||
       in->version!=OT_CFG_JOURNAL_VERSION||
       in->crc!=journal_crc(in,offsetof(ot_cfg_om_journal_t,crc))||
       !outputs_valid(in->previous)||!outputs_valid(in->requested))return false;
    memcpy(previous,in->previous,sizeof(in->previous));
    memcpy(requested,in->requested,sizeof(in->requested));
    return true;
}
