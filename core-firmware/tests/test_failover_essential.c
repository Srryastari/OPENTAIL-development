#include "ot_failover_essential.h"
#include "board_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void named(char dest[OT_OM_NAME_LEN],const char *name){
    memset(dest,0,OT_OM_NAME_LEN);
    snprintf(dest,OT_OM_NAME_LEN,"%s",name);
}
static void install(ot_om_output_t out[OT_OM_OUTPUTS],ot_tm_profile_t *map){
    ot_om_defaults(out);
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        out[i].color_order=(uint8_t)(i?OT_OM_BGR:OT_OM_GRB);
        named(out[i].name,i?"Right independent":"Left independent");
    }
    /* Arbitrary installer-authored zone geometry, not GS400 defaults. */
    out[0].active_pixels=8;out[0].zone_count=4;
    named(out[0].zones[0].name,"Park and brake");
    named(out[0].zones[1].name,"Left arrow");
    named(out[0].zones[2].name,"Reverse");
    named(out[0].zones[3].name,"Decorative");
    const uint8_t lroles[]={OT_OM_PARK_BRAKE,OT_OM_LEFT,OT_OM_REVERSE,OT_OM_DECORATIVE};
    for(unsigned j=0;j<4;j++){
        out[0].zones[j].pixels=2;out[0].zones[j].role=lroles[j];
    }
    out[1].active_pixels=6;out[1].zone_count=3;
    named(out[1].zones[0].name,"Right");
    named(out[1].zones[1].name,"Park");
    named(out[1].zones[2].name,"Aux blackout");
    const uint8_t rroles[]={OT_OM_RIGHT,OT_OM_PARK,OT_OM_AUX1};
    for(unsigned j=0;j<3;j++){
        out[1].zones[j].pixels=2;out[1].zones[j].role=rroles[j];
    }
    ot_tm_defaults(map);
    assert(ot_om_valid(&out[0])&&ot_om_valid(&out[1])&&ot_tm_valid(map));
}
static ot_fo_profile_t profile(void){
    ot_fo_profile_t p={0};
    ot_om_output_t out[OT_OM_OUTPUTS]={0};
    ot_tm_profile_t map={0};
    install(out,&map);
    uint16_t revision[OT_OM_OUTPUTS]={42u,71u};
    assert(ot_fo_profile_seal(&p,out,&map,revision,19u,99u,true));
    assert(ot_fo_profile_valid(&p));
    return p;
}
static void essential_render_all_masks(void){
    ot_fo_profile_t p=profile();
    for(unsigned mask=0;mask<256;mask++){
        const uint8_t resolved=ot_tm_resolve(&p.trigger,(uint8_t)mask);
        for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
            ot_rgb_t frame[32]={0};
            for(unsigned i=0;i<32;i++)frame[i]=(ot_rgb_t){177,178,179};
            assert(ot_fo_render_essential(&p,o,(uint8_t)mask,32u,frame,32));
            for(unsigned px=0;px<p.output[o].active_pixels;px++){
                const int z=ot_om_section(&p.output[o],px);
                assert(z>=0);
                const uint8_t role=p.output[o].zones[z].role;
                ot_rgb_t wanted={0,0,0};
                if(role<=OT_OM_REVERSE && role!=OT_OM_OFF)
                    wanted=ot_om_trigger_color(role,resolved,32u);
                assert(!memcmp(&wanted,&frame[px],sizeof(wanted)));
            }
            /* No writes beyond the output's own active pixel count. */
            for(unsigned px=p.output[o].active_pixels;px<32;px++)
                assert(frame[px].r==177&&frame[px].g==178&&frame[px].b==179);
        }
    }
    ot_rgb_t small[8]={{9,8,7}};
    assert(!ot_fo_render_essential(&p,0,2,32,small,7));
    assert(small[0].r==9&&small[0].g==8&&small[0].b==7);
    assert(!ot_fo_render_essential(&p,OT_OM_OUTPUTS,2,32,small,8));
    /* Prove correct critical priorities with a deliberately authored map. */
    ot_rgb_t frame[8]={0};
    assert(ot_fo_render_essential(&p,0,3u,32,frame,8)); /* Park + Brake */
    assert(frame[0].r>frame[0].g && frame[0].r>0 && frame[0].b==0);
    assert(ot_fo_render_essential(&p,0,14u,32,frame,8)); /* Brake + hazards */
    assert(frame[0].r>0&&frame[2].r>0&&frame[2].g>0);
    assert(ot_fo_render_essential(&p,0,31u,32,frame,8)); /* All essential */
    assert(frame[4].r==frame[4].g&&frame[4].g==frame[4].b&&frame[4].r>0);
    assert(frame[6].r==0&&frame[6].g==0&&frame[6].b==0);
}
static void integrity_and_activation(void){
    ot_fo_profile_t live=profile(),backup=live;
    assert(ot_fo_profiles_match(&live,&backup));
    assert(ot_fo_can_activate_profile(&live,&backup,0,0));
    for(unsigned mask=1;mask<256;mask++){
        assert(!ot_fo_can_activate_profile(&live,&backup,(uint8_t)mask,0));
        assert(!ot_fo_can_activate_profile(&live,&backup,0,(uint8_t)mask));
    }
    backup.output[0].zones[0].role=OT_OM_DECORATIVE;
    assert(!ot_fo_profile_valid(&backup));
    assert(!ot_fo_profiles_match(&live,&backup));
    backup=live;
    backup.trigger.function[1]=OT_TM_DISABLED;
    assert(!ot_fo_profile_valid(&backup)); /* cannot lose independent Brake */
    backup=live;
    backup.output_revision[0]++;
    assert(!ot_fo_profile_valid(&backup));
    backup=live;
    backup.generation++;
    assert(!ot_fo_profile_valid(&backup));
    backup=live;
    backup.digest^=1u;
    assert(!ot_fo_profile_valid(&backup));
    backup=live;
    backup.output[0].color_order=8;
    assert(!ot_fo_profile_valid(&backup));
    backup=live;
    backup.output[0].active_pixels++;
    assert(!ot_fo_profile_valid(&backup));
    backup=live;
    backup.output[0].zones[0].pixels=0;
    assert(!ot_fo_profile_valid(&backup));
    backup=live;
    ot_om_output_t out[OT_OM_OUTPUTS]={0};
    ot_tm_profile_t tr={0};
    install(out,&tr);
    uint16_t rev[OT_OM_OUTPUTS]={42,71};
    assert(!ot_fo_profile_seal(&backup,out,&tr,rev,19u,100u,false));
    assert(!ot_fo_profile_valid(&backup));
    tr.function[1]=OT_TM_DISABLED;
    assert(!ot_fo_profile_seal(&backup,out,&tr,rev,19u,100u,true));
    install(out,&tr);
    out[0].zones[2].role=OT_OM_OFF;
    assert(!ot_fo_profile_seal(&backup,out,&tr,rev,19u,100u,true));
    install(out,&tr);
    rev[0]=0;
    assert(!ot_fo_profile_seal(&backup,out,&tr,rev,19u,100u,true));
    rev[0]=42;
    assert(ot_fo_profile_seal(&backup,out,&tr,rev,19u,100u,true));
    assert(!ot_fo_profiles_match(&live,&backup)); /* new sealed generation */
    /* No volatile overlay, Gamma or AUX is applied to any backup safety color. */
    backup=live;
    backup.output[0].rgb_gain[0]=0;
    assert(!ot_fo_profile_valid(&backup)); /* stale hash rejected */
}
static void ownership_contract(void){
    ot_fo_profile_t p=profile();
    ot_fo_qualification_t q={
        .interlock_physically_verified=true,.primary_grant=true,
        .local_rail_ok=true,.independent_input_sampler_ok=true,
        .completed_frame_progress_ok=true,.local_watchdog_ok=true,
        .same_committed_profile_on_both_mcus=true,
        .break_before_make_verified=true,.fresh_full_frame_after_handoff=true
    };
    assert(ot_fo_assess_grant(OT_FO_PRIMARY,&p,&q)==OT_FO_GRANT_QUALIFIED);
    assert(ot_fo_assess_grant(OT_FO_BACKUP,&p,&q)==OT_FO_DENY_NO_GRANT);
    q.backup_grant=true;
    assert(ot_fo_assess_grant(OT_FO_PRIMARY,&p,&q)==OT_FO_DENY_CONTENTION);
    assert(ot_fo_assess_grant(OT_FO_BACKUP,&p,&q)==OT_FO_DENY_CONTENTION);
    q.primary_grant=false;
    assert(ot_fo_assess_grant(OT_FO_BACKUP,&p,&q)==OT_FO_GRANT_QUALIFIED);
    q.interlock_physically_verified=false;
    assert(ot_fo_assess_grant(OT_FO_BACKUP,&p,&q)==OT_FO_DENY_NO_INTERLOCK);
    q.interlock_physically_verified=true;
    q.fresh_full_frame_after_handoff=false;
    assert(ot_fo_assess_grant(OT_FO_BACKUP,&p,&q)==OT_FO_DENY_HANDOFF_INCOMPLETE);
    q.fresh_full_frame_after_handoff=true;
    for(unsigned bit=0;bit<32;bit++){
        ot_fo_qualification_t t=q;
        t.local_rail_ok=(bit&1u)!=0;
        t.independent_input_sampler_ok=(bit&2u)!=0;
        t.completed_frame_progress_ok=(bit&4u)!=0;
        t.local_watchdog_ok=(bit&8u)!=0;
        t.same_committed_profile_on_both_mcus=(bit&16u)!=0;
        assert((ot_fo_assess_grant(OT_FO_BACKUP,&p,&t)==OT_FO_GRANT_QUALIFIED)==
               (bit==31u));
    }
    assert(ot_fo_assess_grant((ot_fo_role_t)0,&p,&q)==OT_FO_DENY_INVALID);
    assert(ot_fo_assess_grant(OT_FO_BACKUP,NULL,&q)==OT_FO_DENY_INVALID);
}
int main(void){
    essential_render_all_masks();
    integrity_and_activation();
    ownership_contract();
    printf("failover software policy: PASS (256 masks x 2 channels, integrity, revisions, interlock and no-contention policy; NO hardware takeover)\n");
    return 0;
}
