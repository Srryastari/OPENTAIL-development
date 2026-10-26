#include "trigger_manager.h"
#include "output_manager.h"
#include "arbitration_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    ot_tm_profile_t p={0};
    ot_tm_defaults(&p);
    assert(ot_tm_valid(&p));
    const uint8_t masks[]={OT_SIG_PARK,OT_SIG_BRAKE,OT_SIG_LEFT,OT_SIG_RIGHT,OT_SIG_REVERSE,1u<<5,1u<<6,1u<<7};
    const char *names[]={"park","brake","left","right","reverse","aux1","aux2","aux3"};
    for(int i=0;i<OT_IN_COUNT;i++){
        assert(!strcmp(ot_tm_name(p.function[i]),names[i]));
        assert(ot_tm_from_name(names[i])==(int)p.function[i]);
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
    assert(a.mode==OT_ARB_VEHICLE&&a.hazards&&a.brake_over_parking&&a.show_suppressed);
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
