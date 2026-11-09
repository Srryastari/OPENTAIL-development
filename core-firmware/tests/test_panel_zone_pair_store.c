#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "panel_zone_pair_store.h"

#define FAKE_BYTES 4096u
typedef struct {bool present;size_t size;uint8_t bytes[FAKE_BYTES];} fake_blob_t;
typedef struct {
    fake_blob_t lane_active,lane_stage,map_active,map_stage,intent;
    unsigned durable_writes;
} fake_store_t;
typedef struct {uint8_t cut_after,last_phase;uint32_t transaction_id;} phase_state_t;

static fake_blob_t *blob_for(fake_store_t *s,ot_pz_pair_namespace_t ns,const char *key){
    if(ns==OT_PZ_PAIR_NS_OUTPUT_CFG){
        if(strcmp(key,OT_PZ_LANE_KEY_ACTIVE)==0)return &s->lane_active;
        if(strcmp(key,OT_PZ_LANE_KEY_STAGED)==0)return &s->lane_stage;
    }else if(ns==OT_PZ_PAIR_NS_ZONE_MAP){
        if(strcmp(key,OT_PZ_MAP_KEY_ACTIVE)==0)return &s->map_active;
        if(strcmp(key,OT_PZ_MAP_KEY_STAGED)==0)return &s->map_stage;
        if(strcmp(key,OT_PZ_PAIR_INTENT_KEY)==0)return &s->intent;
    }
    return NULL;
}
static ot_pz_pair_io_result_t fake_read(void *context,ot_pz_pair_namespace_t ns,
                                        const char *key,void *data,size_t *size){
    fake_store_t *s=context;fake_blob_t *b=blob_for(s,ns,key);
    if(!b||!size)return OT_PZ_PAIR_IO_ERROR;
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
    assert(ot_pz_pair_store_recover(io,fallback,&live,&pending,active,staged,
                                    &intent,&tx,&reset));
    assert(memcmp(&live,expected_live,sizeof(live))==0);
    assert(pending.pending==expected_pending);
    if(expected_pending)
        assert(memcmp(&pending.candidate,expected_candidate,sizeof(*expected_candidate))==0);
    assert(intent==expected_intent&&tx==expected_tx&&reset==expected_reset);
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
        assert(ot_pz_pair_store_begin(&io,&base,&candidate1,r0,r1,0x1001u,0u,
                                      true,NULL,NULL));
        assert(ot_pz_pair_store_finish(&io,0x1001u,NULL,NULL));
        phases.cut_after=cut;
        assert(!ot_pz_pair_store_begin(&io,&candidate1,&candidate2,r1,r2,
                                       0x2002u,0u,false,phase_hook,&phases));
        assert(phases.last_phase==cut);
        if(cut<3u)
            recover_expect(&io,&base,&base,&candidate1,true,false,0x1001u,0u);
        else
            recover_expect(&io,&base,&candidate1,&candidate2,true,true,0x2002u,0u);
    }
    for(uint8_t cut=4u;cut<=8u;cut++){
        fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
        phase_state_t phases={0};
        assert(ot_pz_pair_store_begin(&io,&base,&candidate1,r0,r1,0x1001u,0u,
                                      true,NULL,NULL));
        assert(ot_pz_pair_store_finish(&io,0x1001u,NULL,NULL));
        assert(ot_pz_pair_store_begin(&io,&candidate1,&candidate2,r1,r2,
                                      0x2002u,0u,false,NULL,NULL));
        phases.cut_after=cut;
        assert(!ot_pz_pair_store_finish(&io,0x2002u,phase_hook,&phases));
        assert(phases.last_phase==cut);
        recover_expect(&io,&base,&candidate1,&candidate2,true,cut<8u,0x2002u,0u);
        if(cut<8u)assert(ot_pz_pair_store_finish(&io,0x2002u,NULL,NULL));
        recover_expect(&io,&base,&candidate1,&candidate2,true,false,0x2002u,0u);
        ot_pz_pair_store_snapshot_t snapshot;
        assert(ot_pz_pair_store_snapshot(&io,&snapshot));
        assert(snapshot.has_lane_active&&snapshot.has_map_active);
        assert(!snapshot.has_lane_stage&&!snapshot.has_map_stage&&!snapshot.has_intent);
        assert(snapshot.lane_active.transaction_id==snapshot.map_active.transaction_id);
    }
}
static void reset_marker_and_fail_closed(void){
    fake_store_t store={0};ot_pz_pair_store_io_t io=make_io(&store);
    ot_pz_controller_t base,candidate;
    make_runtime(&base,100u,OT_OM_GRB,100u);
    make_runtime(&candidate,80u,OT_OM_RGB,80u);
    const uint16_t old_rev[OT_PZ_OUTPUTS]={1u,1u},new_rev[OT_PZ_OUTPUTS]={2u,1u};
    assert(ot_pz_pair_store_begin(&io,&base,&candidate,old_rev,new_rev,
                                  0x3003u,0u,true,NULL,NULL));
    assert(ot_pz_pair_store_finish(&io,0x3003u,NULL,NULL));
    assert(ot_pz_pair_store_mark_reset(&io,0u));
    recover_expect(&io,&base,&base,&candidate,true,false,0x3003u,1u);
    store.map_active.bytes[0]^=0x01u;
    ot_pz_controller_t live={0};ot_pz_pending_t pending={0};
    uint16_t ar[2]={0},sr[2]={0};
    assert(!ot_pz_pair_store_recover(&io,&base,&live,&pending,ar,sr,NULL,NULL,NULL));
}
int main(void){
    durable_boundary_matrix();
    reset_marker_and_fail_closed();
    puts("PASS Issue126 v4 runtime pair store: all eight durable boundaries, restart reconciliation, readback, reset isolation and fail-closed corruption");
    return 0;
}
