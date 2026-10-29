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
    /* Blank universal units never auto-assign parking or brake roles. */
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        assert(configured[o].zone_count==1 && configured[o].active_pixels==1);
        assert(configured[o].zones[0].role==OT_OM_OFF);
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

    /* New software maximum: 623 per independently-configured output,
     * 1,246 across two outputs. These are generic capacity test fixtures,
     * NOT factory-installed vehicle zone assumptions or physical passes. */
    ot_om_output_t candidate[OT_OM_OUTPUTS];ot_om_defaults(candidate);
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        snprintf(candidate[o].name,OT_OM_NAME_LEN,"Prototype OUT %u",o+1u);
        candidate[o].active_pixels=OT_OM_MAX_PIXELS_PER_OUTPUT;
        candidate[o].zone_count=2;
        setzone(&candidate[o].zones[0],"Interior Group",301,OT_OM_DECORATIVE);
        setzone(&candidate[o].zones[1],"Exterior Group",322,OT_OM_LEFT);
        assert(ot_om_valid(&candidate[o]));
        assert(ot_om_section(&candidate[o],622)==1);
        assert(ot_om_section(&candidate[o],623)==-1);
        roundtrip(&candidate[o]); /* two-byte 623 count remains lossless */
        candidate[o].active_pixels=624;
        candidate[o].zones[1].pixels=323;
        assert(!ot_om_valid(&candidate[o]));
        candidate[o].active_pixels=623;
        candidate[o].zones[1].pixels=322;
    }
    assert(candidate[0].active_pixels+candidate[1].active_pixels==1246);

    /* The physically observed GRB color correction stays configurable.
     * Test the actual byte order contract, not a screenshot color. */
    const ot_rgb_t logical={11,22,33};
    const ot_rgb_t swizzled=ot_om_swizzle(logical,OT_OM_GRB);
    const uint8_t wire[3]={swizzled.b,swizzled.r,swizzled.g};
    assert(wire[0]==22 && wire[1]==11 && wire[2]==33);

    /* Inputs IN1..IN8 were observed on GPIO4..11, respectively. Logical
     * mapping and combinations must remain independent of configured zones. */
    const uint8_t physical_masks[]={0,1,2,4,8,16,32,64,128,3,12,14,15,18,31,224,255};
    ot_tm_profile_t trigger;
    ot_tm_defaults(&trigger);
    assert(ot_tm_valid(&trigger));
    for(unsigned i=0;i<sizeof(physical_masks);i++)
        assert(ot_tm_resolve(&trigger,physical_masks[i])==physical_masks[i]);
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
    puts("PASS: neutral factory zones + saved 522/30 bench profiles + generic 623+623 software cap + GRB + masks + brake/hazards/reverse + BLE roundtrip");
    return 0;
}
