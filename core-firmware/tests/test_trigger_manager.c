#include "trigger_manager.h"
#include "output_manager.h"
#include "arbitration_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Exercise the same normalized input levels the independent 10ms sampler
 * feeds to ot_debounce(): staggered transitions and opto-contact chatter must
 * not erase unrelated active inputs or create spurious hazard/brake masks. */
static void test_sampler_transitions(void){
    ot_inputs_t debounce={0};
    bool levels[OT_IN_COUNT]={0};
    levels[1]=true;levels[2]=true;levels[3]=true; /* Brake + both turns */
    for(unsigned n=0;n<3;n++)assert(ot_debounce(&debounce,levels,4u)==0u);
    assert(ot_debounce(&debounce,levels,4u)==14u);
    for(unsigned n=0;n<10;n++){
        levels[2]=false;assert(ot_debounce(&debounce,levels,4u)==14u);
        levels[2]=true;assert(ot_debounce(&debounce,levels,4u)==14u);
    }
    levels[2]=false;
    for(unsigned n=0;n<3;n++)assert(ot_debounce(&debounce,levels,4u)==14u);
    assert(ot_debounce(&debounce,levels,4u)==10u);
    levels[4]=true; /* Reverse and brake/right can coexist. */
    for(unsigned n=0;n<3;n++)assert(ot_debounce(&debounce,levels,4u)==10u);
    assert(ot_debounce(&debounce,levels,4u)==26u);
    for(unsigned i=0;i<OT_IN_COUNT;i++)levels[i]=false;
    for(unsigned n=0;n<3;n++)assert(ot_debounce(&debounce,levels,4u)==26u);
    assert(ot_debounce(&debounce,levels,4u)==0u);
}

int main(void){
    test_sampler_transitions();
    ot_tm_profile_t p={0};
    ot_tm_defaults(&p);
    assert(ot_tm_valid(&p));
    const uint8_t masks[]={OT_SIG_PARK,OT_SIG_BRAKE,OT_SIG_LEFT,OT_SIG_RIGHT,OT_SIG_REVERSE,1u<<5,1u<<6,1u<<7};
    const char *names[]={"park","brake","left","right","reverse","aux1","aux2","aux3"};
    for(int i=0;i<OT_IN_COUNT;i++){
        assert(!strcmp(ot_tm_name(p.function[i]),names[i]));
        assert(ot_tm_from_name(names[i])==(int)p.function[i]);
        assert(ot_tm_signal_mask_of(names[i])==masks[i]);
        assert(ot_tm_resolve(&p,(uint8_t)(1u<<i))==masks[i]);
    }
    for(int i=0;i<256;i++)assert(ot_tm_resolve(&p,(uint8_t)i)==(uint8_t)i);
    /* Remapping physical IN1 to brake changes function but not raw-pin reporting. */
    p.function[0]=OT_TM_BRAKE;
    assert(ot_tm_resolve(&p,1u)==OT_SIG_BRAKE);
    assert(ot_tm_resolve(&p,3u)==OT_SIG_BRAKE); /* Duplicate inputs OR cleanly. */
    assert(ot_tm_resolve(&p,0u)==0);
    /* Disabled input must not suppress decorative shows. */
    p.function[1]=OT_TM_DISABLED;
    assert(ot_tm_resolve(&p,2u)==0);
    assert(ot_arbitrate(ot_tm_resolve(&p,2u),false,true).mode==OT_ARB_SHOW);
    /* Concurrency preserves brake over parking; hazards are both turn bits. */
    ot_tm_defaults(&p);
    uint8_t m=ot_tm_resolve(&p,OT_SIG_PARK|OT_SIG_BRAKE|OT_SIG_LEFT|OT_SIG_RIGHT);
    ot_arbitration_t a=ot_arbitrate(m,false,true);
    assert(a.mode==OT_ARB_SHOW&&a.hazards&&a.brake_over_parking&&!a.show_suppressed);
    ot_rgb_t brake=ot_om_trigger_color(OT_OM_PARK_BRAKE,m,32);
    assert(brake.r==32&&brake.g==0&&brake.b==0);
    /* Zone roles follow logical functions regardless of which input provides them. */
    p.function[0]=OT_TM_AUX1;
    p.function[5]=OT_TM_REVERSE;
    m=ot_tm_resolve(&p,1u|1u<<5);
    assert((m&(1u<<5)) && (m&OT_SIG_REVERSE));
    ot_rgb_t aux=ot_om_trigger_color(OT_OM_AUX1,m,32);
    ot_rgb_t reverse=ot_om_trigger_color(OT_OM_REVERSE,m,32);
    assert((aux.r|aux.g|aux.b)!=0&&(reverse.r|reverse.g|reverse.b)!=0);
    /* The diagnostic simulation parser accepts every physical-input logical
     * function, not only the first five vehicle functions. A simulated AUX
     * signal may coexist with a remapped physical reverse input. */
    uint8_t simulated=(uint8_t)(ot_tm_signal_mask_of("aux1")|
        ot_tm_signal_mask_of("aux2")|ot_tm_signal_mask_of("aux3"));
    assert(simulated==((1u<<5)|(1u<<6)|(1u<<7)));
    ot_tm_defaults(&p);
    p.function[0]=OT_TM_REVERSE;
    uint8_t physical=ot_tm_resolve(&p,1u);
    assert(physical==OT_SIG_REVERSE);
    assert(ot_om_trigger_overlay(OT_OM_REVERSE,(uint8_t)(physical|simulated),true,32,&reverse));
    assert(ot_om_trigger_overlay(OT_OM_AUX3,(uint8_t)(physical|simulated),true,32,&aux));
    assert(ot_tm_signal_mask_of("disabled")==0);
    assert(ot_tm_signal_mask_of("park_brake")==0);
    assert(ot_tm_signal_mask_of("LEFT")==0);
    assert(ot_tm_signal_mask_of("")==0);
    assert(ot_tm_signal_mask_of(NULL)==0);
    /* One-packet BLE mapping update is completely validated before any profile
     * or expected revision is touched; wire values refer to functions, not GPIOs. */
    uint8_t wire[OT_TM_BLE_WRITE_BYTES]={OT_TM_BLE_VERSION,0x34,0x12,
        OT_TM_PARK,OT_TM_BRAKE,OT_TM_LEFT,OT_TM_RIGHT,
        OT_TM_REVERSE,OT_TM_AUX1,OT_TM_AUX2,OT_TM_DISABLED};
    ot_tm_profile_t decoded={0};
    uint16_t expected=0;
    assert(ot_tm_ble_decode(wire,sizeof(wire),&expected,&decoded));
    assert(expected==0x1234u && decoded.function[7]==OT_TM_DISABLED);
    assert(decoded.function[0]==OT_TM_PARK && decoded.function[6]==OT_TM_AUX2);
    decoded.function[0]=OT_TM_AUX3;
    expected=55;
    wire[3]=255;
    assert(!ot_tm_ble_decode(wire,sizeof(wire),&expected,&decoded));
    assert(expected==55 && decoded.function[0]==OT_TM_AUX3);
    wire[3]=OT_TM_PARK;
    assert(!ot_tm_ble_decode(wire,sizeof(wire)-1,&expected,&decoded));
    wire[0]=2;
    assert(!ot_tm_ble_decode(wire,sizeof(wire),&expected,&decoded));
    wire[0]=OT_TM_BLE_VERSION;
    assert(!ot_tm_ble_decode(NULL,sizeof(wire),&expected,&decoded));
    assert(!ot_tm_ble_decode(wire,sizeof(wire),NULL,&decoded));
    assert(!ot_tm_ble_decode(wire,sizeof(wire),&expected,NULL));
    /* Exhaustive arbitration regression: all 256 simultaneous logical input
     * masks, every universal zone role, show/no-show, and trigger-not-matched
     * preservation of existing effect pixels. Physical layout is irrelevant. */
    for(unsigned raw=0;raw<256u;raw++){
        const uint8_t signals=(uint8_t)raw;
        ot_arbitration_t state=ot_arbitrate(signals,false,true);
        assert(state.hazards==((signals&(OT_SIG_LEFT|OT_SIG_RIGHT))==
                              (OT_SIG_LEFT|OT_SIG_RIGHT)));
        assert(state.brake_over_parking==((signals&(OT_SIG_PARK|OT_SIG_BRAKE))==
                                         (OT_SIG_PARK|OT_SIG_BRAKE)));
        assert(!state.show_suppressed);
        for(unsigned show=0;show<=1;show++){
            const uint8_t active=show?(uint8_t)(signals&~OT_SIG_PARK):signals;
            for(unsigned role=OT_OM_OFF;role<=OT_OM_DECORATIVE;role++){
                bool expected_hit=false;
                switch(role){
                    case OT_OM_PARK_BRAKE:expected_hit=(active&(OT_SIG_PARK|OT_SIG_BRAKE))!=0;break;
                    case OT_OM_PARK:expected_hit=(active&OT_SIG_PARK)!=0;break;
                    case OT_OM_BRAKE:expected_hit=(active&OT_SIG_BRAKE)!=0;break;
                    case OT_OM_LEFT:expected_hit=(active&OT_SIG_LEFT)!=0;break;
                    case OT_OM_RIGHT:expected_hit=(active&OT_SIG_RIGHT)!=0;break;
                    case OT_OM_REVERSE:expected_hit=(active&OT_SIG_REVERSE)!=0;break;
                    case OT_OM_AUX1:expected_hit=(active&(1u<<5))!=0;break;
                    case OT_OM_AUX2:expected_hit=(active&(1u<<6))!=0;break;
                    case OT_OM_AUX3:expected_hit=(active&(1u<<7))!=0;break;
                    default:break;
                }
                ot_rgb_t before={13,17,19},after=before;
                bool hit=ot_om_trigger_overlay((uint8_t)role,signals,show!=0,32,&after);
                assert(hit==expected_hit);
                if(!hit)assert(!memcmp(&before,&after,sizeof(before)));
                else {
                    const ot_rgb_t expected_color=ot_om_trigger_color((uint8_t)role,active,32);
                    assert(!memcmp(&after,&expected_color,sizeof(after)));
                }
            }
        }
    }
    /* Exactly one default-ATT-MTU-sized readback; no dynamic allocation,
     * byte-swapping, struct-padding, or dependence on ESP32/NimBLE runtime. */
    uint8_t response[OT_TM_BLE_READ_BYTES]={0};
    ot_tm_defaults(&p);
    assert(sizeof(response)==15u);
    assert(ot_tm_ble_encode(&p,0x1234,0xA5,0xDE,true,3,response));
    const uint8_t expected_response[OT_TM_BLE_READ_BYTES]={
        1,3,0x34,0x12,0xA5,0xDE,1,1,2,3,4,5,6,7,8};
    assert(memcmp(response,expected_response,sizeof(response))==0);
    assert(ot_tm_ble_encode(&p,0,0,0,false,0,response));
    assert(response[2]==0 && response[3]==0 && response[6]==0);
    assert(!ot_tm_ble_encode(NULL,0,0,0,false,0,response));
    assert(!ot_tm_ble_encode(&p,0,0,0,false,0,NULL));
    p.function[0]=255;
    assert(!ot_tm_ble_encode(&p,0,0,0,false,0,response));
    /* Reject malformed profiles and unsupported values. */
    assert(ot_tm_from_name("park_brake")==-1);
    assert(ot_tm_from_name("")==-1);
    assert(ot_tm_from_name(NULL)==-1);
    p.function[7]=255;
    assert(!ot_tm_valid(&p));
    assert(ot_tm_resolve(&p,255)==0);
    assert(!ot_tm_valid(NULL));
    ot_tm_defaults(&p);
    assert(ot_tm_valid(&p));
    puts("Trigger Manager: default mapping, 8 inputs, remap, shared, disabled, hazard/brake priority and AUX zone roles passed");
    return 0;
}
