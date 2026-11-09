#include "panel_zone_pair_store.h"
#include <string.h>

static ot_pz_pair_io_result_t read_exact(const ot_pz_pair_store_io_t *io,
        ot_pz_pair_namespace_t ns,const char *key,void *out,size_t expected){
    size_t length=0u;
    ot_pz_pair_io_result_t result=io->read(io->context,ns,key,NULL,&length);
    if(result!=OT_PZ_PAIR_IO_OK)return result;
    if(length!=expected)return OT_PZ_PAIR_IO_ERROR;
    length=expected;
    result=io->read(io->context,ns,key,out,&length);
    return result==OT_PZ_PAIR_IO_OK&&length==expected?
           OT_PZ_PAIR_IO_OK:OT_PZ_PAIR_IO_ERROR;
}
static bool write_verified(const ot_pz_pair_store_io_t *io,
        ot_pz_pair_store_workspace_t *workspace,
        ot_pz_pair_namespace_t ns,const char *key,const void *record,size_t size){
    if(!workspace||!io->write_commit(io->context,ns,key,record,size))return false;
    if(size>sizeof(workspace->readback))return false;
    return read_exact(io,ns,key,&workspace->readback,size)==OT_PZ_PAIR_IO_OK&&
           memcmp(&workspace->readback,record,size)==0;
}
static uint32_t pair_marker_crc(const ot_pz_pair_store_marker_t *record){
    ot_pz_pair_store_marker_t copy=*record;copy.crc=0u;
    return ot_cfg_crc32(&copy,sizeof(copy));
}
static bool pair_marker_build(ot_pz_pair_store_marker_t *out,uint32_t transaction_id){
    if(!out||!transaction_id)return false;
    ot_pz_pair_store_marker_t next={0};
    next.magic=OT_PZ_PAIR_STORE_MARKER_MAGIC;
    next.version=OT_PZ_PAIR_STORE_MARKER_VERSION;
    next.transaction_id=transaction_id;
    next.crc=pair_marker_crc(&next);*out=next;return true;
}
static bool pair_marker_valid(const ot_pz_pair_store_marker_t *record){
    return record&&record->magic==OT_PZ_PAIR_STORE_MARKER_MAGIC&&
        record->version==OT_PZ_PAIR_STORE_MARKER_VERSION&&
        record->transaction_id&&pair_marker_crc(record)==record->crc;
}
static bool erase_verified(const ot_pz_pair_store_io_t *io,
        ot_pz_pair_namespace_t ns,const char *key){
    if(!io->erase_commit(io->context,ns,key))return false;
    uint8_t probe=0u;
    return read_exact(io,ns,key,&probe,sizeof(probe))==OT_PZ_PAIR_IO_NOT_FOUND;
}
static bool phase_done(ot_pz_pair_phase_hook_t hook,void *context,
                       uint8_t phase,uint32_t transaction_id){
    return !hook||hook(context,phase,transaction_id);
}
static const ot_pz_map_journal_t *select_map(
        const ot_pz_pair_store_snapshot_t *s,uint32_t transaction_id){
    if(s->has_map_stage&&s->map_stage.transaction_id==transaction_id)
        return &s->map_stage;
    if(s->has_map_active&&s->map_active.transaction_id==transaction_id)
        return &s->map_active;
    return NULL;
}
static const ot_pz_lane_journal_t *select_lane(
        const ot_pz_pair_store_snapshot_t *s,uint32_t transaction_id){
    if(s->has_lane_stage&&s->lane_stage.transaction_id==transaction_id)
        return &s->lane_stage;
    if(s->has_lane_active&&s->lane_active.transaction_id==transaction_id)
        return &s->lane_active;
    return NULL;
}
bool ot_pz_pair_store_snapshot(const ot_pz_pair_store_io_t *io,
                               ot_pz_pair_store_workspace_t *workspace){
    if(!io||!io->read||!io->write_commit||!io->erase_commit||!workspace)return false;
    ot_pz_pair_store_snapshot_t *s=&workspace->snapshot;
    memset(s,0,sizeof(*s));
#define READ_RECORD(ns,key,field,has_field) do { \
    ot_pz_pair_io_result_t r=read_exact(io,(ns),(key),&(s->field),sizeof(s->field)); \
    if(r==OT_PZ_PAIR_IO_OK)s->has_field=true; \
    else if(r!=OT_PZ_PAIR_IO_NOT_FOUND)return false; \
} while(0)
    READ_RECORD(OT_PZ_PAIR_NS_OUTPUT_CFG,OT_PZ_LANE_KEY_ACTIVE,lane_active,has_lane_active);
    READ_RECORD(OT_PZ_PAIR_NS_OUTPUT_CFG,OT_PZ_LANE_KEY_STAGED,lane_stage,has_lane_stage);
    READ_RECORD(OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_MAP_KEY_ACTIVE,map_active,has_map_active);
    READ_RECORD(OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_MAP_KEY_STAGED,map_stage,has_map_stage);
    READ_RECORD(OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_PAIR_INTENT_KEY,intent,has_intent);
#undef READ_RECORD
    if(s->has_intent&&!ot_pz_pair_intent_valid(&s->intent))return false;
    /* The marker is a separate durable phase after map-active. If power is
     * interrupted between them, the still-present intent authorizes recovery
     * from the surviving stage/active records and finish() retries both. */
    const ot_pz_pair_io_result_t marker_result=read_exact(io,
        OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_PAIR_STORE_MARKER_KEY,&s->marker,
        sizeof(s->marker));
    if(marker_result==OT_PZ_PAIR_IO_OK)s->has_marker=true;
    else if(marker_result!=OT_PZ_PAIR_IO_NOT_FOUND&&!s->has_intent)return false;
    if(s->has_marker&&!pair_marker_valid(&s->marker)){
        if(!s->has_intent)return false;
        s->has_marker=false;memset(&s->marker,0,sizeof(s->marker));
    }
    if(!s->has_intent&&s->has_map_active&&!s->has_marker)return false;
    if(s->has_marker&&!s->has_intent&&
       (!s->has_map_active||
        s->marker.transaction_id!=s->map_active.transaction_id))return false;
    if((s->has_lane_active&&!ot_pz_lane_journal_valid(&s->lane_active))||
       (s->has_lane_stage&&!ot_pz_lane_journal_valid(&s->lane_stage))||
       (s->has_map_active&&!ot_pz_map_journal_valid(&s->map_active))||
       (s->has_map_stage&&!ot_pz_map_journal_valid(&s->map_stage)))return false;
    return true;
}
bool ot_pz_pair_store_recover(const ot_pz_pair_store_io_t *io,
                              ot_pz_pair_store_workspace_t *workspace,
                              const ot_pz_controller_t *legacy_fallback,
                              ot_pz_controller_t *live,ot_pz_pending_t *pending,
                              uint16_t active_revision[OT_PZ_OUTPUTS],
                              uint16_t staged_revision[OT_PZ_OUTPUTS],
                              bool *intent_pending,uint32_t *transaction_id,
                              uint8_t *reset_mask){
    if(!workspace||!ot_pz_pair_store_snapshot(io,workspace))return false;
    const ot_pz_pair_store_snapshot_t *s=&workspace->snapshot;
    if(s->has_marker&&!s->has_intent&&
       (!s->has_map_active||!s->has_lane_active||
        s->map_active.transaction_id!=s->lane_active.transaction_id))
        return false;
    if(!ot_pz_pair_journal_recover(
        s->has_map_active?&s->map_active:NULL,s->has_map_stage?&s->map_stage:NULL,
        s->has_lane_active?&s->lane_active:NULL,s->has_lane_stage?&s->lane_stage:NULL,
        s->has_intent?&s->intent:NULL,legacy_fallback,live,pending,
        active_revision,staged_revision))return false;
    if(intent_pending)*intent_pending=s->has_intent;
    if(transaction_id){
        *transaction_id=s->has_intent?s->intent.transaction_id:
            (s->has_map_active?s->map_active.transaction_id:
             (s->has_lane_active?s->lane_active.transaction_id:0u));
    }
    if(reset_mask){
        const ot_pz_map_journal_t *record=NULL;
        if(s->has_intent)record=select_map(s,s->intent.transaction_id);
        else if(s->has_map_active)record=&s->map_active;
        *reset_mask=record?record->reset_mask:0u;
    }
    return true;
}
bool ot_pz_pair_store_begin(const ot_pz_pair_store_io_t *io,
                            ot_pz_pair_store_workspace_t *workspace,
                            const ot_pz_controller_t *previous,
                            const ot_pz_controller_t *candidate,
                            const uint16_t previous_revision[OT_PZ_OUTPUTS],
                            const uint16_t candidate_revision[OT_PZ_OUTPUTS],
                            uint32_t transaction_id,uint8_t reset_mask,
                            bool allow_empty_active,
                            ot_pz_pair_phase_hook_t phase_hook,void *phase_context){
    if(!io||!workspace||!previous||!candidate||!previous_revision||
       !candidate_revision||!transaction_id||!ot_pz_valid(previous)||
       !ot_pz_valid(candidate))return false;
    if(!ot_pz_pair_store_snapshot(io,workspace))return false;
    ot_pz_pair_store_snapshot_t *s=&workspace->snapshot;
    if(s->has_intent)return false;
    if(s->has_marker&&!s->has_map_active)return false;
    if(s->has_map_active||s->has_lane_active){
        if(!s->has_map_active||!s->has_lane_active||
           s->map_active.transaction_id!=s->lane_active.transaction_id||
           memcmp(s->map_active.previous_revision,s->lane_active.previous_revision,
                  sizeof(s->map_active.previous_revision))!=0||
           memcmp(s->map_active.candidate_revision,s->lane_active.candidate_revision,
                  sizeof(s->map_active.candidate_revision))!=0||
           memcmp(previous_revision,s->map_active.candidate_revision,
                  sizeof(s->map_active.candidate_revision))!=0)return false;
        if(!ot_pz_join(&s->lane_active.previous,&s->map_active.previous,
                       &workspace->previous)||
           !ot_pz_join(&s->lane_active.candidate,&s->map_active.candidate,
                       &workspace->candidate)||
           memcmp(&workspace->candidate,previous,sizeof(workspace->candidate))!=0)
            return false;
    }else if(!allow_empty_active){
        return false;
    }
    if(!ot_pz_split(previous,&workspace->previous_lanes,&workspace->previous_map)||
       !ot_pz_split(candidate,&workspace->candidate_lanes,&workspace->candidate_map)||
       !ot_pz_lane_journal_build(&s->lane_stage,&workspace->previous_lanes,
            &workspace->candidate_lanes,previous_revision,candidate_revision,
            transaction_id)||
       !ot_pz_map_journal_build_reset(&s->map_stage,&workspace->previous_map,
            &workspace->candidate_map,previous_revision,candidate_revision,
            transaction_id,reset_mask)||
       !ot_pz_pair_intent_build(&s->intent,transaction_id,
            previous_revision,candidate_revision))return false;

    if(!write_verified(io,workspace,OT_PZ_PAIR_NS_OUTPUT_CFG,OT_PZ_LANE_KEY_STAGED,
                       &s->lane_stage,sizeof(s->lane_stage))||
       !phase_done(phase_hook,phase_context,1u,transaction_id))return false;
    if(!write_verified(io,workspace,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_MAP_KEY_STAGED,
                       &s->map_stage,sizeof(s->map_stage))||
       !phase_done(phase_hook,phase_context,2u,transaction_id))return false;
    if(!write_verified(io,workspace,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_PAIR_INTENT_KEY,
                       &s->intent,sizeof(s->intent))||
       !phase_done(phase_hook,phase_context,3u,transaction_id))return false;
    return true;
}
bool ot_pz_pair_store_finish(const ot_pz_pair_store_io_t *io,
                             ot_pz_pair_store_workspace_t *workspace,
                             uint32_t transaction_id,
                             ot_pz_pair_phase_hook_t phase_hook,void *phase_context){
    if(!workspace||!ot_pz_pair_store_snapshot(io,workspace))return false;
    ot_pz_pair_store_snapshot_t *s=&workspace->snapshot;
    if(!s->has_intent||(transaction_id&&s->intent.transaction_id!=transaction_id))
        return false;
    transaction_id=s->intent.transaction_id;
    const ot_pz_map_journal_t *map=select_map(s,transaction_id);
    const ot_pz_lane_journal_t *lane=select_lane(s,transaction_id);
    if(!map||!lane||
       memcmp(map->previous_revision,s->intent.previous_revision,
              sizeof(s->intent.previous_revision))!=0||
       memcmp(map->candidate_revision,s->intent.candidate_revision,
              sizeof(s->intent.candidate_revision))!=0||
       memcmp(lane->previous_revision,s->intent.previous_revision,
              sizeof(s->intent.previous_revision))!=0||
       memcmp(lane->candidate_revision,s->intent.candidate_revision,
              sizeof(s->intent.candidate_revision))!=0)return false;
    if(!ot_pz_join(&lane->previous,&map->previous,&workspace->previous)||
       !ot_pz_join(&lane->candidate,&map->candidate,&workspace->candidate))return false;

    if(!write_verified(io,workspace,OT_PZ_PAIR_NS_OUTPUT_CFG,OT_PZ_LANE_KEY_ACTIVE,
                       lane,sizeof(*lane))||
       !phase_done(phase_hook,phase_context,4u,transaction_id))return false;
    if(!write_verified(io,workspace,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_MAP_KEY_ACTIVE,
                       map,sizeof(*map))||
       !phase_done(phase_hook,phase_context,5u,transaction_id))return false;
    if(!pair_marker_build(&s->marker,transaction_id)||
       !write_verified(io,workspace,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_PAIR_STORE_MARKER_KEY,
                       &s->marker,sizeof(s->marker))||
       !phase_done(phase_hook,phase_context,6u,transaction_id))return false;
    if(!erase_verified(io,OT_PZ_PAIR_NS_OUTPUT_CFG,OT_PZ_LANE_KEY_STAGED)||
       !phase_done(phase_hook,phase_context,7u,transaction_id))return false;
    if(!erase_verified(io,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_MAP_KEY_STAGED)||
       !phase_done(phase_hook,phase_context,8u,transaction_id))return false;
    if(!erase_verified(io,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_PAIR_INTENT_KEY)||
       !phase_done(phase_hook,phase_context,9u,transaction_id))return false;
    return true;
}
bool ot_pz_pair_store_mark_reset(const ot_pz_pair_store_io_t *io,
                                 ot_pz_pair_store_workspace_t *workspace,
                                 uint8_t lane){
    if(lane>=OT_PZ_OUTPUTS||!workspace||
       !ot_pz_pair_store_snapshot(io,workspace))return false;
    ot_pz_pair_store_snapshot_t *s=&workspace->snapshot;
    if(s->has_intent||!s->has_map_active||!s->has_lane_active||
       s->map_active.transaction_id!=s->lane_active.transaction_id||
       memcmp(s->map_active.previous_revision,s->lane_active.previous_revision,
              sizeof(s->map_active.previous_revision))!=0||
       memcmp(s->map_active.candidate_revision,s->lane_active.candidate_revision,
              sizeof(s->map_active.candidate_revision))!=0)return false;
    if(!ot_pz_join(&s->lane_active.previous,&s->map_active.previous,
                   &workspace->previous)||
       !ot_pz_join(&s->lane_active.candidate,&s->map_active.candidate,
                   &workspace->candidate))return false;
    const uint8_t mask=(uint8_t)(s->map_active.reset_mask|(1u<<lane));
    if(!ot_pz_map_journal_build_reset(&s->map_stage,&s->map_active.previous,
        &s->map_active.candidate,s->map_active.previous_revision,
        s->map_active.candidate_revision,s->map_active.transaction_id,mask))
        return false;
    return write_verified(io,workspace,OT_PZ_PAIR_NS_ZONE_MAP,OT_PZ_MAP_KEY_ACTIVE,
                          &s->map_stage,sizeof(s->map_stage));
}
