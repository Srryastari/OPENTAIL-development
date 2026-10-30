#include "config_journal.h"
#include <string.h>
uint32_t ot_cfg_crc32(const void *data,size_t bytes){
    if(!data)return 0u;
    const uint8_t *p=(const uint8_t*)data;uint32_t crc=0xffffffffu;
    for(size_t i=0;i<bytes;i++){
        crc^=p[i];
        for(unsigned j=0;j<8u;j++)
            crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
static uint32_t tm_crc(const ot_tm_journal_t *p){
    ot_tm_journal_t copy=*p;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
static uint32_t om_crc(const ot_om_journal_t *p){
    ot_om_journal_t copy=*p;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
bool ot_tm_journal_build(ot_tm_journal_t *out,const ot_tm_profile_t *old,
                         const ot_tm_profile_t *next){
    if(!out||!ot_tm_valid(old)||!ot_tm_valid(next))return false;
    ot_tm_journal_t r={0};
    r.magic=OT_CFG_JOURNAL_MAGIC;r.version=OT_CFG_JOURNAL_VERSION;
    r.previous=*old;r.candidate=*next;r.crc=tm_crc(&r);
    *out=r;return true;
}
bool ot_tm_journal_valid(const ot_tm_journal_t *r){
    return r&&r->magic==OT_CFG_JOURNAL_MAGIC&&
           r->version==OT_CFG_JOURNAL_VERSION&&tm_crc(r)==r->crc&&
           ot_tm_valid(&r->previous)&&ot_tm_valid(&r->candidate);
}
bool ot_om_journal_build(ot_om_journal_t *out,
                         const ot_om_output_t old[OT_OM_OUTPUTS],
                         const ot_om_output_t next[OT_OM_OUTPUTS]){
    if(!out||!old||!next)return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)
        if(!ot_om_valid(&old[i])||!ot_om_valid(&next[i]))return false;
    ot_om_journal_t r={0};
    r.magic=OT_CFG_JOURNAL_MAGIC;r.version=OT_CFG_JOURNAL_VERSION;
    memcpy(r.previous,old,sizeof(r.previous));
    memcpy(r.candidate,next,sizeof(r.candidate));r.crc=om_crc(&r);
    *out=r;return true;
}
bool ot_om_journal_valid(const ot_om_journal_t *r){
    if(!r||r->magic!=OT_CFG_JOURNAL_MAGIC||
       r->version!=OT_CFG_JOURNAL_VERSION||om_crc(r)!=r->crc)return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)
        if(!ot_om_valid(&r->previous[i])||!ot_om_valid(&r->candidate[i]))return false;
    return true;
}
bool ot_tm_journal_recover(const ot_tm_journal_t *active,
                           const ot_tm_journal_t *staged,
                           ot_tm_profile_t *live,ot_tm_pending_t *pending){
    if(!live||!pending||(active&&!ot_tm_journal_valid(active))||
       (staged&&!ot_tm_journal_valid(staged)))return false;
    pending->pending=staged!=NULL;
    if(staged){*live=staged->previous;pending->profile=staged->candidate;}
    else if(active)*live=active->candidate;
    return true;
}
bool ot_om_journal_recover(const ot_om_journal_t *active,
                           const ot_om_journal_t *staged,
                           ot_om_output_t live[OT_OM_OUTPUTS],
                           ot_om_pending_t *pending){
    if(!live||!pending||(active&&!ot_om_journal_valid(active))||
       (staged&&!ot_om_journal_valid(staged)))return false;
    pending->pending=staged!=NULL;
    if(staged){
        memcpy(live,staged->previous,sizeof(staged->previous));
        memcpy(pending->outputs,staged->candidate,sizeof(staged->candidate));
    }else if(active)memcpy(live,active->candidate,sizeof(active->candidate));
    return true;
}
