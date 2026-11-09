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

static uint32_t pz_crc(const ot_pz_journal_t *record){
    ot_pz_journal_t copy=*record;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
bool ot_pz_journal_build(ot_pz_journal_t *out,
                         const ot_pz_controller_t *previous,
                         const ot_pz_controller_t *candidate,
                         const uint16_t previous_revision[OT_PZ_OUTPUTS],
                         const uint16_t candidate_revision[OT_PZ_OUTPUTS]){
    if(!out||!previous||!candidate||!previous_revision||!candidate_revision||
       !ot_pz_valid(previous)||!ot_pz_valid(candidate))return false;
    ot_pz_journal_t next={0};
    next.magic=OT_PZ_JOURNAL_MAGIC;next.version=OT_PZ_JOURNAL_VERSION;
    next.previous=*previous;next.candidate=*candidate;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(previous_revision[lane]==0u||
           candidate_revision[lane]<previous_revision[lane]||
           (memcmp(&previous->outputs[lane],&candidate->outputs[lane],
                   sizeof(previous->outputs[lane]))!=0&&
            candidate_revision[lane]<=previous_revision[lane]))return false;
        next.previous_revision[lane]=previous_revision[lane];
        next.candidate_revision[lane]=candidate_revision[lane];
    }
    next.crc=pz_crc(&next);*out=next;return true;
}
bool ot_pz_journal_valid(const ot_pz_journal_t *record){
    if(!record||record->magic!=OT_PZ_JOURNAL_MAGIC||
       record->version!=OT_PZ_JOURNAL_VERSION||
       pz_crc(record)!=record->crc||
       !ot_pz_valid(&record->previous)||!ot_pz_valid(&record->candidate))return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(record->previous_revision[lane]==0u||
           record->candidate_revision[lane]<record->previous_revision[lane]||
           (memcmp(&record->previous.outputs[lane],
                   &record->candidate.outputs[lane],
                   sizeof(record->previous.outputs[lane]))!=0&&
            record->candidate_revision[lane]<=record->previous_revision[lane]))
            return false;
    }
    return true;
}
bool ot_pz_journal_recover(const ot_pz_journal_t *active,
                           const ot_pz_journal_t *staged,
                           ot_pz_controller_t *live,
                           ot_pz_pending_t *pending,
                           uint16_t active_revision[OT_PZ_OUTPUTS],
                           uint16_t staged_revision[OT_PZ_OUTPUTS]){
    if(!live||!pending||!active_revision||!staged_revision||
       (active&&!ot_pz_journal_valid(active))||
       (staged&&!ot_pz_journal_valid(staged)))return false;
    if(active&&staged){
        /* A newer staged transaction must be based on the last fully written
         * active candidate; reject cross-revision or torn journal pairs. */
        const bool promotion_copy=memcmp(active,staged,sizeof(*active))==0;
        const bool next_transaction=
            memcmp(&active->candidate,&staged->previous,sizeof(active->candidate))==0&&
            memcmp(active->candidate_revision,staged->previous_revision,
                   sizeof(active->candidate_revision))==0;
        if(!promotion_copy&&!next_transaction)return false;
    }
    const ot_pz_journal_t *record=staged?staged:active;
    if(!record){
        ot_pz_defaults(live);pending->candidate=*live;
        pending->pending=false;
        for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
            active_revision[lane]=staged_revision[lane]=1u;
        return true;
    }
    /* The previous image remains authoritative until the quiet-window
     * promoter completes. If stage was erased just before a reset, the active
     * record still carries both images and recovery safely retries promotion. */
    *live=record->previous;
    pending->candidate=record->candidate;
    pending->pending=staged!=NULL||
        memcmp(&record->previous,&record->candidate,sizeof(record->previous))!=0;
    memcpy(active_revision,record->previous_revision,sizeof(record->previous_revision));
    memcpy(staged_revision,record->candidate_revision,sizeof(record->candidate_revision));
    if(!pending->pending)
        memcpy(active_revision,staged_revision,sizeof(record->candidate_revision));
    return true;
}


static bool pz_pair_revisions_valid(
    const uint16_t previous[OT_PZ_OUTPUTS],
    const uint16_t candidate[OT_PZ_OUTPUTS]){
    if(!previous||!candidate)return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(previous[lane]==0u||candidate[lane]<previous[lane])return false;
    return true;
}
static bool pz_pair_has_revision_change(
    const uint16_t previous[OT_PZ_OUTPUTS],
    const uint16_t candidate[OT_PZ_OUTPUTS]){
    if(!previous||!candidate)return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(candidate[lane]>previous[lane])return true;
    return false;
}
static uint32_t pz_map_journal_crc(const ot_pz_map_journal_t *record){
    ot_pz_map_journal_t copy=*record;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
static uint32_t pz_lane_journal_crc(const ot_pz_lane_journal_t *record){
    ot_pz_lane_journal_t copy=*record;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
static uint32_t pz_pair_intent_crc(const ot_pz_pair_intent_t *record){
    ot_pz_pair_intent_t copy=*record;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
bool ot_pz_map_journal_build(ot_pz_map_journal_t *out,
                         const ot_pz_map_t *previous,const ot_pz_map_t *candidate,
                         const uint16_t previous_revision[OT_PZ_OUTPUTS],
                         const uint16_t candidate_revision[OT_PZ_OUTPUTS],
                         uint32_t transaction_id){
    if(!out||!previous||!candidate||!transaction_id||
       !previous_revision||!candidate_revision||
       !ot_pz_map_valid(previous)||!ot_pz_map_valid(candidate)||
       !pz_pair_revisions_valid(previous_revision,candidate_revision))return false;
    ot_pz_map_journal_t next={0};
    next.magic=OT_PZ_MAP_JOURNAL_MAGIC;
    next.version=OT_PZ_PAIR_SCHEMA_VERSION;
    next.transaction_id=transaction_id;
    next.previous=*previous;next.candidate=*candidate;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(memcmp(&previous->outputs[lane],&candidate->outputs[lane],
                  sizeof(previous->outputs[lane]))!=0&&
           candidate_revision[lane]<=previous_revision[lane])return false;
        next.previous_revision[lane]=previous_revision[lane];
        next.candidate_revision[lane]=candidate_revision[lane];
    }
    next.crc=pz_map_journal_crc(&next);*out=next;return true;
}
bool ot_pz_map_journal_valid(const ot_pz_map_journal_t *record){
    if(!record||record->magic!=OT_PZ_MAP_JOURNAL_MAGIC||
       record->version!=OT_PZ_PAIR_SCHEMA_VERSION||!record->transaction_id||
       pz_map_journal_crc(record)!=record->crc||
       !ot_pz_map_valid(&record->previous)||!ot_pz_map_valid(&record->candidate)||
       !pz_pair_revisions_valid(record->previous_revision,
                                record->candidate_revision))return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(memcmp(&record->previous.outputs[lane],&record->candidate.outputs[lane],
                  sizeof(record->previous.outputs[lane]))!=0&&
           record->candidate_revision[lane]<=record->previous_revision[lane])
            return false;
    return true;
}
bool ot_pz_lane_journal_build(ot_pz_lane_journal_t *out,
                         const ot_pz_lane_config_set_t *previous,
                         const ot_pz_lane_config_set_t *candidate,
                         const uint16_t previous_revision[OT_PZ_OUTPUTS],
                         const uint16_t candidate_revision[OT_PZ_OUTPUTS],
                         uint32_t transaction_id){
    if(!out||!previous||!candidate||!transaction_id||
       !previous_revision||!candidate_revision||
       !ot_pz_lane_config_set_valid(previous)||
       !ot_pz_lane_config_set_valid(candidate)||
       !pz_pair_revisions_valid(previous_revision,candidate_revision))return false;
    ot_pz_lane_journal_t next={0};
    next.magic=OT_PZ_LANE_JOURNAL_MAGIC;
    next.version=OT_PZ_PAIR_SCHEMA_VERSION;
    next.transaction_id=transaction_id;
    next.previous=*previous;next.candidate=*candidate;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(memcmp(&previous->lanes[lane],&candidate->lanes[lane],
                  sizeof(previous->lanes[lane]))!=0&&
           candidate_revision[lane]<=previous_revision[lane])return false;
        next.previous_revision[lane]=previous_revision[lane];
        next.candidate_revision[lane]=candidate_revision[lane];
    }
    next.crc=pz_lane_journal_crc(&next);*out=next;return true;
}
bool ot_pz_lane_journal_valid(const ot_pz_lane_journal_t *record){
    if(!record||record->magic!=OT_PZ_LANE_JOURNAL_MAGIC||
       record->version!=OT_PZ_PAIR_SCHEMA_VERSION||!record->transaction_id||
       pz_lane_journal_crc(record)!=record->crc||
       !ot_pz_lane_config_set_valid(&record->previous)||
       !ot_pz_lane_config_set_valid(&record->candidate)||
       !pz_pair_revisions_valid(record->previous_revision,
                                record->candidate_revision))return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(memcmp(&record->previous.lanes[lane],&record->candidate.lanes[lane],
                  sizeof(record->previous.lanes[lane]))!=0&&
           record->candidate_revision[lane]<=record->previous_revision[lane])
            return false;
    return true;
}
bool ot_pz_pair_intent_build(ot_pz_pair_intent_t *out,uint32_t transaction_id,
                         const uint16_t previous_revision[OT_PZ_OUTPUTS],
                         const uint16_t candidate_revision[OT_PZ_OUTPUTS]){
    if(!out||!transaction_id||
       !pz_pair_revisions_valid(previous_revision,candidate_revision)||
       !pz_pair_has_revision_change(previous_revision,candidate_revision))return false;
    ot_pz_pair_intent_t next={0};
    next.magic=OT_PZ_PAIR_INTENT_MAGIC;
    next.version=OT_PZ_PAIR_SCHEMA_VERSION;
    next.transaction_id=transaction_id;
    memcpy(next.previous_revision,previous_revision,sizeof(next.previous_revision));
    memcpy(next.candidate_revision,candidate_revision,sizeof(next.candidate_revision));
    next.crc=pz_pair_intent_crc(&next);*out=next;return true;
}
bool ot_pz_pair_intent_valid(const ot_pz_pair_intent_t *record){
    return record&&record->magic==OT_PZ_PAIR_INTENT_MAGIC&&
        record->version==OT_PZ_PAIR_SCHEMA_VERSION&&record->transaction_id!=0u&&
        pz_pair_intent_crc(record)==record->crc&&
        pz_pair_revisions_valid(record->previous_revision,record->candidate_revision)&&
        pz_pair_has_revision_change(record->previous_revision,record->candidate_revision);
}
static const ot_pz_map_journal_t *pz_select_map_transaction(
    const ot_pz_map_journal_t *active,const ot_pz_map_journal_t *staged,
    uint32_t transaction_id){
    if(staged&&staged->transaction_id==transaction_id)return staged;
    if(active&&active->transaction_id==transaction_id)return active;
    return NULL;
}
static const ot_pz_lane_journal_t *pz_select_lane_transaction(
    const ot_pz_lane_journal_t *active,const ot_pz_lane_journal_t *staged,
    uint32_t transaction_id){
    if(staged&&staged->transaction_id==transaction_id)return staged;
    if(active&&active->transaction_id==transaction_id)return active;
    return NULL;
}
bool ot_pz_pair_journal_recover(
                         const ot_pz_map_journal_t *map_active,
                         const ot_pz_map_journal_t *map_staged,
                         const ot_pz_lane_journal_t *lane_active,
                         const ot_pz_lane_journal_t *lane_staged,
                         const ot_pz_pair_intent_t *intent,
                         const ot_pz_controller_t *legacy_fallback,
                         ot_pz_controller_t *live,ot_pz_pending_t *pending,
                         uint16_t active_revision[OT_PZ_OUTPUTS],
                         uint16_t staged_revision[OT_PZ_OUTPUTS]){
    if(!live||!pending||!active_revision||!staged_revision||
       (map_active&&!ot_pz_map_journal_valid(map_active))||
       (map_staged&&!ot_pz_map_journal_valid(map_staged))||
       (lane_active&&!ot_pz_lane_journal_valid(lane_active))||
       (lane_staged&&!ot_pz_lane_journal_valid(lane_staged))||
       (intent&&!ot_pz_pair_intent_valid(intent)))return false;

    const ot_pz_map_journal_t *map_record=NULL;
    const ot_pz_lane_journal_t *lane_record=NULL;
    if(intent){
        /* The durable pair intent is written only after both stage records.
         * It authorizes roll-forward across any active/staged write boundary. */
        map_record=pz_select_map_transaction(map_active,map_staged,intent->transaction_id);
        lane_record=pz_select_lane_transaction(lane_active,lane_staged,intent->transaction_id);
        if(!map_record||!lane_record)return false;
        if(memcmp(map_record->previous_revision,intent->previous_revision,
                  sizeof(intent->previous_revision))!=0||
           memcmp(map_record->candidate_revision,intent->candidate_revision,
                  sizeof(intent->candidate_revision))!=0||
           memcmp(lane_record->previous_revision,intent->previous_revision,
                  sizeof(intent->previous_revision))!=0||
           memcmp(lane_record->candidate_revision,intent->candidate_revision,
                  sizeof(intent->candidate_revision))!=0)return false;
    }else if(map_active||lane_active){
        /* Without a commit intent, orphan stage records are not authoritative.
         * Both active namespaces must already name the same complete pair. */
        if(!map_active||!lane_active||
           map_active->transaction_id!=lane_active->transaction_id||
           memcmp(map_active->previous_revision,lane_active->previous_revision,
                  sizeof(map_active->previous_revision))!=0||
           memcmp(map_active->candidate_revision,lane_active->candidate_revision,
                  sizeof(map_active->candidate_revision))!=0)return false;
        map_record=map_active;lane_record=lane_active;
    }else{
        if(legacy_fallback&&!ot_pz_valid(legacy_fallback))return false;
        if(legacy_fallback)*live=*legacy_fallback;
        else ot_pz_defaults(live);
        pending->candidate=*live;pending->pending=false;
        for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
            active_revision[lane]=staged_revision[lane]=1u;
        return true;
    }

    ot_pz_controller_t previous,candidate;
    if(!ot_pz_join(&lane_record->previous,&map_record->previous,&previous)||
       !ot_pz_join(&lane_record->candidate,&map_record->candidate,&candidate))
        return false;
    if(map_record->transaction_id!=lane_record->transaction_id||
       memcmp(map_record->previous_revision,lane_record->previous_revision,
              sizeof(map_record->previous_revision))!=0||
       memcmp(map_record->candidate_revision,lane_record->candidate_revision,
              sizeof(map_record->candidate_revision))!=0)return false;
    *live=previous;pending->candidate=candidate;
    pending->pending=memcmp(&previous,&candidate,sizeof(previous))!=0;
    memcpy(active_revision,map_record->previous_revision,
           sizeof(map_record->previous_revision));
    memcpy(staged_revision,map_record->candidate_revision,
           sizeof(map_record->candidate_revision));
    if(!pending->pending)
        memcpy(active_revision,staged_revision,sizeof(map_record->candidate_revision));
    return true;
}
