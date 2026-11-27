#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config_journal.h"

static void make_output(ot_pz_output_t *out,unsigned lane,uint16_t pixels){
    memset(out,0,sizeof(*out));
    (void)snprintf(out->name,sizeof(out->name),"Physical lane %u",lane+1u);
    out->active_pixels=pixels;out->color_order=OT_OM_GRB;
    for(unsigned c=0;c<3u;c++)out->rgb_gain[c]=out->white_gain[c]=255u;
}
static void make_panel(ot_pz_output_t *out,unsigned lane,uint16_t id,const char *name){
    ot_pz_panel_t *panel=&out->panels[out->panel_count++];
    panel->id=id;panel->lane=(uint8_t)lane;
    (void)snprintf(panel->name,sizeof(panel->name),"%s",name);
}
static void make_zone(ot_pz_output_t *out,unsigned index,uint16_t id,uint16_t panel_id,
                      uint8_t role,const char *name,uint16_t start,uint16_t count){
    ot_pz_zone_t *zone=&out->zones[index];
    zone->id=id;zone->panel_id=panel_id;zone->role=role;
    (void)snprintf(zone->name,sizeof(zone->name),"%s",name);
    zone->range_count=1u;zone->ranges[0]=(ot_pz_range_t){start,count};
    out->zone_count++;
    for(unsigned p=0;p<out->panel_count;p++)
        if(out->panels[p].id==panel_id)out->panels[p].zone_count++;
}
static void make_runtime(ot_pz_controller_t *cfg,uint16_t pixels,
                         uint8_t color_order,uint16_t zone_count){
    ot_pz_defaults(cfg);
    ot_pz_output_t *out=&cfg->outputs[0];make_output(out,0u,pixels);
    out->color_order=color_order;
    out->rgb_gain[0]=191u;out->white_gain[2]=223u;out->gamma_enabled=true;
    make_panel(out,0u,101u,"Rear panel");
    make_zone(out,0u,201u,101u,OT_OM_BRAKE,"Brake zone",0u,zone_count);
    make_output(&cfg->outputs[1],1u,50u);
    assert(ot_pz_valid(cfg));
}
static void assert_pending_pair(const ot_pz_map_journal_t *map_active,
                                const ot_pz_map_journal_t *map_stage,
                                const ot_pz_lane_journal_t *lane_active,
                                const ot_pz_lane_journal_t *lane_stage,
                                const ot_pz_pair_intent_t *intent,
                                const ot_pz_controller_t *previous,
                                const ot_pz_controller_t *candidate){
    ot_pz_controller_t live={0};ot_pz_pending_t pending={0};
    uint16_t active_revision[OT_PZ_OUTPUTS]={0},staged_revision[OT_PZ_OUTPUTS]={0};
    assert(ot_pz_pair_journal_recover(map_active,map_stage,lane_active,lane_stage,
        intent,NULL,&live,&pending,active_revision,staged_revision));
    assert(memcmp(&live,previous,sizeof(live))==0);
    assert(pending.pending);
    assert(memcmp(&pending.candidate,candidate,sizeof(*candidate))==0);
    assert(active_revision[0]==10u&&active_revision[1]==20u);
    assert(staged_revision[0]==11u&&staged_revision[1]==20u);
}
static void pair_recovery_boundaries(void){
    ot_pz_controller_t previous,candidate;
    make_runtime(&previous,100u,OT_OM_GRB,100u);
    make_runtime(&candidate,80u,OT_OM_RGB,80u);
    candidate.outputs[0].rgb_gain[1]=177u;
    assert(ot_pz_valid(&previous)&&ot_pz_valid(&candidate));

    ot_pz_lane_config_set_t previous_lanes,candidate_lanes;
    ot_pz_map_t previous_map,candidate_map;
    assert(ot_pz_split(&previous,&previous_lanes,&previous_map));
    assert(ot_pz_split(&candidate,&candidate_lanes,&candidate_map));
    const uint16_t old_revision[OT_PZ_OUTPUTS]={10u,20u};
    const uint16_t new_revision[OT_PZ_OUTPUTS]={11u,20u};
    ot_pz_map_journal_t map_active={0},map_stage={0};
    ot_pz_lane_journal_t lane_active={0},lane_stage={0};
    ot_pz_pair_intent_t intent={0};
    assert(ot_pz_map_journal_build(&map_active,&previous_map,&previous_map,
        old_revision,old_revision,0x1001u));
    assert(ot_pz_lane_journal_build(&lane_active,&previous_lanes,&previous_lanes,
        old_revision,old_revision,0x1001u));
    assert(ot_pz_map_journal_build(&map_stage,&previous_map,&candidate_map,
        old_revision,new_revision,0x2002u));
    assert(ot_pz_lane_journal_build(&lane_stage,&previous_lanes,&candidate_lanes,
        old_revision,new_revision,0x2002u));
    assert(ot_pz_pair_intent_build(&intent,0x2002u,old_revision,new_revision));
    assert(ot_pz_map_journal_valid(&map_active)&&ot_pz_map_journal_valid(&map_stage));
    assert(ot_pz_lane_journal_valid(&lane_active)&&ot_pz_lane_journal_valid(&lane_stage));
    assert(ot_pz_pair_intent_valid(&intent));

    /* Stable active pair is authoritative. Orphan lane/map stages written
     * before the pair intent must not leak into the live runtime. */
    ot_pz_controller_t live={0};ot_pz_pending_t pending={0};
    uint16_t ar[2]={0},sr[2]={0};
    assert(ot_pz_pair_journal_recover(&map_active,NULL,&lane_active,NULL,NULL,
        &previous,&live,&pending,ar,sr));
    assert(memcmp(&live,&previous,sizeof(live))==0&&!pending.pending);
    assert(ot_pz_pair_journal_recover(&map_active,&map_stage,&lane_active,NULL,NULL,
        &previous,&live,&pending,ar,sr));
    assert(memcmp(&live,&previous,sizeof(live))==0&&!pending.pending);
    assert(ot_pz_pair_journal_recover(&map_active,&map_stage,&lane_active,&lane_stage,
        NULL,&previous,&live,&pending,ar,sr));
    assert(memcmp(&live,&previous,sizeof(live))==0&&!pending.pending);

    /* First migration has no active pair yet. Intent is durable only after
     * both stage records; it selects the complete candidate pair. */
    assert_pending_pair(NULL,&map_stage,NULL,&lane_stage,&intent,&previous,&candidate);

    /* Power loss after either active namespace write: intent plus the other
     * namespace's stage still reconstructs the same complete pair. */
    assert_pending_pair(NULL,&map_stage,&lane_stage,NULL,&intent,&previous,&candidate);
    assert_pending_pair(&map_stage,NULL,NULL,&lane_stage,&intent,&previous,&candidate);
    assert_pending_pair(&map_stage,NULL,&lane_stage,NULL,&intent,&previous,&candidate);

    /* Stage erasure is idempotent. Intent may coexist with zero, one or two
     * surviving stage records as long as both active candidate records exist. */
    assert_pending_pair(&map_stage,NULL,&lane_stage,&lane_stage,&intent,&previous,&candidate);
    assert_pending_pair(&map_stage,NULL,&lane_stage,NULL,&intent,&previous,&candidate);
    /* After intent clear, the matching active pair still retains previous and
     * candidate so reboot defers activation through the physical quiet gate. */
    assert_pending_pair(&map_stage,NULL,&lane_stage,NULL,NULL,&previous,&candidate);

    /* Every input mask must still pass the independent quiet gate only at 0. */
    for(unsigned mask=0;mask<256u;mask++){
        const bool quiet=ot_pz_activation_inputs_quiet((uint8_t)mask,
            (uint8_t)mask,4u,4u,77u,77u);
        assert(quiet==(mask==0u));
    }
}
static void fail_closed_mismatch_and_fallback(void){
    ot_pz_controller_t previous,candidate,live;
    make_runtime(&previous,100u,OT_OM_GRB,100u);
    make_runtime(&candidate,80u,OT_OM_RGB,80u);
    ot_pz_lane_config_set_t pl,cl;ot_pz_map_t pm,cm;
    assert(ot_pz_split(&previous,&pl,&pm)&&ot_pz_split(&candidate,&cl,&cm));
    const uint16_t old_revision[2]={3u,4u},new_revision[2]={4u,4u};
    ot_pz_map_journal_t ma={0},ms={0},bad_map={0};
    ot_pz_lane_journal_t la={0},ls={0},bad_lane={0};
    ot_pz_pair_intent_t intent={0},bad_intent={0};
    assert(ot_pz_map_journal_build(&ma,&pm,&pm,old_revision,old_revision,1u));
    assert(ot_pz_lane_journal_build(&la,&pl,&pl,old_revision,old_revision,1u));
    assert(ot_pz_map_journal_build(&ms,&pm,&cm,old_revision,new_revision,2u));
    assert(ot_pz_lane_journal_build(&ls,&pl,&cl,old_revision,new_revision,2u));
    assert(ot_pz_pair_intent_build(&intent,2u,old_revision,new_revision));
    ot_pz_pending_t pending={0};uint16_t ar[2]={0},sr[2]={0};

    /* A missing half of an active pair or mismatched transaction ID is fatal. */
    assert(!ot_pz_pair_journal_recover(&ma,NULL,NULL,NULL,NULL,&previous,
        &live,&pending,ar,sr));
    assert(!ot_pz_pair_journal_recover(&ma,NULL,&la,NULL,&intent,&previous,
        &live,&pending,ar,sr));
    ot_pz_lane_journal_t mismatched_active=la;
    mismatched_active.transaction_id=9u;
    mismatched_active.crc=0u;
    mismatched_active.crc=ot_cfg_crc32(&mismatched_active,sizeof(mismatched_active));
    assert(ot_pz_lane_journal_valid(&mismatched_active));
    assert(!ot_pz_pair_journal_recover(&ma,NULL,&mismatched_active,NULL,NULL,
        &previous,&live,&pending,ar,sr));

    bad_intent=intent;bad_intent.candidate_revision[0]++;
    bad_intent.crc=0u;bad_intent.crc=ot_cfg_crc32(&bad_intent,sizeof(bad_intent));
    assert(!ot_pz_pair_journal_recover(&ma,&ms,&la,&ls,&bad_intent,&previous,
        &live,&pending,ar,sr));
    bad_map=ms;bad_map.crc^=1u;
    assert(!ot_pz_pair_journal_recover(&ma,&bad_map,&la,&ls,&intent,&previous,
        &live,&pending,ar,sr));
    bad_lane=ls;bad_lane.transaction_id=9u;
    bad_lane.crc=0u;bad_lane.crc=ot_cfg_crc32(&bad_lane,sizeof(bad_lane));
    assert(ot_pz_lane_journal_valid(&bad_lane));
    assert(!ot_pz_pair_journal_recover(&ma,&ms,&la,&bad_lane,&intent,&previous,
        &live,&pending,ar,sr));

    /* A structurally valid map and lane record can still be an invalid joined
     * pair when the map range exceeds the actual hardware lane length. */
    ot_pz_lane_config_set_t short_lanes=cl;
    short_lanes.lanes[0].active_pixels=50u;
    assert(ot_pz_lane_config_set_valid(&short_lanes));
    ot_pz_lane_journal_t short_lane_journal={0};
    assert(ot_pz_lane_journal_build(&short_lane_journal,&pl,&short_lanes,
        old_revision,new_revision,2u));
    assert(!ot_pz_pair_journal_recover(NULL,&ms,NULL,&short_lane_journal,
        &intent,&previous,&live,&pending,ar,sr));

    /* No v4 records means the read-only legacy snapshot remains the fallback.
     * Orphan stages without an intent are never promoted. */
    assert(ot_pz_pair_journal_recover(NULL,&ms,NULL,&ls,NULL,&previous,
        &live,&pending,ar,sr));
    assert(memcmp(&live,&previous,sizeof(live))==0&&!pending.pending);
    assert(ot_pz_pair_journal_recover(NULL,NULL,NULL,NULL,NULL,&previous,
        &live,&pending,ar,sr));
    assert(memcmp(&live,&previous,sizeof(live))==0&&!pending.pending);
    assert(!ot_pz_pair_intent_build(&intent,0u,old_revision,new_revision));
    assert(!ot_pz_pair_intent_build(&intent,2u,old_revision,old_revision));
}
int main(void){
    pair_recovery_boundaries();
    fail_closed_mismatch_and_fallback();
    puts("PASS Issue126 v4 pair journal: split lane/map ownership, intent-gated recovery, every cross-namespace crash boundary and fail-closed mismatch checks");
    return 0;
}
