#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "panel_zone.h"

static void make_config(ot_pz_controller_t *cfg){
    ot_pz_defaults(cfg);
    ot_pz_output_t *o=&cfg->outputs[0];
    (void)snprintf(o->name,sizeof(o->name),"Rear output");
    o->active_pixels=20u;o->panel_count=1u;o->zone_count=1u;
    o->panels[0].id=101u;o->panels[0].lane=0u;
    (void)snprintf(o->panels[0].name,sizeof(o->panels[0].name),"Rear panel");
    o->panels[0].zone_count=1u;
    o->zones[0].id=201u;o->zones[0].panel_id=101u;
    o->zones[0].role=OT_OM_DECORATIVE;
    (void)snprintf(o->zones[0].name,sizeof(o->zones[0].name),"Full strip");
    o->zones[0].range_count=1u;
    o->zones[0].ranges[0]=(ot_pz_range_t){0u,20u};
    assert(ot_pz_valid(cfg));
}
static ot_pz_output_t resized(const ot_pz_output_t *source,uint16_t pixels){
    ot_pz_output_t next=*source;
    next.active_pixels=pixels;
    next.zones[0].ranges[0]=(ot_pz_range_t){0u,pixels};
    assert(ot_pz_output_valid(&next,0u));
    return next;
}
static void preview_lifecycle(void){
    ot_pz_controller_t committed;make_config(&committed);
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,100u));
    assert(preview.active&&preview.expires_ms==5100u);
    assert(memcmp(&preview.candidate,&committed,sizeof(committed))==0);
    ot_pz_output_t first=resized(&committed.outputs[0],15u);
    assert(ot_pz_preview_update(&preview,0u,&first,7u,7u,false,200u));
    assert(preview.candidate.outputs[0].active_pixels==15u);
    assert(committed.outputs[0].active_pixels==20u);
    assert(preview.expires_ms==5100u);
    ot_pz_output_t latest=resized(&committed.outputs[0],10u);
    assert(ot_pz_preview_update(&preview,0u,&latest,7u,7u,false,250u));
    assert(preview.candidate.outputs[0].active_pixels==10u);
    assert(preview.expires_ms==5100u);
    assert(ot_pz_preview_heartbeat(&preview,0u,7u,260u));
    assert(preview.expires_ms==5260u);
    assert(!ot_pz_preview_update(&preview,0u,&latest,6u,7u,false,260u));
    assert(!ot_pz_preview_update(&preview,1u,&latest,7u,7u,false,260u));
    assert(!ot_pz_preview_update(&preview,0u,&latest,7u,7u,true,260u));
    assert(!ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,300u));
    assert(preview.candidate.outputs[0].active_pixels==10u);
    assert(preview.expires_ms==5260u);
    assert(!ot_pz_preview_begin(&preview,&committed,1u,7u,7u,false,301u));
    assert(!ot_pz_preview_expire(&preview,5259u));
    assert(ot_pz_preview_expire(&preview,5260u));
    assert(!preview.active);
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,
                               UINT32_MAX-1000u));
    assert(!ot_pz_preview_expire(&preview,UINT32_MAX-1u));
    assert(ot_pz_preview_expire(&preview,3999u));
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,5000u));
    preview.committing=true;
    assert(!ot_pz_preview_update(&preview,0u,&latest,7u,7u,false,5100u));
    assert(!ot_pz_preview_expire(&preview,8000u));
    ot_pz_preview_cancel(&preview);
    assert(!preview.active&&!preview.committing);
}

static void range_transaction(void){
    ot_pz_controller_t committed;make_config(&committed);
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,100u));
    const ot_pz_range_update_t expand[]={
        {101u,201u,0u,0u,10u,(uint8_t)OT_OM_DECORATIVE},
        {101u,201u,1u,10u,10u,(uint8_t)OT_OM_DECORATIVE}
    };
    assert(ot_pz_preview_apply_updates(&preview,0u,expand,2u,7u,7u,false,200u));
    assert(preview.candidate.outputs[0].zones[0].range_count==2u);
    assert(ot_pz_zone_pixel_count(&preview.candidate.outputs[0].zones[0])==20u);
    const ot_pz_range_update_t overlap={101u,201u,1u,9u,5u,(uint8_t)OT_OM_DECORATIVE};
    assert(!ot_pz_preview_apply_updates(&preview,0u,&overlap,1u,7u,7u,false,250u));
    assert(preview.candidate.outputs[0].zones[0].ranges[1].start==10u);
    const ot_pz_range_update_t add_zone={101u,202u,0u,20u,0u,
        (uint8_t)(OT_PZ_TX_DELETE_ZONE<<OT_PZ_TX_ACTION_SHIFT)};
    assert(!ot_pz_preview_apply_updates(&preview,0u,&add_zone,1u,7u,7u,false,260u));
    const ot_pz_range_update_t remove_range={101u,201u,1u,0u,0u,
        (uint8_t)(OT_PZ_TX_DELETE_RANGE<<OT_PZ_TX_ACTION_SHIFT)};
    assert(ot_pz_preview_apply_updates(&preview,0u,&remove_range,1u,7u,7u,false,270u));
    assert(preview.candidate.outputs[0].zones[0].range_count==1u);
    const ot_pz_range_update_t delete_zone={101u,201u,0u,0u,0u,
        (uint8_t)(OT_PZ_TX_DELETE_ZONE<<OT_PZ_TX_ACTION_SHIFT)};
    assert(ot_pz_preview_apply_updates(&preview,0u,&delete_zone,1u,7u,7u,false,280u));
    assert(preview.candidate.outputs[0].zone_count==0u);
    assert(preview.candidate.outputs[0].panel_count==0u);
}
/* Fresh zero-LED lane can stage a real 100-LED physical layout in RAM
 * without mutating committed configuration or requiring a trigger role. */
static void fresh_zero_to_100_unsaved_preview(void){
    ot_pz_controller_t committed;
    ot_pz_defaults(&committed);
    assert(ot_pz_valid(&committed));
    assert(committed.outputs[0].active_pixels==0u);
    assert(committed.outputs[0].panel_count==0u);
    assert(committed.outputs[0].zone_count==0u);
    ot_pz_preview_t preview={0};
    assert(ot_pz_preview_begin(&preview,&committed,0u,1u,1u,false,100u));
    preview.candidate.outputs[0].active_pixels=100u;
    assert(ot_pz_valid(&preview.candidate));
    const ot_pz_range_update_t zone={1u,1u,0u,0u,100u,OT_OM_OFF};
    assert(ot_pz_preview_apply_updates(&preview,0u,&zone,1u,1u,1u,false,120u));
    assert(preview.candidate.outputs[0].active_pixels==100u);
    assert(preview.candidate.outputs[0].panel_count==1u);
    assert(preview.candidate.outputs[0].zone_count==1u);
    assert(preview.candidate.outputs[0].zones[0].role==OT_OM_OFF);
    assert(committed.outputs[0].active_pixels==0u);
    assert(committed.outputs[0].zone_count==0u);
    assert(committed.outputs[1].active_pixels==0u);
    ot_pz_preview_cancel(&preview);
    assert(!preview.active);
    assert(committed.outputs[0].active_pixels==0u);
}

static void activation_quiet_window(void){
    for(unsigned mask=0;mask<256u;mask++){
        const bool quiet=ot_pz_activation_inputs_quiet(
            (uint8_t)mask,0u,4u,4u,99u,99u);
        assert(quiet==(mask==0u));
        const bool debounced=ot_pz_activation_inputs_quiet(
            0u,(uint8_t)mask,4u,4u,99u,99u);
        assert(debounced==(mask==0u));
    }
    assert(!ot_pz_activation_inputs_quiet(0u,0u,3u,4u,99u,99u));
    assert(!ot_pz_activation_inputs_quiet(0u,0u,4u,4u,100u,99u));
}
int main(void){
    preview_lifecycle();range_transaction();fresh_zero_to_100_unsaved_preview();activation_quiet_window();
    printf("Issue126 static object sizes: controller=%zu preview=%zu max_pixels_per_lane=%u wire_bytes=%u\\n",
           sizeof(ot_pz_controller_t),sizeof(ot_pz_preview_t),
           (unsigned)OT_PZ_MAX_PIXELS,(unsigned)OT_PZ_MAX_WIRE_BYTES);
    puts("PASS Issue126 preview lifecycle, revision checks, coalescing, 5s heartbeat-owned wrap-safe lease and 256-mask activation quiet gate");
    return 0;
}
