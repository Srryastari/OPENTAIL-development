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
static bool outputs_valid(const ot_om_output_t a[OT_OM_OUTPUTS]){
    if(!a)return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)if(!ot_om_valid(&a[i]))return false;
    return true;
}
bool ot_tm_journal_build_revisions(ot_tm_journal_t *out,
    const ot_tm_profile_t *old,const ot_tm_profile_t *next,
    uint16_t prev_rev,uint16_t next_rev){
    if(!out||!ot_tm_valid(old)||!ot_tm_valid(next)||!prev_rev||
       next_rev<prev_rev || (memcmp(old,next,sizeof(*old))!=0 &&
                             next_rev<=prev_rev))return false;
    ot_tm_journal_t r={0};
    r.magic=OT_CFG_JOURNAL_MAGIC;r.version=OT_CFG_JOURNAL_VERSION;
    r.previous=*old;r.candidate=*next;
    r.previous_revision=prev_rev;r.candidate_revision=next_rev;
    r.crc=tm_crc(&r);*out=r;return true;
}
bool ot_om_journal_build_revisions(ot_om_journal_t *out,
    const ot_om_output_t old[OT_OM_OUTPUTS],
    const ot_om_output_t next[OT_OM_OUTPUTS],
    const uint16_t prev_rev[OT_OM_OUTPUTS],
    const uint16_t next_rev[OT_OM_OUTPUTS]){
    if(!out||!outputs_valid(old)||!outputs_valid(next)||
       !prev_rev||!next_rev)return false;
    ot_om_journal_t r={0};
    r.magic=OT_CFG_JOURNAL_MAGIC;r.version=OT_CFG_JOURNAL_VERSION;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        if(!prev_rev[i]||next_rev[i]<prev_rev[i]||
           (memcmp(&old[i],&next[i],sizeof(old[i]))!=0 &&
            next_rev[i]<=prev_rev[i]))return false;
        r.previous_revision[i]=prev_rev[i];r.candidate_revision[i]=next_rev[i];
    }
    memcpy(r.previous,old,sizeof(r.previous));
    memcpy(r.candidate,next,sizeof(r.candidate));
    r.crc=om_crc(&r);*out=r;return true;
}
bool ot_tm_journal_build(ot_tm_journal_t *out,const ot_tm_profile_t *old,
                         const ot_tm_profile_t *next){
    return ot_tm_journal_build_revisions(out,old,next,1u,2u);
}
bool ot_om_journal_build(ot_om_journal_t *out,
                         const ot_om_output_t old[OT_OM_OUTPUTS],
                         const ot_om_output_t next[OT_OM_OUTPUTS]){
    uint16_t a[OT_OM_OUTPUTS],b[OT_OM_OUTPUTS];
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){a[i]=1u;b[i]=2u;}
    return ot_om_journal_build_revisions(out,old,next,a,b);
}
bool ot_tm_journal_valid(const ot_tm_journal_t *r){
    return r&&r->magic==OT_CFG_JOURNAL_MAGIC&&
           r->version==OT_CFG_JOURNAL_VERSION&&tm_crc(r)==r->crc&&
           r->previous_revision>=1u&&r->candidate_revision>=r->previous_revision&&
           ot_tm_valid(&r->previous)&&ot_tm_valid(&r->candidate)&&
           (memcmp(&r->previous,&r->candidate,sizeof(r->previous))==0||
            r->candidate_revision>r->previous_revision);
}
bool ot_om_journal_valid(const ot_om_journal_t *r){
    if(!r||r->magic!=OT_CFG_JOURNAL_MAGIC||
       r->version!=OT_CFG_JOURNAL_VERSION||om_crc(r)!=r->crc||
       !outputs_valid(r->previous)||!outputs_valid(r->candidate))return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        if(!r->previous_revision[i]||r->candidate_revision[i]<r->previous_revision[i]||
           (memcmp(&r->previous[i],&r->candidate[i],sizeof(r->previous[i]))!=0&&
             r->candidate_revision[i]<=r->previous_revision[i]))return false;
    }
    return true;
}
/* Read-only upgrade of v1 records. Their old wire format had NO durable
 * revision. It is impossible to reconstruct edits across prior v1 reboots;
 * start at 1/2 and prohibit future wrap. */
bool ot_tm_journal_upgrade_v1(const ot_tm_journal_v1_t *v1,ot_tm_journal_t *v2){
    if(!v1||!v2||v1->magic!=OT_CFG_JOURNAL_MAGIC||v1->version!=1u)return false;
    ot_tm_journal_v1_t tmp=*v1;tmp.crc=0;
    if(ot_cfg_crc32(&tmp,sizeof(tmp))!=v1->crc)return false;
    return ot_tm_journal_build_revisions(v2,&v1->previous,&v1->candidate,1u,2u);
}
bool ot_om_journal_upgrade_v1(const ot_om_journal_v1_t *v1,ot_om_journal_t *v2){
    if(!v1||!v2||v1->magic!=OT_CFG_JOURNAL_MAGIC||v1->version!=1u)return false;
    ot_om_journal_v1_t tmp=*v1;tmp.crc=0;
    if(ot_cfg_crc32(&tmp,sizeof(tmp))!=v1->crc)return false;
    return ot_om_journal_build(v2,v1->previous,v1->candidate);
}
bool ot_tm_journal_recover(const ot_tm_journal_t *active,
                           const ot_tm_journal_t *staged,
                           ot_tm_profile_t *live,ot_tm_pending_t *pending){
    if(!live||!pending||(active&&!ot_tm_journal_valid(active))||
       (staged&&!ot_tm_journal_valid(staged)))return false;
    const ot_tm_journal_t *r=staged?staged:active;
    if(!r){pending->pending=false;return true;}
    /* Even without stage_v3, a crash during its erase must not cause an
     * asserted-input boot to load candidate as live: recover previous. */
    *live=r->previous;
    pending->profile=r->candidate;
    pending->pending=staged!=NULL ||
        memcmp(&r->previous,&r->candidate,sizeof(r->previous))!=0;
    return true;
}
bool ot_om_journal_recover(const ot_om_journal_t *active,
                           const ot_om_journal_t *staged,
                           ot_om_output_t live[OT_OM_OUTPUTS],
                           ot_om_pending_t *pending){
    if(!live||!pending||(active&&!ot_om_journal_valid(active))||
       (staged&&!ot_om_journal_valid(staged)))return false;
    const ot_om_journal_t *r=staged?staged:active;
    if(!r){pending->pending=false;return true;}
    memcpy(live,r->previous,sizeof(r->previous));
    memcpy(pending->outputs,r->candidate,sizeof(r->candidate));
    pending->pending=staged!=NULL ||
        memcmp(r->previous,r->candidate,sizeof(r->previous))!=0;
    return true;
}
