/* All explicitly approved OpenTail originals must remain discoverable and
 * bounded. These are preliminary render-contract checks, not visual signoff.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ot_auto_effects.h"
#include "effects_1d.h"

typedef struct { uint16_t id; const char *name; } expected_t;
static const expected_t originals[]={
    {OT_FX_MATERIALIZE_SPARKS,"Materialize Sparks"},
    {OT_FX_RANDOM_MATERIALIZE,"Random Materialize"},
    {OT_FX_SMOOTH_FADE,"Smooth Fade"},
    {OT_FX_PLASMA_SWEEP,"Plasma Sweep"},
    {OT_FX_SMOOTH_FADE_OUT,"Smooth Fade Out"},
    {OT_FX_DIMENSIONAL_RIFT,"Dimensional Rift"},
    {OT_FX_METEOR_STORM,"Meteor Storm"},
    {OT_FX_HELIX_DRIVE,"Helix Drive"},
    {OT_FX_CRYSTAL_GROWTH,"Crystal Growth"},
    {OT_FX_FUSION_CORE,"Fusion Core"},
    {OT_FX_PORTAL_GATE,"Portal Gate"},
    {OT_FX_TIME_FRACTURE,"Time Fracture"},
    {OT_FX_AURORA_SURGE,"Aurora Surge"},
    {OT_FX_CHROMATIC_SHATTER,"Chromatic Shatter"},
    {OT_FX_VORTEX,"Vortex"},
    {OT_FX_QUANTUM_COLLAPSE,"Quantum Collapse"},
    {OT_FX_REALITY_BREAK,"Reality Break"},
    {OT_FX_EVENT_HORIZON,"Event Horizon"},
    {OT_FX_SINGULARITY_SHUTDOWN,"Singularity Shutdown"}
};
int main(void){
    uint16_t ids[254]={0};
    const size_t total=ot_effects_1d_supported_ids(ids,254);
    assert(total==140u);
    for(size_t i=0;i<total;i++){
        for(size_t j=0;j<i;j++)assert(ids[i]!=ids[j]);
        ot_wled_fx_metadata_t meta={0};
        assert(ot_effects_1d_metadata(ids[i],&meta));
        assert(meta.id==ids[i]);
    }
    static ot_wled_rgb_t frame[525];
    const ot_wled_rgb_t guard={77,88,99};
    ot_wled_fx_config_t c={
        .effect_id=OT_FX_DIMENSIONAL_RIFT,.speed=150u,.intensity=220u,
        .brightness=32u,.palette_id=0u,.frame_time_ms=23u,
        .foreground={220,16,42},.background={0,0,0}
    };
    for(size_t i=0;i<sizeof(originals)/sizeof(originals[0]);i++){
        ot_wled_fx_metadata_t meta={0};
        assert(ot_auto_fx_id(originals[i].id));
        assert(ot_effects_1d_metadata(originals[i].id,&meta));
        assert(strcmp(meta.name,originals[i].name)==0);
        ot_effects_1d_state_t state;
        ot_effects_1d_reset(&state,21u);
        c.effect_id=originals[i].id;
        frame[521]=guard;frame[522]=guard;
        for(uint32_t ms=0;ms<1500;ms+=37){
            assert(ot_effects_1d_render(&c,&state,ms,frame,521u));
            for(size_t p=0;p<521;p++)
                assert(frame[p].r<=32u && frame[p].g<=32u && frame[p].b<=32u);
            assert(memcmp(&frame[521],&guard,sizeof(guard))==0);
            assert(memcmp(&frame[522],&guard,sizeof(guard))==0);
        }
    }
    /* The approved special must end in exact desired color; its shutdown
     * variant must reach true off, including when the effect TTL continues. */
    const uint32_t duration=ot_auto_fx_duration_ms(c.speed);
    c.effect_id=OT_FX_EVENT_HORIZON;
    assert(ot_auto_fx_render(&c,21u,duration,frame,521u));
    for(size_t i=0;i<521;i++){
        assert(frame[i].r==28u && frame[i].g==2u && frame[i].b==5u);
    }
    c.effect_id=OT_FX_SINGULARITY_SHUTDOWN;
    assert(ot_auto_fx_render(&c,21u,duration,frame,521u));
    for(size_t i=0;i<521;i++)
        assert(frame[i].r==0u && frame[i].g==0u && frame[i].b==0u);
    assert(ot_auto_fx_render(&c,21u,duration+5000u,frame,521u));
    for(size_t i=0;i<521;i++)
        assert(frame[i].r==0u && frame[i].g==0u && frame[i].b==0u);
    printf("CORE original effects: %zu names/IDs unique, frame bounds, Event Horizon completion and Singularity true-off passed (host only).\\n",sizeof(originals)/sizeof(originals[0]));
    return 0;
}
