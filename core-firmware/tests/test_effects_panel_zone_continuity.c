/* Effects-owned FX/PZ runtime continuity regression.
 * Uses the real Panel/Zone mapper, preview model, FX zone bank and effect
 * renderers. The preview commit is a simulated atomic promotion: this test
 * does not claim to exercise ESP NVS, BLE scheduling, RMT or physical optics.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "effects_1d.h"
#include "fx_zone_bank.h"
#include "ot_auto_effects.h"
#include "panel_zone.h"
#include "panel_zone_protocol.h"

#define TEST_PIXELS 16u
#define TEST_BASE_REVISION 7u

static bool same_wled_rgb(ot_wled_rgb_t a,ot_wled_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
static bool same_rgb(ot_rgb_t a,ot_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}

typedef struct {
    ot_effects_1d_state_t state[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint32_t bank_seen_generation[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint32_t bank_seen_refresh_ms[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint16_t bank_owner_zone_id[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint16_t bank_owner_panel_id[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint8_t resolved_bank_index[OT_PZ_OUTPUTS][OT_OM_ZONES];
    ot_wled_rgb_t frame[OT_PZ_OUTPUTS][TEST_PIXELS];
    ot_wled_rgb_t logical_frame[OT_PZ_OUTPUTS][OT_OM_ZONES][TEST_PIXELS];
    uint8_t prior_pixel_zone_map[OT_PZ_OUTPUTS][OT_PZ_MAX_PIXELS];
    uint16_t prior_zone_id[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint16_t prior_panel_id[OT_PZ_OUTPUTS][OT_OM_ZONES];
    uint32_t prior_generation[OT_PZ_OUTPUTS][OT_OM_ZONES];
    bool prior_active[OT_PZ_OUTPUTS][OT_OM_ZONES];
} render_probe_t;

static void make_output(ot_pz_output_t *out,unsigned lane){
    memset(out,0,sizeof(*out));
    snprintf(out->name,sizeof(out->name),"Output %u",lane+1u);
    out->active_pixels=TEST_PIXELS;
    out->color_order=OT_OM_GRB;
    out->panel_count=1u;
    out->zone_count=2u;

    const uint16_t panel_id=(uint16_t)(100u+lane);
    out->panels[0].id=panel_id;
    out->panels[0].lane=(uint8_t)lane;
    snprintf(out->panels[0].name,sizeof(out->panels[0].name),"Housing Panel");
    out->panels[0].zone_count=2u;

    out->zones[0].id=(uint16_t)(200u+lane*10u);
    out->zones[0].panel_id=panel_id;
    out->zones[0].role=lane==0u?OT_OM_LEFT:OT_OM_RIGHT;
    snprintf(out->zones[0].name,sizeof(out->zones[0].name),"Turn Zone");
    out->zones[0].range_count=2u;
    out->zones[0].ranges[0]=(ot_pz_range_t){0u,3u};
    out->zones[0].ranges[1]=(ot_pz_range_t){8u,3u};

    out->zones[1].id=(uint16_t)(201u+lane*10u);
    out->zones[1].panel_id=panel_id;
    out->zones[1].role=lane==0u?OT_OM_BRAKE:OT_OM_REVERSE;
    snprintf(out->zones[1].name,sizeof(out->zones[1].name),"Safety Zone");
    out->zones[1].range_count=2u;
    out->zones[1].ranges[0]=(ot_pz_range_t){3u,3u};
    out->zones[1].ranges[1]=(ot_pz_range_t){12u,3u};
}

static void make_controller(ot_pz_controller_t *cfg){
    memset(cfg,0,sizeof(*cfg));
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        make_output(&cfg->outputs[lane],lane);
    assert(ot_pz_valid(cfg));
}

static ot_ble_fx_packet_t make_fx(uint16_t effect,unsigned lane,unsigned zone){
    ot_ble_fx_packet_t request={0};
    request.effect_id=effect;
    request.chain_mask=(uint8_t)(1u<<lane);
    request.zone_mask=(uint8_t)(1u<<zone);
    request.r=210u;request.g=75u;request.b=180u;
    request.bg_r=0u;request.bg_g=0u;request.bg_b=0u;
    request.brightness=24u;
    request.speed=180u;
    request.intensity=190u;
    request.palette_id=effect==OT_FX_EMBERS?2u:0u;
    request.ttl_sec=60u;
    return request;
}

/* Mirrors the CORE per-zone renderer's state-identity and gather/render/scatter
 * path. The source-order contract separately pins this harness to led_task().
 */
static bool render_probe_frame(render_probe_t *probe,
        const ot_pz_controller_t *panel,const ot_fx_zone_bank_t *bank,
        uint32_t now_ms){
    if(!probe||!panel||!bank)return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        const ot_pz_output_t *panel_out=&panel->outputs[lane];
        ot_om_output_t output={0};
        if(!ot_pz_to_fx_view(panel_out,lane,&output))return false;
        ot_layout_t layout;
        if(!ot_om_layout(&output,&layout))return false;

        /* Mirror CORE's stable bank-slot ownership. A generation change is a
         * new command for the current zone at that bank index; unchanged slots
         * remain attached to their previous Panel/Zone identity after compaction. */
        for(unsigned bank_z=0;bank_z<OT_OM_ZONES;bank_z++){
            const ot_fx_zone_slot_t *slot=&bank->zones[lane][bank_z];
            if(!ot_fx_slot_active(slot,now_ms)){
                probe->bank_seen_generation[lane][bank_z]=slot->generation;
                probe->bank_seen_refresh_ms[lane][bank_z]=slot->refreshed_ms;
                probe->bank_owner_zone_id[lane][bank_z]=0u;
                probe->bank_owner_panel_id[lane][bank_z]=0u;
                continue;
            }
            if(probe->bank_seen_generation[lane][bank_z]!=slot->generation||
               probe->bank_seen_refresh_ms[lane][bank_z]!=slot->refreshed_ms||
               probe->bank_owner_zone_id[lane][bank_z]==0u||
               probe->bank_owner_panel_id[lane][bank_z]==0u){
                const uint16_t owner_zone=bank_z<panel_out->zone_count?
                    panel_out->zones[bank_z].id:0u;
                const uint16_t owner_panel=bank_z<panel_out->zone_count?
                    panel_out->zones[bank_z].panel_id:0u;
                for(unsigned other=0;other<OT_OM_ZONES;other++){
                    if(other==bank_z)continue;
                    if(probe->bank_owner_zone_id[lane][other]==owner_zone&&
                       probe->bank_owner_panel_id[lane][other]==owner_panel){
                        probe->bank_owner_zone_id[lane][other]=0u;
                        probe->bank_owner_panel_id[lane][other]=0u;
                    }
                }
                probe->bank_seen_generation[lane][bank_z]=slot->generation;
                probe->bank_seen_refresh_ms[lane][bank_z]=slot->refreshed_ms;
                probe->bank_owner_zone_id[lane][bank_z]=owner_zone;
                probe->bank_owner_panel_id[lane][bank_z]=owner_panel;
            }
        }
        memset(probe->resolved_bank_index[lane],0xff,
               sizeof(probe->resolved_bank_index[lane]));
        for(unsigned zone=0;zone<output.zone_count;zone++){
            const ot_pz_zone_t *zone_cfg=&panel_out->zones[zone];
            for(unsigned bank_z=0;bank_z<OT_OM_ZONES;bank_z++){
                if(probe->bank_owner_zone_id[lane][bank_z]==zone_cfg->id&&
                   probe->bank_owner_panel_id[lane][bank_z]==zone_cfg->panel_id&&
                   ot_fx_slot_active(&bank->zones[lane][bank_z],now_ms)){
                    probe->resolved_bank_index[lane][zone]=(uint8_t)bank_z;break;
                }
            }
        }

        uint8_t zone_map[OT_PZ_MAX_PIXELS];
        uint16_t logical_map[OT_PZ_MAX_PIXELS];
        ot_pz_build_pixel_map(panel_out,zone_map,logical_map);
        uint16_t old_zone_id[OT_OM_ZONES],old_panel_id[OT_OM_ZONES];
        uint32_t old_generation[OT_OM_ZONES];
        bool old_active[OT_OM_ZONES];
        ot_effects_1d_state_t old_state[OT_OM_ZONES];
        memcpy(old_zone_id,probe->prior_zone_id[lane],sizeof(old_zone_id));
        memcpy(old_panel_id,probe->prior_panel_id[lane],sizeof(old_panel_id));
        memcpy(old_generation,probe->prior_generation[lane],sizeof(old_generation));
        memcpy(old_active,probe->prior_active[lane],sizeof(old_active));
        memcpy(old_state,probe->state[lane],sizeof(old_state));

        for(unsigned zone=0;zone<output.zone_count;zone++){
            const unsigned len=output.zones[zone].pixels;
            const ot_pz_zone_t *zone_cfg=&panel_out->zones[zone];
            const uint8_t bank_z=probe->resolved_bank_index[lane][zone];
            const ot_fx_zone_slot_t *slot=bank_z<OT_OM_ZONES?
                &bank->zones[lane][bank_z]:NULL;
            const bool active=len>0u&&zone_cfg->role!=OT_OM_OFF&&slot&&
                ot_fx_slot_active(slot,now_ms);
            int prior_index=-1;
            if(active){
                for(unsigned old_z=0;old_z<OT_OM_ZONES;old_z++){
                    if(old_active[old_z]&&old_zone_id[old_z]==zone_cfg->id&&
                       old_panel_id[old_z]==zone_cfg->panel_id&&
                       old_generation[old_z]==slot->generation){
                        prior_index=(int)old_z;break;
                    }
                }
            }
            const bool same_identity=prior_index>=0;
            if(active&&len<=OT_PZ_MAX_PIXELS&&len<=TEST_PIXELS){
                if(same_identity){
                    if((unsigned)prior_index!=zone)
                        probe->state[lane][zone]=old_state[prior_index];
                }else{
                    ot_effects_1d_reset(&probe->state[lane][zone],
                        0x1234u+lane*97u+zone*19u+slot->generation);
                }

                ot_wled_rgb_t scratch[OT_PZ_MAX_PIXELS];
                memset(scratch,0,(size_t)len*sizeof(scratch[0]));
                for(unsigned px=0;px<panel_out->active_pixels;px++){
                    if(zone_map[px]!=(uint8_t)zone)continue;
                    const unsigned logical=logical_map[px];
                    if(logical>=len)continue;
                    const uint8_t old_z=probe->prior_pixel_zone_map[lane][px];
                    if(same_identity&&old_z<OT_OM_ZONES&&old_active[old_z]&&
                       old_z==(uint8_t)prior_index&&
                       old_zone_id[old_z]==zone_cfg->id&&
                       old_panel_id[old_z]==zone_cfg->panel_id)
                        scratch[logical]=probe->frame[lane][px];
                }

                const ot_ble_fx_packet_t *fx=&slot->request;
                const ot_wled_fx_config_t cfg={
                    .effect_id=fx->effect_id,.speed=fx->speed,
                    .intensity=fx->intensity,.brightness=fx->brightness,
                    .palette_id=fx->palette_id,.frame_time_ms=8u,
                    .foreground={fx->r,fx->g,fx->b},
                    .background={fx->bg_r,fx->bg_g,fx->bg_b},
                    .tertiary={slot->tertiary.r,slot->tertiary.g,slot->tertiary.b},
                    .tertiary_set=slot->tertiary.set,.spark_style=slot->spark.style,
                    .spark_peak=slot->spark.peak,.spark_color=slot->spark.color,
                    .resolved_palette=NULL
                };
                const uint32_t zone_elapsed=now_ms-slot->started_ms;
                if(!ot_effects_1d_render(&cfg,&probe->state[lane][zone],
                                         zone_elapsed,scratch,len))return false;
                memset(probe->logical_frame[lane][zone],0,
                       sizeof(probe->logical_frame[lane][zone]));
                memcpy(probe->logical_frame[lane][zone],scratch,
                       (size_t)len*sizeof(scratch[0]));
                for(unsigned px=0;px<panel_out->active_pixels;px++){
                    if(zone_map[px]!=(uint8_t)zone)continue;
                    const unsigned logical=logical_map[px];
                    if(logical<len)probe->frame[lane][px]=scratch[logical];
                }
                probe->prior_active[lane][zone]=true;
                probe->prior_generation[lane][zone]=slot->generation;
            }else probe->prior_active[lane][zone]=false;
            probe->prior_zone_id[lane][zone]=zone_cfg->id;
            probe->prior_panel_id[lane][zone]=zone_cfg->panel_id;
        }
        for(unsigned zone=output.zone_count;zone<OT_OM_ZONES;zone++)
            probe->prior_active[lane][zone]=false;
        memcpy(probe->prior_pixel_zone_map[lane],zone_map,
               OT_PZ_MAX_PIXELS*sizeof(zone_map[0]));
    }
    return true;
}

static void test_zone_independence_and_discontiguous_ranges(
        ot_pz_controller_t *active,ot_fx_zone_bank_t *bank,
        render_probe_t *probe){
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        const uint32_t started=1000u+lane*40u;
        ot_ble_fx_packet_t turn=make_fx(OT_FX_DIMENSIONAL_RIFT,lane,0u);
        ot_ble_fx_packet_t safety=make_fx(OT_FX_EMBERS,lane,1u);
        assert(ot_fx_bank_apply(bank,&turn,started,false));
        assert(ot_fx_bank_apply(bank,&safety,started,false));
        bank->zones[lane][0].continuous=true;
        bank->zones[lane][1].continuous=true;
    }
    assert(render_probe_frame(probe,active,bank,1100u));
    assert(probe->state[0][0].original_clock_initialized);
    assert(probe->state[1][0].original_clock_initialized);
    assert(probe->state[0][0].original_phase_q32!=
           probe->state[1][0].original_phase_q32);
    assert(probe->state[0][1].initialized);
    assert(probe->state[1][1].initialized);
    assert(probe->state[0][0].particles.seed!=
           probe->state[0][1].particles.seed);
    assert(bank->zones[0][0].generation!=bank->zones[0][1].generation);
    assert(bank->zones[0][0].started_ms==1000u);
    assert(bank->zones[1][0].started_ms==1040u);

    uint8_t zone_map[OT_PZ_MAX_PIXELS];
    uint16_t logical_map[OT_PZ_MAX_PIXELS];
    ot_pz_build_pixel_map(&active->outputs[0],zone_map,logical_map);
    assert(zone_map[0]==0u&&logical_map[0]==0u);
    assert(zone_map[2]==0u&&logical_map[2]==2u);
    assert(zone_map[8]==0u&&logical_map[8]==3u);
    assert(zone_map[10]==0u&&logical_map[10]==5u);
    assert(zone_map[3]==1u&&logical_map[3]==0u);
    assert(zone_map[12]==1u&&logical_map[12]==3u);
    assert(zone_map[6]==0xffu&&zone_map[11]==0xffu);

    for(unsigned i=0;i<3u;i++)
        assert(same_wled_rgb(probe->frame[0][i],
                             probe->logical_frame[0][0][i]));
    for(unsigned i=0;i<3u;i++)
        assert(same_wled_rgb(probe->frame[0][8u+i],
                             probe->logical_frame[0][0][3u+i]));
}

static void test_preview_phase_commit_cancel_and_unaffected_zone(
        ot_pz_controller_t *active,ot_fx_zone_bank_t *bank,
        render_probe_t *probe){
    const uint32_t generation=bank->zones[0][0].generation;
    const uint32_t started=bank->zones[0][0].started_ms;
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,active,0u,TEST_BASE_REVISION,
                               TEST_BASE_REVISION,false,1100u));
    ot_pz_range_update_t move={100u,200u,1u,9u,3u,OT_OM_LEFT};
    assert(ot_pz_preview_apply_updates(&preview,0u,&move,1u,
        TEST_BASE_REVISION,TEST_BASE_REVISION,false,1110u));
    assert(active->outputs[0].zones[0].ranges[1].start==8u);
    assert(preview.candidate.outputs[0].zones[0].ranges[1].start==9u);

    /* Compare independent clones at the same timestamp. Zone 1 is unchanged
     * and must produce byte-identical reference frames during zone 0 editing. */
    render_probe_t reference=*probe;
    render_probe_t previewed=*probe;
    assert(render_probe_frame(&reference,active,bank,1110u));
    assert(render_probe_frame(&previewed,&preview.candidate,bank,1110u));
    const unsigned untouched[]={3u,4u,5u,12u,13u,14u};
    for(size_t i=0;i<sizeof(untouched)/sizeof(untouched[0]);i++)
        assert(same_wled_rgb(reference.frame[0][untouched[i]],
                             previewed.frame[0][untouched[i]]));
    assert(previewed.state[0][0].original_last_ms==110u);
    assert(previewed.state[0][0].original_phase_q32!=
           probe->state[0][0].original_phase_q32);
    assert(bank->zones[0][0].generation==generation);
    assert(bank->zones[0][0].started_ms==started);

    /* Repeated PREVIEW_UPDATE changes only the shadow geometry; the same
     * effect generation and its source-owned phase clock continue. */
    move.start=7u;
    assert(ot_pz_preview_apply_updates(&preview,0u,&move,1u,
        TEST_BASE_REVISION,TEST_BASE_REVISION,false,1118u));
    assert(render_probe_frame(&previewed,&preview.candidate,bank,1118u));
    assert(previewed.state[0][0].original_last_ms==118u);
    const uint32_t phase_before_commit=previewed.state[0][0].original_phase_q32;
    move.start=9u;
    assert(ot_pz_preview_apply_updates(&preview,0u,&move,1u,
        TEST_BASE_REVISION,TEST_BASE_REVISION,false,1126u));
    assert(render_probe_frame(&previewed,&preview.candidate,bank,1126u));
    assert(previewed.state[0][0].original_last_ms==126u);
    assert(previewed.state[0][0].original_phase_q32!=phase_before_commit);

    /* Simulated successful durable promotion. The CORE worker owns the real
     * NVS journal and atomic activation; this host model tests renderer state. */
    preview.committing=true;
    *active=preview.candidate;
    ot_pz_preview_cancel(&preview);
    assert(active->outputs[0].zones[0].ranges[1].start==9u);
    assert(render_probe_frame(&previewed,active,bank,1134u));
    assert(previewed.state[0][0].original_last_ms==134u);
    assert(bank->zones[0][0].generation==generation);
    assert(bank->zones[0][0].started_ms==started);

    /* A later preview is cancelled. The committed geometry and running FX
     * state remain authoritative; cancellation must not restart the effect. */
    const ot_pz_controller_t committed=*active;
    assert(ot_pz_preview_begin(&preview,active,0u,8u,8u,false,1140u));
    move.start=7u;
    assert(ot_pz_preview_apply_updates(&preview,0u,&move,1u,8u,8u,false,1142u));
    assert(preview.candidate.outputs[0].zones[0].ranges[1].start==7u);
    ot_pz_preview_cancel(&preview);
    assert(memcmp(active,&committed,sizeof(committed))==0);
    const uint32_t phase_before_cancel=previewed.state[0][0].original_phase_q32;
    assert(render_probe_frame(&previewed,active,bank,1142u));
    assert(previewed.state[0][0].original_last_ms==142u);
    assert(previewed.state[0][0].original_phase_q32!=phase_before_cancel);
    assert(bank->zones[0][0].generation==generation);
    assert(bank->zones[0][0].started_ms==started);
}

static void test_identity_state_survives_reorder_delete_and_cancel(
        ot_pz_controller_t *active,const ot_fx_zone_bank_t *bank,
        render_probe_t *probe){
    const uint16_t original_zone0=active->outputs[0].zones[0].id;
    const uint16_t original_zone1=active->outputs[0].zones[1].id;
    const uint16_t panel_id=active->outputs[0].zones[0].panel_id;
    assert(original_zone0!=original_zone1);

    /* Reference both continuous effects at the same future timestamp
     * without changing the active geometry. */
    render_probe_t original_order_reference=*probe;
    assert(render_probe_frame(&original_order_reference,active,bank,1158u));

    /* Reorder through the real preview candidate and commit promotion. */
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,active,0u,9u,9u,false,1150u));
    ot_pz_zone_t swap=preview.candidate.outputs[0].zones[0];
    preview.candidate.outputs[0].zones[0]=preview.candidate.outputs[0].zones[1];
    preview.candidate.outputs[0].zones[1]=swap;
    assert(ot_pz_valid(&preview.candidate));
    assert(preview.candidate.outputs[0].zones[0].id==original_zone1);
    assert(preview.candidate.outputs[0].zones[1].id==original_zone0);
    preview.committing=true;
    *active=preview.candidate;
    ot_pz_preview_cancel(&preview);

    assert(render_probe_frame(probe,active,bank,1158u));
    assert(memcmp(&original_order_reference.state[0][1],&probe->state[0][0],
                  sizeof(probe->state[0][0]))==0);
    assert(memcmp(&original_order_reference.state[0][0],&probe->state[0][1],
                  sizeof(probe->state[0][1]))==0);
    assert(probe->resolved_bank_index[0][0]==1u);
    assert(probe->resolved_bank_index[0][1]==0u);

    /* A subsequent reorder is cancelled. The committed mapping and both
     * animation clocks must remain unchanged and continue on the next frame. */
    const ot_pz_controller_t committed=*active;
    assert(ot_pz_preview_begin(&preview,active,0u,10u,10u,false,1160u));
    swap=preview.candidate.outputs[0].zones[0];
    preview.candidate.outputs[0].zones[0]=preview.candidate.outputs[0].zones[1];
    preview.candidate.outputs[0].zones[1]=swap;
    assert(ot_pz_valid(&preview.candidate));
    ot_pz_preview_cancel(&preview);
    assert(memcmp(active,&committed,sizeof(committed))==0);
    render_probe_t cancel_reference=*probe;
    assert(render_probe_frame(&cancel_reference,active,bank,1168u));
    assert(render_probe_frame(probe,active,bank,1168u));
    assert(memcmp(&cancel_reference.state[0][0],&probe->state[0][0],
                  sizeof(probe->state[0][0]))==0);
    assert(memcmp(&cancel_reference.state[0][1],&probe->state[0][1],
                  sizeof(probe->state[0][1]))==0);

    /* Delete the first ordered zone. The other zone shifts from index 1 to 0;
     * its renderer state and its bank-owned effect slot must follow its IDs. */
    render_probe_t delete_reference=*probe;
    assert(render_probe_frame(&delete_reference,active,bank,1176u));
    const ot_pz_range_update_t delete_first={
        .panel_id=panel_id,.zone_id=original_zone1,.range_id=0u,
        .start=0u,.count=0u,
        .role_flags=(uint8_t)(OT_PZ_TX_DELETE_ZONE<<OT_PZ_TX_ACTION_SHIFT)
    };
    assert(ot_pz_preview_begin(&preview,active,0u,10u,10u,false,1170u));
    assert(ot_pz_preview_apply_updates(&preview,0u,&delete_first,1u,
        10u,10u,false,1172u));
    assert(preview.candidate.outputs[0].zone_count==1u);
    assert(preview.candidate.outputs[0].zones[0].id==original_zone0);
    preview.committing=true;
    *active=preview.candidate;
    ot_pz_preview_cancel(&preview);
    assert(render_probe_frame(probe,active,bank,1176u));
    assert(memcmp(&delete_reference.state[0][1],&probe->state[0][0],
                  sizeof(probe->state[0][0]))==0);
    assert(probe->prior_zone_id[0][0]==original_zone0);
    assert(probe->resolved_bank_index[0][0]==0u);
}

static void test_committing_and_sync_fault_admission(
        const ot_pz_controller_t *active){
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,active,0u,8u,8u,false,1200u));
    const ot_pz_range_update_t delayed={100u,200u,0u,1u,2u,OT_OM_LEFT};
    assert(ot_pz_preview_apply_updates(&preview,0u,&delayed,1u,8u,8u,false,1201u));
    preview.committing=true;
    const ot_pz_controller_t shadow_before=preview.candidate;
    assert(!ot_pz_preview_apply_updates(&preview,0u,&delayed,1u,8u,8u,false,1202u));
    assert(memcmp(&preview.candidate,&shadow_before,sizeof(shadow_before))==0);

    struct packet_case {
        uint32_t last_sequence,last_crc,sequence,packet_crc;
    };
    const struct packet_case cases[]={
        {4u,0x1111u,5u,0x2222u}, /* delayed next packet */
        {4u,0x1111u,4u,0x1111u}, /* exact duplicate */
        {4u,0x1111u,6u,0x3333u}, /* forward gap */
        {4u,0x1111u,3u,0x4444u}, /* out of order */
        {4u,0x1111u,4u,0x9999u}  /* same sequence, changed payload */
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
        assert(ot_pz_tx_update_admit(true,false,true,
            cases[i].last_sequence,cases[i].last_crc,
            cases[i].sequence,cases[i].packet_crc)==
            OT_PZ_TX_UPDATE_REJECT_COMMITTING);
    }
    assert(memcmp(&preview.candidate,&shadow_before,sizeof(shadow_before))==0);

    /* A latched synchronization fault rejects further UPDATE admission.
     * Firmware's source-contract test separately verifies the COMMIT guard
     * precedes ot_ble_panel_preview_commit() in the actual BLE handler. */
    assert(ot_pz_tx_update_admit(false,true,true,4u,0x1111u,5u,0x2222u)==
           OT_PZ_TX_UPDATE_REJECT_SYNC_REQUIRED);
    assert(memcmp(&preview.candidate,&shadow_before,sizeof(shadow_before))==0);
    ot_pz_preview_cancel(&preview);
}

static void test_stop_and_physical_priority(ot_fx_zone_bank_t *bank){
    ot_fx_bank_stop_all(bank);
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        for(unsigned zone=0;zone<OT_OM_ZONES;zone++)
            assert(!ot_fx_slot_active(&bank->zones[lane][zone],1300u));

    ot_rgb_t physical={0};
    const ot_rgb_t effect={24u,10u,80u};
    assert(ot_om_trigger_overlay(OT_OM_BRAKE,OT_SIG_BRAKE,true,255u,&physical));
    const ot_rgb_t brake=ot_om_trigger_color(OT_OM_BRAKE,OT_SIG_BRAKE,255u);
    assert(same_rgb(physical,brake));
    assert(!same_rgb(physical,effect));

    assert(ot_om_trigger_overlay(OT_OM_LEFT,
        (uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT),true,255u,&physical));
    const ot_rgb_t left=ot_om_trigger_color(OT_OM_LEFT,
        (uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT),255u);
    assert(same_rgb(physical,left));
    assert(ot_om_trigger_overlay(OT_OM_RIGHT,
        (uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT),true,255u,&physical));
    const ot_rgb_t right=ot_om_trigger_color(OT_OM_RIGHT,
        (uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT),255u);
    assert(same_rgb(physical,right));
    assert(ot_om_trigger_overlay(OT_OM_REVERSE,OT_SIG_REVERSE,true,255u,&physical));
    const ot_rgb_t reverse=ot_om_trigger_color(OT_OM_REVERSE,OT_SIG_REVERSE,255u);
    assert(same_rgb(physical,reverse));
}

int main(void){
    ot_pz_controller_t active;
    make_controller(&active);
    ot_fx_zone_bank_t bank;
    ot_fx_bank_reset(&bank);
    render_probe_t probe={0};

    test_zone_independence_and_discontiguous_ranges(&active,&bank,&probe);
    test_preview_phase_commit_cancel_and_unaffected_zone(&active,&bank,&probe);
    test_identity_state_survives_reorder_delete_and_cancel(&active,&bank,&probe);
    test_committing_and_sync_fault_admission(&active);
    test_stop_and_physical_priority(&bank);

    puts("PASS FX-PZ: independent zone/output state, discontiguous ranges, live preview, stable identity bank/state migration through reorder/delete/cancel, COMMITTING/sync-fault rejection, STOP and physical priority (host simulated; no NVS/RMT/optics)");
    return 0;
}
