/* Independent composition regression. Reuse only the existing fake NVS adapter
 * and its baseline checks; production preview/store/codec functions are linked. */
#define main existing_pair_store_tests
#include "../../core-firmware/tests/test_panel_zone_pair_store.c"
#undef main

static void readback(const ot_pz_controller_t *cfg, const char *dir) {
    for (unsigned lane=0; lane<2; lane++) {
        uint8_t wire[OT_PZ_MAX_WIRE_BYTES]; ot_pz_output_t decoded={0};
        assert(ot_pz_wire_encode(&cfg->outputs[lane],lane,wire));
        assert(ot_pz_wire_decode(wire,lane,&decoded));
        assert(memcmp(&decoded,&cfg->outputs[lane],sizeof(decoded))==0);
        assert(!ot_pz_wire_decode(wire,1u-lane,&decoded));
        if (dir) {
            char path[1024]; snprintf(path,sizeof(path),"%s/lane%u.bin",dir,lane);
            FILE *f=fopen(path,"wb"); assert(f);
            assert(fwrite(wire,1,sizeof(wire),f)==sizeof(wire)); assert(fclose(f)==0);
        }
    }
}
static ot_pz_controller_t recovered(fake_store_t *s, const ot_pz_controller_t *fallback) {
    ot_pz_pair_store_io_t io=make_io(s); ot_pz_controller_t live={0};
    ot_pz_pending_t pending={0}; uint16_t ar[2],sr[2]; bool intent=false;
    uint32_t tx=0; uint8_t reset=0;
    assert(ot_pz_pair_store_recover(&io,&workspace,fallback,&live,&pending,ar,sr,&intent,&tx,&reset));
    if(intent) assert(ot_pz_pair_store_finish(&io,&workspace,tx,NULL,NULL));
    /* Host simulates the quiet-window promotion, not the RTOS activation task. */
    return pending.pending ? pending.candidate : live;
}
static void scenarios(const char *dir) {
    ot_pz_controller_t zero,configured; ot_pz_defaults(&zero);
    assert(zero.outputs[0].active_pixels==0 && zero.outputs[1].active_pixels==0);
    make_runtime(&configured,37,OT_OM_GRB,37);
    configured.outputs[1].active_pixels=144;
    configured.outputs[1].zones[0].ranges[0].count=144;
    ot_pz_preview_t previews[2]={0};
    for(unsigned lane=0;lane<2;lane++) {
        assert(ot_pz_preview_begin(&previews[lane],&zero,lane,1,1,false,100));
        assert(ot_pz_preview_update(&previews[lane],lane,&configured.outputs[lane],1,1,false,110));
    }
    /* Repeated selection/heartbeat must not replace the other lane's draft. */
    for(unsigned i=0;i<100;i++) {
        unsigned lane=i%2;
        assert(ot_pz_preview_heartbeat(&previews[lane],lane,1,200+i));
        assert(previews[0].candidate.outputs[0].active_pixels==37);
        assert(previews[1].candidate.outputs[1].active_pixels==144);
    }
    ot_pz_controller_t first=zero;
    first.outputs[0]=previews[0].candidate.outputs[0];
    snprintf(first.outputs[0].zones[0].name,sizeof(first.outputs[0].zones[0].name),"Renamed accent");
    first.outputs[0].color_order=OT_OM_BGR;
    first.outputs[0].rgb_gain[0]=123; first.outputs[0].white_gain[2]=201;
    first.outputs[0].gamma_enabled=true;
    assert(ot_pz_live_geometry_matches(&previews[0].candidate.outputs[0],&first.outputs[0],0));
    const uint16_t r0[2]={1,1},r1[2]={2,1},r2[2]={2,2};
    fake_store_t saved={0}; ot_pz_pair_store_io_t io=make_io(&saved);
    assert(ot_pz_pair_store_begin(&io,&workspace,&zero,&first,r0,r1,101,0,true,NULL,NULL));
    assert(ot_pz_pair_store_finish(&io,&workspace,101,NULL,NULL));
    ot_pz_controller_t actual=recovered(&saved,&zero);
    assert(memcmp(&actual,&first,sizeof(first))==0); readback(&actual,NULL);
    assert(previews[1].active && previews[1].candidate.outputs[1].active_pixels==144);
    assert(actual.outputs[1].active_pixels==0); /* O2 is still unsaved. */
    ot_pz_controller_t second=actual; second.outputs[1]=previews[1].candidate.outputs[1];
    /* Crash after every durable phase, including lost success after phase 9. */
    for(uint8_t cut=1;cut<=9;cut++) {
        fake_store_t crashed=saved; io=make_io(&crashed);
        phase_state_t phase={.cut_after=cut}; bool ok;
        ok=ot_pz_pair_store_begin(&io,&workspace,&first,&second,r1,r2,102,0,false,phase_hook,&phase);
        if(ok) ok=ot_pz_pair_store_finish(&io,&workspace,102,phase_hook,&phase);
        assert(!ok && phase.last_phase==cut);
        actual=recovered(&crashed,&zero);
        const ot_pz_controller_t *expected=cut<3?&first:&second;
        assert(memcmp(&actual,expected,sizeof(actual))==0); readback(&actual,NULL);
        assert(memcmp(&actual.outputs[0],&first.outputs[0],sizeof(actual.outputs[0]))==0);
    }
    /* Metadata and hardware fields must recover together, never a mixed pair. */
    ot_pz_controller_t metadata=first;
    snprintf(metadata.outputs[0].zones[0].name,sizeof(metadata.outputs[0].zones[0].name),"Atomic metadata");
    metadata.outputs[0].color_order=OT_OM_RGB; metadata.outputs[0].rgb_gain[1]=77;
    const uint16_t rm[2]={3,1};
    for(uint8_t cut=1;cut<=9;cut++) {
        fake_store_t crashed=saved; io=make_io(&crashed);
        phase_state_t phase={.cut_after=cut};
        bool ok=ot_pz_pair_store_begin(&io,&workspace,&first,&metadata,r1,rm,103,0,false,phase_hook,&phase);
        if(ok) ok=ot_pz_pair_store_finish(&io,&workspace,103,phase_hook,&phase);
        assert(!ok); actual=recovered(&crashed,&zero);
        assert(memcmp(&actual,cut<3?&first:&metadata,sizeof(actual))==0);
        readback(&actual,NULL);
    }
    io=make_io(&saved);
    assert(ot_pz_pair_store_begin(&io,&workspace,&first,&second,r1,r2,102,0,false,NULL,NULL));
    assert(ot_pz_pair_store_finish(&io,&workspace,102,NULL,NULL));
    for(unsigned lane=0;lane<2;lane++) ot_pz_preview_cancel(&previews[lane]);
    actual=recovered(&saved,&zero);
    assert(memcmp(&actual,&second,sizeof(actual))==0); readback(&actual,dir);
    puts("PASS independent composition: zero -> 37/144, metadata, isolated Save, 18 crash boundaries, wire readback");
}
int main(int argc,char **argv) {
    existing_pair_store_tests(); scenarios(argc>1?argv[1]:NULL); return 0;
}
