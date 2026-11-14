#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config_journal.h"

static void make_config(ot_pz_controller_t *cfg,uint16_t pixels,uint8_t role){
    ot_pz_defaults(cfg);
    ot_pz_output_t *o=&cfg->outputs[0];
    (void)snprintf(o->name,sizeof(o->name),"Migrated OUT 1");
    o->active_pixels=pixels;o->panel_count=1u;o->zone_count=1u;
    o->panels[0].id=101u;o->panels[0].lane=0u;
    (void)snprintf(o->panels[0].name,sizeof(o->panels[0].name),"Rear panel");
    o->panels[0].zone_count=1u;
    o->zones[0].id=201u;o->zones[0].panel_id=101u;o->zones[0].role=role;
    (void)snprintf(o->zones[0].name,sizeof(o->zones[0].name),"Brake");
    o->zones[0].range_count=1u;o->zones[0].ranges[0]=(ot_pz_range_t){0,pixels};
    assert(ot_pz_valid(cfg));
}
static void journal_contract(void){
    ot_pz_controller_t base,previous,candidate,live;
    make_config(&base,100u,OT_OM_OFF);
    previous=base;previous.outputs[0].zones[0].role=OT_OM_BRAKE;
    candidate=previous;candidate.outputs[0].zones[0].ranges[0]=(ot_pz_range_t){0,75};
    candidate.outputs[0].active_pixels=75u;
    assert(ot_pz_valid(&previous)&&ot_pz_valid(&candidate));
    uint16_t prev_rev[OT_PZ_OUTPUTS]={41u,8u};
    uint16_t next_rev[OT_PZ_OUTPUTS]={42u,8u};
    ot_pz_journal_t active={0},stage={0};
    assert(ot_pz_journal_build(&active,&base,&previous,(uint16_t[2]){40u,8u},prev_rev));
    assert(ot_pz_journal_build(&stage,&previous,&candidate,prev_rev,next_rev));
    assert(ot_pz_journal_valid(&active)&&ot_pz_journal_valid(&stage));
    ot_pz_pending_t pending={0};uint16_t active_rev[2]={0},staged_rev[2]={0};
    assert(ot_pz_journal_recover(&active,&stage,&live,&pending,active_rev,staged_rev));
    assert(memcmp(&live,&previous,sizeof(live))==0);
    assert(pending.pending&&memcmp(&pending.candidate,&candidate,sizeof(candidate))==0);
    assert(active_rev[0]==41u&&staged_rev[0]==42u);

    /* If stage is absent after its erase, active still holds previous/candidate;
     * boot must retry quiet-window promotion instead of activating during boot. */
    assert(ot_pz_journal_recover(&stage,NULL,&live,&pending,active_rev,staged_rev));
    assert(memcmp(&live,&previous,sizeof(live))==0&&pending.pending);
    assert(active_rev[0]==41u&&staged_rev[0]==42u);

    /* After a successful candidate snapshot (previous==candidate), it is active. */
    ot_pz_journal_t committed={0};
    assert(ot_pz_journal_build(&committed,&candidate,&candidate,next_rev,next_rev));
    assert(ot_pz_journal_recover(&committed,NULL,&live,&pending,active_rev,staged_rev));
    assert(memcmp(&live,&candidate,sizeof(live))==0&&!pending.pending);
    assert(active_rev[0]==42u&&staged_rev[0]==42u);

    /* Revisions cannot wrap, move backward or stay equal across a real edit. */
    assert(!ot_pz_journal_build(&stage,&previous,&candidate,
        (uint16_t[2]){UINT16_MAX,8u},(uint16_t[2]){0u,8u}));
    assert(!ot_pz_journal_build(&stage,&previous,&candidate,prev_rev,prev_rev));
    ot_pz_journal_t corrupt=stage;corrupt.crc^=1u;
    assert(!ot_pz_journal_valid(&corrupt));
    for(size_t i=0;i<sizeof(stage);i++){
        corrupt=stage;((uint8_t*)&corrupt)[i]^=1u;
        assert(!ot_pz_journal_valid(&corrupt));
    }
    assert(!ot_pz_journal_recover(&active,&corrupt,&live,&pending,active_rev,staged_rev));
    assert(ot_pz_journal_recover(&active,&stage,&live,&pending,active_rev,staged_rev));
}
static void all_input_masks_crash_phases(void){
    ot_pz_controller_t old_cfg,new_cfg,live;
    make_config(&old_cfg,623u,OT_OM_BRAKE);
    new_cfg=old_cfg;new_cfg.outputs[0].zones[0].role=OT_OM_REVERSE;
    ot_pz_journal_t primary={0},stage={0};
    uint16_t old_rev[2]={7u,7u},new_rev[2]={8u,7u};
    assert(ot_pz_journal_build(&primary,&old_cfg,&old_cfg,old_rev,old_rev));
    assert(ot_pz_journal_build(&stage,&old_cfg,&new_cfg,old_rev,new_rev));
    for(unsigned mask=0;mask<256u;mask++){
        const bool can_promote=ot_pz_activation_inputs_quiet(
            (uint8_t)mask,(uint8_t)mask,4u,4u,55u,55u);
        assert(can_promote==(mask==0u));
        for(unsigned phase=0;phase<4u;phase++){
            const ot_pz_journal_t *active=phase>=2u?&stage:&primary;
            const ot_pz_journal_t *staged=(phase==1u||phase==2u)?&stage:NULL;
            ot_pz_pending_t pending={0};uint16_t ar[2]={0},sr[2]={0};
            assert(ot_pz_journal_recover(active,staged,&live,&pending,ar,sr));
            assert(memcmp(&live,&old_cfg,sizeof(live))==0);
            assert(pending.pending==(phase!=0u));
            if(phase==0u)assert(memcmp(&pending.candidate,&old_cfg,sizeof(old_cfg))==0);
            else assert(memcmp(&pending.candidate,&new_cfg,sizeof(new_cfg))==0);
        }
    }
}
int main(void){
    journal_contract();all_input_masks_crash_phases();
    puts("PASS Issue126 v3 NVS journal: CRC, monotonic revisions, stage-first recovery, rollback image and 256 input-mask power-cut model");
    return 0;
}
