/* Previously reported OpenTail trigger bench scenarios, reconstructed as
 * host regressions. No assertion here is a fresh electrical or optical test.
 * Output sizes/zone names are deliberately synthetic, not vehicle defaults. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "trigger_manager.h"
#include "output_manager.h"
#include "arbitration_logic.h"

static void expect_rgb(ot_rgb_t actual,unsigned r,unsigned g,unsigned b){
    assert(actual.r==r && actual.g==g && actual.b==b);
}
static void zone(ot_om_zone_t *z,const char *name,unsigned n,uint8_t role){
    memset(z,0,sizeof(*z));
    snprintf(z->name,OT_OM_NAME_LEN,"%s",name);
    z->pixels=(uint16_t)n;
    z->role=role;
}
static void test_independent_outputs_and_selected_zones(void){
    /* The two outputs have deliberately different geometry and turn roles.
     * Reproduces the earlier saved-zone Brake/Rainbow behavior without
     * importing the prior car's 522/30 geometry into production defaults. */
    assert(OT_OM_OUTPUTS>=2);
    ot_om_output_t outputs[OT_OM_OUTPUTS];
    ot_om_defaults(outputs);
    outputs[0].active_pixels=46;
    outputs[0].zone_count=3;
    zone(&outputs[0].zones[0],"Brake A",17,OT_OM_PARK_BRAKE);
    zone(&outputs[0].zones[1],"Left A",7,OT_OM_LEFT);
    zone(&outputs[0].zones[2],"Reverse A",22,OT_OM_REVERSE);
    outputs[1].active_pixels=41;
    outputs[1].zone_count=3;
    zone(&outputs[1].zones[0],"Brake B",19,OT_OM_PARK_BRAKE);
    zone(&outputs[1].zones[1],"Right B",9,OT_OM_RIGHT);
    zone(&outputs[1].zones[2],"Aux B",13,OT_OM_AUX1);
    assert(ot_om_valid(&outputs[0]) && ot_om_valid(&outputs[1]));
    assert(ot_om_section(&outputs[0],16)==0);
    assert(ot_om_section(&outputs[0],17)==1);
    assert(ot_om_section(&outputs[0],23)==1);
    assert(ot_om_section(&outputs[0],24)==2);
    assert(ot_om_section(&outputs[1],18)==0);
    assert(ot_om_section(&outputs[1],19)==1);
    assert(ot_om_section(&outputs[1],27)==1);
    assert(ot_om_section(&outputs[1],28)==2);

    /* Show selected on Brake A, Reverse A, Right B and Aux B only.
     * Brake B remains a physical-trigger zone with no selected effect. */
    const bool selected[2][3]={{true,false,true},{false,true,true}};
    const uint8_t signals[]={0,1,2,3,12,14,15,16,18,30,31,32,224,255};
    for(unsigned t=0;t<sizeof(signals);t++){
        const uint8_t mask=signals[t];
        const ot_arbitration_t arb=ot_arbitrate(mask,false,true);
        assert(arb.show_requested && !arb.show_suppressed);
        for(unsigned out=0;out<2;out++){
            for(unsigned zi=0;zi<3;zi++){
                const ot_om_zone_t *z=&outputs[out].zones[zi];
                const bool show_here=selected[out][zi];
                /* The caller owns the show frame; overlay must only touch
                 * a matching role, never clear an unrelated effect. */
                ot_rgb_t c=show_here?(ot_rgb_t){11,22,33}:(ot_rgb_t){0,0,0};
                const uint8_t active=show_here?(uint8_t)(mask&~OT_SIG_PARK):mask;
                bool expected_hit=false;
                switch(z->role){
                    case OT_OM_PARK_BRAKE:
                        expected_hit=(active&(OT_SIG_BRAKE|OT_SIG_PARK))!=0;
                        break;
                    case OT_OM_LEFT:expected_hit=(active&OT_SIG_LEFT)!=0;break;
                    case OT_OM_RIGHT:expected_hit=(active&OT_SIG_RIGHT)!=0;break;
                    case OT_OM_REVERSE:expected_hit=(active&OT_SIG_REVERSE)!=0;break;
                    case OT_OM_AUX1:expected_hit=(active&(1u<<5))!=0;break;
                    default:assert(false);
                }
                assert(ot_om_trigger_overlay(z->role,mask,show_here,32,&c)==expected_hit);
                if(!expected_hit){
                    if(show_here)expect_rgb(c,11,22,33);
                    else expect_rgb(c,0,0,0);
                }else{
                    switch(z->role){
                        case OT_OM_PARK_BRAKE:
                            if(mask&OT_SIG_BRAKE)expect_rgb(c,32,0,0);
                            else expect_rgb(c,7,0,0);
                            break;
                        case OT_OM_LEFT:case OT_OM_RIGHT:expect_rgb(c,32,19,0);break;
                        case OT_OM_REVERSE:expect_rgb(c,32,32,32);break;
                        case OT_OM_AUX1:expect_rgb(c,16,0,32);break;
                        default:assert(false);
                    }
                }
            }
        }
    }
    /* Prior saved-zone Brake test: Brake A has an effect underneath;
     * Brake B has no selected effect; both must be red when IN2 is active. */
    for(unsigned out=0;out<2;out++){
        ot_rgb_t c={11,22,33};
        assert(ot_om_trigger_overlay(outputs[out].zones[0].role,OT_SIG_BRAKE,
                                     selected[out][0],32,&c));
        expect_rgb(c,32,0,0);
    }
    /* Previously user-confirmed simulated Brake+Hazards diagnostics:
     * mode=vehicle, hazards=true; both brake roles red, both turn roles
     * amber, Reverse off. This tests the logical overlay, not optical LEDs. */
    const uint8_t brake_hazards=(uint8_t)(OT_SIG_BRAKE|OT_SIG_LEFT|OT_SIG_RIGHT);
    const ot_arbitration_t diag=ot_arbitrate(brake_hazards,false,false);
    assert(diag.mode==OT_ARB_VEHICLE && diag.hazards && !diag.show_requested);
    for(unsigned out=0;out<2;out++){
        ot_rgb_t tail={0,0,0},turn={0,0,0};
        assert(ot_om_trigger_overlay(outputs[out].zones[0].role,
                                     brake_hazards,false,32,&tail));
        assert(ot_om_trigger_overlay(outputs[out].zones[1].role,
                                     brake_hazards,false,32,&turn));
        expect_rgb(tail,32,0,0);
        expect_rgb(turn,32,19,0);
    }
    ot_rgb_t reverse_off={0,0,0};
    assert(!ot_om_trigger_overlay(OT_OM_REVERSE,brake_hazards,false,32,&reverse_off));
    expect_rgb(reverse_off,0,0,0);

    /* Previously user-confirmed Reverse simulation: both independently
     * configured reverse sections were active; turn sections stayed dark;
     * after simulated expiry, normal rendering restored. */
    outputs[1].zones[2].role=OT_OM_REVERSE; /* temporary TEST profile only */
    assert(ot_om_valid(&outputs[1]));
    for(unsigned out=0;out<2;out++){
        ot_rgb_t reverse={0,0,0},turn={0,0,0};
        assert(ot_om_trigger_overlay(outputs[out].zones[2].role,
                                     OT_SIG_REVERSE,false,32,&reverse));
        expect_rgb(reverse,32,32,32);
        assert(!ot_om_trigger_overlay(outputs[out].zones[1].role,
                                      OT_SIG_REVERSE,false,32,&turn));
        expect_rgb(turn,0,0,0);
        reverse=(ot_rgb_t){11,22,33}; /* effect resumes underneath on release */
        assert(!ot_om_trigger_overlay(outputs[out].zones[2].role,
                                      0,true,32,&reverse));
        expect_rgb(reverse,11,22,33);
    }
    outputs[1].zones[2].role=OT_OM_AUX1;
    assert(ot_om_valid(&outputs[1]));

    /* Releasing Brake leaves the effect on A and black on unselected B. */
    ot_rgb_t a={11,22,33},b={0,0,0};
    assert(!ot_om_trigger_overlay(OT_OM_PARK_BRAKE,0,true,32,&a));
    assert(!ot_om_trigger_overlay(OT_OM_PARK_BRAKE,0,false,32,&b));
    expect_rgb(a,11,22,33);
    expect_rgb(b,0,0,0);
}
static void test_every_input_reassignment(void){
    /* Each of 8 pins can independently map to every supported function,
     * including Disabled. Test every one of the 256 physical combinations,
     * not only identity mapping or first five vehicle functions. */
    for(unsigned pin=0;pin<OT_IN_COUNT;pin++){
        const uint8_t own_bit=(uint8_t)(1u<<pin);
        for(unsigned function=OT_TM_DISABLED;function<=OT_TM_AUX3;function++){
            ot_tm_profile_t p;
            ot_tm_defaults(&p);
            p.function[pin]=(uint8_t)function;
            assert(ot_tm_valid(&p));
            for(unsigned raw=0;raw<256u;raw++){
                uint8_t expected=(uint8_t)((uint8_t)raw&~own_bit);
                if((raw&own_bit) && function!=OT_TM_DISABLED)
                    expected|=(uint8_t)(1u<<(function-1u));
                assert(ot_tm_resolve(&p,(uint8_t)raw)==expected);
            }
        }
    }
}
static void test_duplicate_disconnected_and_simulated_inputs(void){
    ot_tm_profile_t p;
    ot_tm_defaults(&p);
    p.function[0]=OT_TM_BRAKE;
    p.function[7]=OT_TM_BRAKE;
    p.function[1]=OT_TM_DISABLED;
    /* Releasing either brake source must not clear the other. */
    assert(ot_tm_resolve(&p,0x81u)==OT_SIG_BRAKE);
    assert(ot_tm_resolve(&p,0x80u)==OT_SIG_BRAKE);
    assert(ot_tm_resolve(&p,0x01u)==OT_SIG_BRAKE);
    assert(ot_tm_resolve(&p,0x02u)==0);
    assert(ot_tm_resolve(&p,0x00u)==0);

    const uint8_t real=ot_tm_resolve(&p,0x81u);
    const uint8_t simulated=ot_tm_signal_mask_of("reverse");
    assert(simulated==OT_SIG_REVERSE);
    const uint8_t effective=(uint8_t)(real|simulated);
    assert(effective==(OT_SIG_BRAKE|OT_SIG_REVERSE));
    assert((uint8_t)(effective&~simulated)==real);
    assert(ot_tm_resolve(&p,0x81u)==real); /* simulated clear leaves physical intact */
    ot_rgb_t c={0,0,0};
    assert(ot_om_trigger_overlay(OT_OM_REVERSE,effective,true,32,&c));
    expect_rgb(c,32,32,32);
    assert(!ot_om_trigger_overlay(OT_OM_REVERSE,real,true,32,&c));

    /* All eight pins may intentionally be assigned one logical function. */
    for(unsigned function=OT_TM_DISABLED;function<=OT_TM_AUX3;function++){
        for(unsigned i=0;i<OT_IN_COUNT;i++)p.function[i]=(uint8_t)function;
        const uint8_t expected=function?
            (uint8_t)(1u<<(function-1u)):0u;
        for(unsigned raw=0;raw<256u;raw++)
            assert(ot_tm_resolve(&p,(uint8_t)raw)==(raw?expected:0u));
    }
}
int main(void){
    test_independent_outputs_and_selected_zones();
    test_every_input_reassignment();
    test_duplicate_disconnected_and_simulated_inputs();
    puts("Trigger historical profiles: independent outputs, selected-zone priority, 8x9 remaps, duplicate/disabled inputs and simulated coexistence passed");
    return 0;
}
