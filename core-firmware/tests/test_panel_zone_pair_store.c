#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "panel_zone_pair_store.h"

#define FAKE_BYTES 4096u
typedef struct {bool present;size_t size;uint8_t bytes[FAKE_BYTES];} fake_blob_t;
typedef struct {
    fake_blob_t lane_active,lane_stage,map_active,map_stage,intent,marker;
    unsigned durable_writes;
    bool marker_read_error;
} fake_store_t;
typedef struct {uint8_t cut_after,last_phase;uint32_t transaction_id;} phase_state_t;
static ot_pz_pair_store_workspace_t workspace;

static fake_blob_t *blob_for(fake_store_t *s,ot_pz_pair_namespace_t ns,const char *key){
    if(ns==OT_PZ_PAIR_NS_OUTPUT_CFG){
        if(strcmp(key,OT_PZ_LANE_KEY_ACTIVE)==0)return &s->lane_active;
        if(strcmp(key,OT_PZ_LANE_KEY_STAGED)==0)return &s->lane_stage;
    }else if(ns==OT_PZ_PAIR_NS_ZONE_MAP){
        if(strcmp(key,OT_PZ_MAP_KEY_ACTIVE)==0)return &s->map_active;
        if(strcmp(key,OT_PZ_MAP_KEY_STAGED)==0)return &s->map_stage;
        if(strcmp(key,OT_PZ_PAIR_INTENT_KEY)==0)return &s->intent;
        if(strcmp(key,OT_PZ_PAIR_STORE_MARKER_KEY)==0)return &s->marker;
    }
    return NULL;
}
static ot_pz_pair_io_result_t fake_read(void *context,ot_pz_pair_namespace_t ns,
                                        const char *key,void *data,size_t *size){
    fake_store_t *s=context;fake_blob_t *b=blob_for(s,ns,key);
    if(!b||!size)return OT_PZ_PAIR_IO_ERROR;
    if(ns==OT_PZ_PAIR_NS_ZONE_MAP&&
       strcmp(key,OT_PZ_PAIR_STORE_MARKER_KEY)==0&&s->marker_read_error)
        return OT_PZ_PAIR_IO_ERROR;
    if(!b->present)return OT_PZ_PAIR_IO_NOT_FOUND;
    if(!data){*size=b->size;return OT_PZ_PAIR_IO_OK;}
    if(*size<b->size){*size=b->size;return OT_PZ_PAIR_IO_ERROR;}
    memcpy(data,b->bytes,b->size);*size=b->size;return OT_PZ_PAIR_IO_OK;
}
static bool fake_write_commit(void *context,ot_pz_pair_namespace_t ns,
                              const char *key,const void *data,size_t size){
    fake_store_t *s=context;fake_blob_t *b=blob_for(s,ns,key);
    if(!b||!data||size>FAKE_BYTES)return false;
    memcpy(b->bytes,data,size);b->size=size;b->present=true;s->durable_writes++;
    return true;
}
static bool fake_erase_commit(void *context,ot_pz_pair_namespace_t ns,const char *key){
    fake_store_t *s=context;fake_blob_t *b=blob_for(s,ns,key);
    if(!b)return false;
    memset(b,0,sizeof(*b));s->durable_writes++;return true;
}
static bool phase_hook(void *context,uint8_t phase,uint32_t transaction_id){
    phase_state_t *state=context;
    state->last_phase=phase;state->transaction_id=transaction_id;
    return state->cut_after!=phase;
}
static ot_pz_pair_store_io_t make_io(fake_store_t *store){
    return (ot_pz_pair_store_io_t){store,fake_read,fake_write_commit,fake_erase_commit};
}
typedef struct {
    uint32_t magic,version,crc;
    uint8_t payload[13];
} crc_fixture_t;
static void zeroed_crc_compatibility(void){
    crc_fixture_t record;memset(&record,0,sizeof(record));
    record.magic=0x12345678u;record.version=7u;record.crc=0xabcdef01u;
    for(unsigned i=0;i<sizeof(record.payload);i++)record.payload[i]=(uint8_t)i;
    crc_fixture_t copy;memcpy(&copy,&record,sizeof(copy));copy.crc=0u;
    assert(ot_cfg_crc32_zeroed(&record,sizeof(record),offsetof(crc_fixture_t,crc),
                               sizeof(record.crc))==ot_cfg_crc32(&copy,sizeof(copy)));
}
static void make_runtime(ot_pz_controller_t *cfg,uint16_t pixels,uint8_t order,
                         uint16_t range_count){
    ot_pz_defaults(cfg);
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        ot_pz_output_t *out=&cfg->outputs[lane];
        memset(out,0,sizeof(*out));
        (void)snprintf(out->name,sizeof(out->name),"Pixel OUT %u",lane+1u);
        out->active_pixels=lane==0u?pixels:50u;
        out->color_order=lane==0u?order:OT_OM_GRB;
        for(unsigned c=0;c<3u;c++)out->rgb_gain[c]=out->white_gain[c]=255u;
        out->panel_count=1u;out->panels[0].id=(uint16_t)(100u+lane);
        out->panels[0].lane=(uint8_t)lane;
        (void)snprintf(out->panels[0].name,sizeof(out->panels[0].name),"Panel %u",lane);
        out->panels[0].zone_count=1u;out->zone_count=1u;
        out->zones[0].id=(uint16_t)(200u+lane);
        out->zones[0].panel_id=out->panels[0].id;
        out->zones[0].role=OT_OM_DECORATIVE;
        (void)snprintf(out->zones[0].name,sizeof(out->zones[0].name),"Zone %u",lane);
        out->zones[0].range_count=1u;
        out->zones[0].ranges[0]=(ot_pz_range_t){0u,lane==0u?range_count:50u};
    }
    assert(ot_pz_valid(cfg));
}
static void recover_expect(const ot_pz_pair_store_io_t *io,
        const ot_pz_controller_t *fallback,const ot_pz_controller_t *expected_live,
        const ot_pz_controller_t *expected_candidate,bool expected_pending,
        bool expected_intent,uint32_t expected_tx,uint8_t expected_reset){
    ot_pz_controller_t live={0};ot_pz_pending_t pending={0};
    uint16_t active[OT_PZ_OUTPUTS]={0},staged[OT_PZ_OUTPUTS]={0};
    bool intent=false;uint32_t tx=0u;uint8_t reset=0u;
    assert(ot_pz_pair_store_recover(io,&workspace,fallback,&live,&pending,active,staged,
                                    &intent,&tx,&reset));
    assert(memcmp(&live,expected_live,sizeof(live))==0);
    assert(pending.pending==expected_pending);
    if(expected_pending)
        assert(memcmp(&pending.candidate,expected_candidate,sizeof(*expected_candidate))==0);
    assert(intent==expected_intent&&tx==expected_tx&&reset==expected_reset);
}
static void initial_migration_cut_matrix(void){
    ot_pz_controller_t legacy,candidate;
    make_runtime(&legacy,100u,OT_OM_GRB,100u);
    make_runtime(&candidate,80u,OT_OM_RGB,80u);
    const uint16_t previous_revision[OT_PZ_OUTPUTS]={1u,1u};
    const uint16_t candidate_revision[OT_PZ_OUTPUTS]={2u,1u};
    for(uint8_t cut=1u;cut<=3u;cut++){
        fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
        phase_state_t phases={.cut_after=cut};
        assert(!ot_pz_pair_store_begin(&io,&workspace,&legacy,&candidate,
            previous_revision,candidate_revision,0x0a0au,0u,true,
            phase_hook,&phases));
        assert(phases.last_phase==cut);
        assert(store.durable_writes==cut);
        recover_expect(&io,&legacy,&legacy,&candidate,cut==3u,cut==3u,
                       cut==3u?0x0a0au:0u,0u);
        assert(ot_pz_pair_store_snapshot(&io,&workspace));
        assert(!workspace.snapshot.has_marker);
    }
}
static void marker_read_error_fails_closed(void){
    fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
    ot_pz_controller_t base,candidate;
    make_runtime(&base,100u,OT_OM_GRB,100u);
    make_runtime(&candidate,80u,OT_OM_RGB,80u);
    const uint16_t previous_revision[OT_PZ_OUTPUTS]={1u,1u};
    const uint16_t candidate_revision[OT_PZ_OUTPUTS]={2u,1u};
    phase_state_t phases={.cut_after=3u};
    /* Leave a durable intent with staged records but no marker (power cut
     * after phase 3). NOT_FOUND is recoverable; an actual read error is not. */
    assert(!ot_pz_pair_store_begin(&io,&workspace,&base,&candidate,
        previous_revision,candidate_revision,0x3a3au,0u,true,
        phase_hook,&phases));
    assert(store.intent.present&&!store.marker.present);
    store.marker_read_error=true;
    assert(!ot_pz_pair_store_snapshot(&io,&workspace));
    ot_pz_controller_t live={0};ot_pz_pending_t pending={0};
    uint16_t active[OT_PZ_OUTPUTS]={0},staged[OT_PZ_OUTPUTS]={0};
    bool intent=false;uint32_t transaction=0u;uint8_t reset=0u;
    assert(!ot_pz_pair_store_recover(&io,&workspace,&base,&live,&pending,
        active,staged,&intent,&transaction,&reset));
}
static void durable_boundary_matrix(void){
    ot_pz_controller_t base,candidate1,candidate2;
    make_runtime(&base,100u,OT_OM_GRB,100u);
    make_runtime(&candidate1,80u,OT_OM_RGB,80u);
    candidate1.outputs[0].rgb_gain[1]=177u;
    make_runtime(&candidate2,70u,OT_OM_BGR,70u);
    candidate2.outputs[0].white_gain[2]=211u;
    assert(ot_pz_valid(&base)&&ot_pz_valid(&candidate1)&&ot_pz_valid(&candidate2));
    const uint16_t r0[OT_PZ_OUTPUTS]={1u,1u};
    const uint16_t r1[OT_PZ_OUTPUTS]={2u,1u};
    const uint16_t r2[OT_PZ_OUTPUTS]={3u,1u};

    for(uint8_t cut=1u;cut<=3u;cut++){
        fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
        phase_state_t phases={0};
        assert(ot_pz_pair_store_begin(&io,&workspace,&base,&candidate1,r0,r1,0x1001u,0u,
                                      true,NULL,NULL));
        assert(ot_pz_pair_store_finish(&io,&workspace,0x1001u,NULL,NULL));
        assert(store.durable_writes==9u);
        phases.cut_after=cut;
        assert(!ot_pz_pair_store_begin(&io,&workspace,&candidate1,&candidate2,r1,r2,
                                       0x2002u,0u,false,phase_hook,&phases));
        assert(phases.last_phase==cut);
        if(cut<3u)
            recover_expect(&io,&base,&base,&candidate1,true,false,0x1001u,0u);
        else
            recover_expect(&io,&base,&candidate1,&candidate2,true,true,0x2002u,0u);
    }
    for(uint8_t cut=4u;cut<=9u;cut++){
        fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
        phase_state_t phases={0};
        assert(ot_pz_pair_store_begin(&io,&workspace,&base,&candidate1,r0,r1,0x1001u,0u,
                                      true,NULL,NULL));
        assert(ot_pz_pair_store_finish(&io,&workspace,0x1001u,NULL,NULL));
        assert(ot_pz_pair_store_begin(&io,&workspace,&candidate1,&candidate2,r1,r2,
                                      0x2002u,0u,false,NULL,NULL));
        phases.cut_after=cut;
        assert(!ot_pz_pair_store_finish(&io,&workspace,0x2002u,phase_hook,&phases));
        assert(phases.last_phase==cut);
        recover_expect(&io,&base,&candidate1,&candidate2,true,cut<9u,0x2002u,0u);
        if(cut<9u)assert(ot_pz_pair_store_finish(&io,&workspace,0x2002u,NULL,NULL));
        recover_expect(&io,&base,&candidate1,&candidate2,true,false,0x2002u,0u);
        assert(ot_pz_pair_store_snapshot(&io,&workspace));
        assert(workspace.snapshot.has_lane_active&&workspace.snapshot.has_map_active&&workspace.snapshot.has_marker);
        assert(!workspace.snapshot.has_lane_stage&&!workspace.snapshot.has_map_stage&&!workspace.snapshot.has_intent);
        assert(workspace.snapshot.lane_active.transaction_id==workspace.snapshot.map_active.transaction_id);
        assert(workspace.snapshot.marker.transaction_id==workspace.snapshot.map_active.transaction_id);
    }
}
/* Actual split-namespace journal: saving only metadata on O1 must stage
 * the complete owner draft once, retain O2 byte-identical, and survive
 * restart readback through the normal v4 lane/map pair recovery path. */
static void metadata_only_one_save_pair_recovery(void){
    fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
    ot_pz_controller_t base,selected;
    make_runtime(&base,100u,OT_OM_GRB,100u);
    selected=base;
    ot_pz_output_t *o=&selected.outputs[0];
    (void)snprintf(o->name,sizeof(o->name),"Selected output");
    (void)snprintf(o->panels[0].name,sizeof(o->panels[0].name),"Left panel");
    (void)snprintf(o->zones[0].name,sizeof(o->zones[0].name),"Left accent");
    o->color_order=OT_OM_RGB;
    o->rgb_gain[0]=117u;
    o->white_gain[2]=204u;
    o->gamma_enabled=true;
    assert(ot_pz_valid(&selected));
    assert(ot_pz_live_geometry_matches(&base.outputs[0],o,0u));
    assert(memcmp(&base.outputs[0],o,sizeof(*o))!=0);
    assert(memcmp(&base.outputs[1],&selected.outputs[1],
                  sizeof(base.outputs[1]))==0);
    const uint16_t from[OT_PZ_OUTPUTS]={1u,1u};
    const uint16_t to[OT_PZ_OUTPUTS]={2u,1u};
    assert(ot_pz_pair_store_begin(&io,&workspace,&base,&selected,
        from,to,0x4d455441u,0u,true,NULL,NULL));
    assert(ot_pz_pair_store_finish(&io,&workspace,0x4d455441u,NULL,NULL));
    assert(store.durable_writes==9u);
    recover_expect(&io,&base,&base,&selected,true,false,0x4d455441u,0u);
    assert(ot_pz_pair_store_snapshot(&io,&workspace));
    assert(workspace.snapshot.has_lane_active&&workspace.snapshot.has_map_active);
    assert(workspace.snapshot.lane_active.candidate.lanes[0].rgb_gain[0]==117u);
    assert(workspace.snapshot.lane_active.candidate.lanes[0].white_gain[2]==204u);
    assert(workspace.snapshot.map_active.candidate.outputs[0].zones[0].id==
           base.outputs[0].zones[0].id);
    assert(workspace.snapshot.map_active.candidate.outputs[0].zones[0].role==
           base.outputs[0].zones[0].role);
    assert(workspace.snapshot.map_active.candidate.outputs[0].zones[0].ranges[0].count==100u);
    assert(memcmp(&workspace.snapshot.map_active.candidate.outputs[1],
                  &workspace.snapshot.map_active.previous.outputs[1],
                  sizeof(workspace.snapshot.map_active.candidate.outputs[1]))==0);
}
static void reset_marker_rejects_pending_intent(void){
    fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
    ot_pz_controller_t base,candidate1,candidate2;
    make_runtime(&base,100u,OT_OM_GRB,100u);
    make_runtime(&candidate1,80u,OT_OM_RGB,80u);
    const uint16_t rev0[OT_PZ_OUTPUTS]={1u,1u};
    const uint16_t rev1[OT_PZ_OUTPUTS]={2u,2u};
    assert(ot_pz_pair_store_begin(&io,&workspace,&base,&candidate1,
        rev0,rev1,0x2111u,0u,true,NULL,NULL));
    assert(ot_pz_pair_store_finish(&io,&workspace,0x2111u,NULL,NULL));
    make_runtime(&candidate2,60u,OT_OM_GRB,60u);
    const uint16_t rev2[OT_PZ_OUTPUTS]={3u,3u};
    phase_state_t cut={.cut_after=3u};
    assert(!ot_pz_pair_store_begin(&io,&workspace,&candidate1,&candidate2,
        rev1,rev2,0x2112u,0u,false,phase_hook,&cut));
    assert(store.intent.present);
    const unsigned writes=store.durable_writes;
    assert(!ot_pz_pair_store_mark_reset(&io,&workspace,0u));
    assert(store.durable_writes==writes);
}
static void reset_marker_first_write_and_idempotence(void){
    fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
    ot_pz_controller_t base,candidate;
    make_runtime(&base,100u,OT_OM_GRB,100u);candidate=base;
    candidate.outputs[0].panel_count=0u;candidate.outputs[0].zone_count=0u;
    memset(candidate.outputs[0].panels,0,sizeof(candidate.outputs[0].panels));
    memset(candidate.outputs[0].zones,0,sizeof(candidate.outputs[0].zones));
    const uint16_t previous[OT_PZ_OUTPUTS]={1u,1u};
    const uint16_t next[OT_PZ_OUTPUTS]={2u,1u};
    assert(ot_pz_pair_store_begin(&io,&workspace,&base,&candidate,
        previous,next,0x3113u,0u,true,NULL,NULL));
    assert(ot_pz_pair_store_finish(&io,&workspace,0x3113u,NULL,NULL));
    unsigned writes=store.durable_writes;
    assert(ot_pz_pair_store_mark_reset(&io,&workspace,0u));
    assert(store.durable_writes==writes+1u);
    assert(ot_pz_pair_store_snapshot(&io,&workspace));
    assert(workspace.snapshot.map_active.reset_mask==1u);
    writes=store.durable_writes;
    assert(ot_pz_pair_store_mark_reset(&io,&workspace,0u));
    assert(store.durable_writes==writes);
}
static void reset_marker_and_fail_closed(void){
    fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
    ot_pz_controller_t base,candidate;
    make_runtime(&base,100u,OT_OM_GRB,100u);
    candidate=base;
    candidate.outputs[0].panel_count=0u;
    candidate.outputs[0].zone_count=0u;
    memset(candidate.outputs[0].panels,0,sizeof(candidate.outputs[0].panels));
    memset(candidate.outputs[0].zones,0,sizeof(candidate.outputs[0].zones));
    assert(ot_pz_valid(&base)&&ot_pz_valid(&candidate));
    const uint16_t old_rev[OT_PZ_OUTPUTS]={1u,1u},new_rev[OT_PZ_OUTPUTS]={2u,1u};
    assert(ot_pz_pair_store_begin(&io,&workspace,&base,&candidate,old_rev,new_rev,
                                  0x3003u,1u,true,NULL,NULL));
    assert(ot_pz_pair_store_snapshot(&io,&workspace));
    assert(workspace.snapshot.has_lane_stage&&workspace.snapshot.has_map_stage&&workspace.snapshot.has_intent);
    /* Reset changes only geometry: lane electrical configuration is byte-identical. */
    assert(memcmp(&workspace.snapshot.lane_stage.previous,&workspace.snapshot.lane_stage.candidate,
                  sizeof(workspace.snapshot.lane_stage.previous))==0);
    assert(memcmp(&workspace.snapshot.map_stage.previous.outputs[0],
                  &workspace.snapshot.map_stage.candidate.outputs[0],
                  sizeof(workspace.snapshot.map_stage.previous.outputs[0]))!=0);
    assert(memcmp(&workspace.snapshot.map_stage.previous.outputs[1],
                  &workspace.snapshot.map_stage.candidate.outputs[1],
                  sizeof(workspace.snapshot.map_stage.previous.outputs[1]))==0);
    assert(workspace.snapshot.map_stage.reset_mask==1u);
    assert(ot_pz_pair_store_finish(&io,&workspace,0x3003u,NULL,NULL));
    assert(ot_pz_pair_store_mark_reset(&io,&workspace,0u));
    recover_expect(&io,&base,&base,&candidate,true,false,0x3003u,1u);
    assert(ot_pz_pair_store_snapshot(&io,&workspace));
    assert(workspace.snapshot.has_lane_active&&workspace.snapshot.has_map_active&&workspace.snapshot.has_marker);
    assert(memcmp(&workspace.snapshot.lane_active.previous,&workspace.snapshot.lane_active.candidate,
                  sizeof(workspace.snapshot.lane_active.previous))==0);
    assert(workspace.snapshot.map_active.candidate.outputs[0].panel_count==0u);
    assert(workspace.snapshot.map_active.candidate.outputs[0].zone_count==0u);
    assert(workspace.snapshot.map_active.reset_mask==1u);
    /* A completed v4 active pair without its initialization marker is
     * ambiguous and must not silently reopen legacy fallback. */
    fake_store_t missing_marker_store=store;
    missing_marker_store.marker.present=false;
    ot_pz_pair_store_io_t missing_marker_io=make_io(&missing_marker_store);
    ot_pz_controller_t missing_live={0};ot_pz_pending_t missing_pending={0};
    uint16_t missing_ar[2]={0},missing_sr[2]={0};
    assert(!ot_pz_pair_store_recover(&missing_marker_io,&workspace,&base,&missing_live,
        &missing_pending,missing_ar,missing_sr,NULL,NULL,NULL));
    store.map_active.bytes[0]^=0x01u;
    ot_pz_controller_t live={0};ot_pz_pending_t pending={0};
    uint16_t ar[2]={0},sr[2]={0};
    assert(!ot_pz_pair_store_recover(&io,&workspace,&base,&live,&pending,ar,sr,NULL,NULL,NULL));
    /* If both active records disappear after v4 initialization, the durable
     * marker prevents fallback to stale legacy combined configuration. */
    store.map_active.present=false;store.lane_active.present=false;
    assert(!ot_pz_pair_store_recover(&io,&workspace,&base,&live,&pending,ar,sr,NULL,NULL,NULL));
}
int main(void){
    assert(sizeof(workspace)<=16u*1024u);
    zeroed_crc_compatibility();
    initial_migration_cut_matrix();
    marker_read_error_fails_closed();
    durable_boundary_matrix();
    metadata_only_one_save_pair_recovery();
    reset_marker_rejects_pending_intent();
    reset_marker_first_write_and_idempotence();
    reset_marker_and_fail_closed();
    puts("PASS Issue126 v4 runtime pair store: all nine durable boundaries, metadata-only single Save, restart readback, reset isolation and fail-closed corruption");
    return 0;
}
