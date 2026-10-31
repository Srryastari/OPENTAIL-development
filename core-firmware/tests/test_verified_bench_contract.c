/* Bench-evidence preservation contract.
 * These are reproducible host assertions matching previously REPORTED physical
 * bench observations; they do NOT constitute a new physical test.
 * Saved configurations are user-owned, not OpenTail factory defaults.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "core_logic.h"
#include "output_manager.h"
#include "ble_config.h"
#include "trigger_manager.h"

static void setzone(ot_om_zone_t *z,const char *name,uint16_t pixels,uint8_t role){
    memset(z,0,sizeof(*z));
    strncpy(z->name,name,OT_OM_NAME_LEN-1);
    z->pixels=pixels;z->role=role;
}
static void roundtrip(const ot_om_output_t *saved){
    uint8_t bytes[OT_CFG_WIRE_BYTES];
    ot_om_output_t decoded={0};
    assert(ot_om_valid(saved));
    ot_cfg_encode(saved,bytes);
    const uint16_t crc=ot_cfg_crc16(bytes,sizeof(bytes));
    assert(ot_cfg_decode(bytes,&decoded));
    assert(!memcmp(&decoded,saved,sizeof(decoded)));
    assert(crc==ot_cfg_crc16(bytes,sizeof(bytes)));
}
int main(void){
    ot_om_output_t configured[OT_OM_OUTPUTS];ot_om_defaults(configured);
    /* Blank universal units are genuinely disabled and never infer a
     * parking/brake role or allocate a fictitious 1-pixel output. */
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        assert(configured[o].zone_count==0 && configured[o].active_pixels==0);
        assert(ot_om_valid(&configured[o]));
        assert(ot_om_section(&configured[o],0)==-1);
    }
    /* Historical GPIO16 bench configuration, supplied as a saved user profile.
     * It must remain valid when factory defaults change. It is not auto-loaded
     * on other vehicles. Exactly four active sections, 522 pixels in total. */
    configured[0].active_pixels=522;configured[0].zone_count=4;
    configured[0].color_order=OT_OM_GRB;
    setzone(&configured[0].zones[0],"User Zone A",118,OT_OM_PARK_BRAKE);
    setzone(&configured[0].zones[1],"User Zone B",109,OT_OM_LEFT);
    setzone(&configured[0].zones[2],"User Zone C",42,OT_OM_REVERSE);
    setzone(&configured[0].zones[3],"User Zone D",253,OT_OM_PARK_BRAKE);
    assert(ot_om_valid(&configured[0]));
    assert(ot_om_section(&configured[0],0)==0);
    assert(ot_om_section(&configured[0],117)==0);
    assert(ot_om_section(&configured[0],118)==1);
    assert(ot_om_section(&configured[0],226)==1);
    assert(ot_om_section(&configured[0],227)==2);
    assert(ot_om_section(&configured[0],268)==2);
    assert(ot_om_section(&configured[0],269)==3);
    assert(ot_om_section(&configured[0],521)==3);
    assert(ot_om_section(&configured[0],522)==-1);
    ot_layout_t layout;
    assert(ot_om_layout(&configured[0],&layout));
    assert(layout.chain_pixels==522 && layout.brightness_cap==32u);
    roundtrip(&configured[0]);

    /* GPIO17 bench configuration is independently configurable; no mirroring
     * of the GPIO16 geometry, color order, names or zone assignments. */
    configured[1].active_pixels=30;configured[1].zone_count=1;
    configured[1].color_order=OT_OM_GRB;
    setzone(&configured[1].zones[0],"User Output 2",30,OT_OM_RIGHT);
    assert(ot_om_valid(&configured[1]));
    assert(ot_om_section(&configured[1],29)==0);
    assert(ot_om_section(&configured[1],30)==-1);
    roundtrip(&configured[1]);
    assert(configured[0].active_pixels==522);
    assert(configured[1].active_pixels==30);

    /* Capacity budget is a board-build capability, not a verified 1,246
     * hardware result. This test uses arbitrary user-owned geometry. */
    const unsigned lane_cap=OT_OM_MAX_PIXELS_PER_OUTPUT;
    assert(lane_cap>522u);
    const unsigned first=lane_cap/2u, second=lane_cap-first;
    ot_om_output_t candidate[OT_OM_OUTPUTS];ot_om_defaults(candidate);
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        snprintf(candidate[o].name,OT_OM_NAME_LEN,"Prototype OUT %u",o+1u);
        candidate[o].active_pixels=(uint16_t)lane_cap;
        candidate[o].zone_count=2;
        setzone(&candidate[o].zones[0],"Interior Group",(uint16_t)first,OT_OM_DECORATIVE);
        setzone(&candidate[o].zones[1],"Exterior Group",(uint16_t)second,OT_OM_LEFT);
        assert(ot_om_valid(&candidate[o]));
        assert(ot_om_section(&candidate[o],lane_cap-1u)==1);
        assert(ot_om_section(&candidate[o],lane_cap)==-1);
        roundtrip(&candidate[o]);
        candidate[o].active_pixels=(uint16_t)(lane_cap+1u);
        candidate[o].zones[1].pixels=(uint16_t)(second+1u);
        assert(!ot_om_valid(&candidate[o]));
        candidate[o].active_pixels=(uint16_t)lane_cap;
        candidate[o].zones[1].pixels=(uint16_t)second;
    }
    assert((unsigned)candidate[0].active_pixels+candidate[1].active_pixels==OT_OM_MAX_TOTAL_PIXELS);

    /* The physically observed GRB color correction stays configurable.
     * Test the actual byte order contract, not a screenshot color. */
    const ot_rgb_t logical={11,22,33};
    const ot_rgb_t swizzled=ot_om_swizzle(logical,OT_OM_GRB);
    const uint8_t wire[3]={swizzled.b,swizzled.r,swizzled.g};
    assert(wire[0]==22 && wire[1]==11 && wire[2]==33);

    /* Inputs IN1..IN8 were observed on GPIO4..11, respectively. Logical
     * mapping and combinations must remain independent of configured zones. */
    /* Each case was previously reported from an older physical bench flash.
     * This list is evidence of input-mask observations, NOT of fresh hardware
     * coverage for the current firmware build or of mechanical debounce. */
    const uint8_t physical_masks[]={0,1,2,4,8,16,32,64,128,
                                    3,5,6,9,10,12,14,15,17,18,30,31,224,255};
    ot_tm_profile_t trigger;
    ot_tm_defaults(&trigger);
    assert(ot_tm_valid(&trigger));
    for(unsigned i=0;i<sizeof(physical_masks);i++)
        assert(ot_tm_resolve(&trigger,physical_masks[i])==physical_masks[i]);
    /* Reuse the earlier observed masks as a strict per-zone overlay contract.
     * Observed input combinations are distinct from newly verified optical
     * colors: the latter are expected software results, not fresh bench passes.
     * No actual vehicle output geometry is assumed by this fixture. */
    enum {BR=1u,LT=2u,RT=4u,RV=8u,A1=16u,A2=32u,A3=64u};
    static const struct {
        uint8_t physical_mask;
        uint8_t expected_show_override;
    } previously_observed[]={
        {0,0}, {1,0}, {2,BR}, {3,BR}, {4,LT},
        {5,LT}, {6,BR|LT}, {8,RT}, {9,RT},
        {10,BR|RT}, {12,LT|RT}, {14,BR|LT|RT},
        {15,BR|LT|RT}, {16,RV}, {17,RV},
        {18,BR|RV}, {30,BR|LT|RT|RV},
        {31,BR|LT|RT|RV}, {32,A1}, {64,A2},
        {128,A3}, {224,A1|A2|A3},
        {255,BR|LT|RT|RV|A1|A2|A3}
    };
    static const struct {
        uint8_t role;
        uint8_t override_flag;
        uint8_t r,g,b; /* expected at the existing bench cap 32 */
    } role_expect[]={
        {OT_OM_PARK_BRAKE,BR,32,0,0},
        {OT_OM_BRAKE,BR,32,0,0},
        {OT_OM_LEFT,LT,32,19,0},
        {OT_OM_RIGHT,RT,32,19,0},
        {OT_OM_REVERSE,RV,32,32,32},
        {OT_OM_AUX1,A1,16,0,32},
        {OT_OM_AUX2,A2,16,0,32},
        {OT_OM_AUX3,A3,16,0,32},
        {OT_OM_OFF,0,0,0,0},
        {OT_OM_DECORATIVE,0,0,0,0}
    };
    for(unsigned t=0;t<sizeof(previously_observed)/sizeof(previously_observed[0]);t++){
        uint8_t physical=previously_observed[t].physical_mask;
        const uint8_t logical=ot_tm_resolve(&trigger,physical);
        assert(logical==physical);
        for(unsigned z=0;z<sizeof(role_expect)/sizeof(role_expect[0]);z++){
            const bool expected=(previously_observed[t].expected_show_override&
                                 role_expect[z].override_flag)!=0;
            ot_rgb_t pixel={11,22,33}; /* sample ongoing-show RGB */
            const bool overlaid=ot_om_trigger_overlay(role_expect[z].role,
                                                      logical,true,32,&pixel);
            assert(overlaid==expected);
            if(expected){
                assert(pixel.r==role_expect[z].r);
                assert(pixel.g==role_expect[z].g);
                assert(pixel.b==role_expect[z].b);
            }else{
                assert(pixel.r==11 && pixel.g==22 && pixel.b==33);
            }
        }
        /* Parking alone must yield to a selected show, but remain the
         * baseline where that section has no effect selected. */
        ot_rgb_t pixel={11,22,33};
        assert(!ot_om_trigger_overlay(OT_OM_PARK,logical,true,32,&pixel));
        assert(pixel.r==11 && pixel.g==22 && pixel.b==33);
        const bool park=(logical&OT_SIG_PARK)!=0;
        assert(ot_om_trigger_overlay(OT_OM_PARK,logical,false,32,&pixel)==park);
        if(park)assert(pixel.r==7 && pixel.g==0 && pixel.b==0);
    }
    /* Replay a locked physical combination sequence with a persistent show.
     * The mask sequence is an illustrative source-side replay; it is not a
     * claim that those transitions or effect phases were measured anew. */
    static const uint8_t replay[]={0,1,15,31,15,1,0};
    for(unsigned t=0;t<sizeof(replay);t++){
        const uint8_t logical=ot_tm_resolve(&trigger,replay[t]);
        ot_rgb_t base={11,22,33},left=base,right=base,reverse=base;
        const bool braking=(logical&OT_SIG_BRAKE)!=0;
        const bool hazards=(logical&(OT_SIG_LEFT|OT_SIG_RIGHT))==
                           (OT_SIG_LEFT|OT_SIG_RIGHT);
        assert(ot_om_trigger_overlay(OT_OM_PARK_BRAKE,logical,true,32,&base)==braking);
        assert(ot_om_trigger_overlay(OT_OM_LEFT,logical,true,32,&left)==hazards);
        assert(ot_om_trigger_overlay(OT_OM_RIGHT,logical,true,32,&right)==hazards);
        assert(ot_om_trigger_overlay(OT_OM_REVERSE,logical,true,32,&reverse)==
               ((logical&OT_SIG_REVERSE)!=0));
        if(!braking)assert(base.r==11 && base.g==22 && base.b==33);
        if(!hazards){
            assert(left.r==11 && left.g==22 && left.b==33);
            assert(right.r==11 && right.g==22 && right.b==33);
        }
        if(!(logical&OT_SIG_REVERSE))
            assert(reverse.r==11 && reverse.g==22 && reverse.b==33);
    }
    /* Explicit user remapping changes only logical meaning. */
    trigger.function[0]=OT_TM_DISABLED;
    trigger.function[5]=OT_TM_REVERSE;
    assert(ot_tm_resolve(&trigger,1u)==0);
    assert(ot_tm_resolve(&trigger,1u<<5)==OT_SIG_REVERSE);

    /* One mask can drive brake + hazards simultaneously without applying a
     * universal five-zone template. Nonmatching roles do not interrupt show. */
    ot_rgb_t c={0,0,0};
    const uint8_t brake_hazards=OT_SIG_BRAKE|OT_SIG_LEFT|OT_SIG_RIGHT;
    assert(brake_hazards==14);
    assert(ot_om_trigger_overlay(OT_OM_PARK_BRAKE,OT_SIG_PARK|OT_SIG_BRAKE,true,32,&c));
    assert(c.r==32 && c.g==0 && c.b==0); /* Brake outranks Park */
    assert(ot_om_trigger_overlay(OT_OM_LEFT,brake_hazards,true,32,&c));
    assert(c.r==32 && c.g>0 && c.b==0);
    assert(ot_om_trigger_overlay(OT_OM_RIGHT,brake_hazards,true,32,&c));
    assert(!ot_om_trigger_overlay(OT_OM_REVERSE,brake_hazards,true,32,&c));
    assert(ot_om_trigger_overlay(OT_OM_REVERSE,OT_SIG_REVERSE,true,32,&c));
    assert(c.r==32 && c.g==32 && c.b==32);
    assert(!ot_om_trigger_overlay(OT_OM_PARK_BRAKE,OT_SIG_PARK,true,32,&c));
    assert(ot_om_trigger_overlay(OT_OM_PARK_BRAKE,OT_SIG_PARK,false,32,&c));

    /* The separately verified persistent output-1 name/value profile can
     * still roundtrip. Reboot persistence is a physical/NVS test, not proven
     * by these codec assertions. */
    ot_om_output_t remembered=configured[1];
    strcpy(remembered.name,"Front DRL");
    remembered.color_order=OT_OM_BRG;
    roundtrip(&remembered);
    puts("PASS: neutral factory zones + saved 522/30 bench profiles + board-derived memory budget + GRB + masks + brake/hazards/reverse + BLE roundtrip");
    return 0;
}
