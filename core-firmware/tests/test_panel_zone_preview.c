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
    assert(preview.active&&preview.expires_ms==2100u);
    assert(memcmp(&preview.candidate,&committed,sizeof(committed))==0);
    ot_pz_output_t first=resized(&committed.outputs[0],15u);
    assert(ot_pz_preview_update(&preview,0u,&first,7u,7u,false,200u));
    assert(preview.candidate.outputs[0].active_pixels==15u);
    assert(committed.outputs[0].active_pixels==20u);
    assert(preview.expires_ms==2200u);
    ot_pz_output_t latest=resized(&committed.outputs[0],10u);
    assert(ot_pz_preview_update(&preview,0u,&latest,7u,7u,false,250u));
    assert(preview.candidate.outputs[0].active_pixels==10u);
    assert(!ot_pz_preview_update(&preview,0u,&latest,6u,7u,false,260u));
    assert(!ot_pz_preview_update(&preview,1u,&latest,7u,7u,false,260u));
    assert(!ot_pz_preview_update(&preview,0u,&latest,7u,7u,true,260u));
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,300u));
    assert(preview.candidate.outputs[0].active_pixels==10u);
    assert(preview.expires_ms==2300u);
    assert(!ot_pz_preview_begin(&preview,&committed,1u,7u,7u,false,301u));
    assert(!ot_pz_preview_expire(&preview,2299u));
    assert(ot_pz_preview_expire(&preview,2300u));
    assert(!preview.active);
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,
                               UINT32_MAX-1000u));
    assert(!ot_pz_preview_expire(&preview,UINT32_MAX-1u));
    assert(ot_pz_preview_expire(&preview,999u));
    assert(ot_pz_preview_begin(&preview,&committed,0u,7u,7u,false,5000u));
    preview.committing=true;
    assert(!ot_pz_preview_update(&preview,0u,&latest,7u,7u,false,5100u));
    assert(!ot_pz_preview_expire(&preview,8000u));
    ot_pz_preview_cancel(&preview);
    assert(!preview.active&&!preview.committing);
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
    preview_lifecycle();activation_quiet_window();
    printf("Issue126 static object sizes: controller=%zu preview=%zu max_pixels_per_lane=%u wire_bytes=%u\\n",
           sizeof(ot_pz_controller_t),sizeof(ot_pz_preview_t),
           (unsigned)OT_PZ_MAX_PIXELS,(unsigned)OT_PZ_MAX_WIRE_BYTES);
    puts("PASS Issue126 preview lifecycle, revision checks, coalescing, 2s wrap-safe lease and 256-mask activation quiet gate");
    return 0;
}
