#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "arbitration_logic.h"
int main(void){
 ot_arbitration_t a=ot_arbitrate(0,false,false);
 assert(a.mode==OT_ARB_IDLE);
 a=ot_arbitrate(OT_SIG_PARK,false,false);
 assert(a.mode==OT_ARB_VEHICLE&&ot_arb_zone(&a,0,OT_TOP,3,31)==OT_ZONE_PARK);
 a=ot_arbitrate(OT_SIG_PARK,false,true);
 assert(a.mode==OT_ARB_SHOW&&!a.show_suppressed);
 assert(ot_arb_zone(&a,0,OT_TOP,3,31)==OT_ZONE_SHOW);
 assert(ot_arb_zone(&a,1,OT_TRUNK,1,31)==OT_ZONE_PARK);
 a=ot_arbitrate(OT_SIG_PARK|OT_SIG_BRAKE,true,true);
 assert(a.brake_over_parking&&!a.show_suppressed&&!a.demo_suppressed);
 assert(ot_arb_zone(&a,0,OT_BOTTOM,3,31)==OT_ZONE_BRAKE);
 assert(ot_arb_zone(&a,0,OT_TURN,3,31)==OT_ZONE_SHOW);
 a=ot_arbitrate(OT_SIG_LEFT|OT_SIG_RIGHT,false,true);
 assert(a.mode==OT_ARB_SHOW&&a.hazards);
 assert(ot_arb_zone(&a,0,OT_TURN,3,31)==OT_ZONE_LEFT_TURN);
 assert(ot_arb_zone(&a,1,OT_TURN,3,31)==OT_ZONE_RIGHT_TURN);
 assert(ot_arb_zone(&a,0,OT_TOP,3,31)==OT_ZONE_SHOW);
 a=ot_arbitrate(OT_SIG_REVERSE|OT_SIG_PARK,false,true);
 assert(ot_arb_zone(&a,0,OT_REVERSE,3,31)==OT_ZONE_REVERSE);
 assert(ot_arb_zone(&a,0,OT_TOP,3,31)==OT_ZONE_SHOW);
 a=ot_arbitrate(OT_SIG_REVERSE|OT_SIG_PARK,false,false);
 assert(ot_arb_zone(&a,0,OT_REVERSE,3,31)==OT_ZONE_REVERSE);
 assert(ot_arb_zone(&a,0,OT_TOP,3,31)==OT_ZONE_PARK);
 a=ot_arbitrate(OT_SIG_LEFT|OT_SIG_BRAKE,false,true);
 assert(ot_arb_zone(&a,0,OT_TURN,3,31)==OT_ZONE_LEFT_TURN);
 assert(ot_arb_zone(&a,1,OT_TURN,3,31)==OT_ZONE_SHOW);
 assert(ot_arb_zone(&a,1,OT_BOTTOM,3,31)==OT_ZONE_BRAKE);
 a=ot_arbitrate((1u<<5)|(1u<<6)|(1u<<7),false,true);
 assert(!a.show_suppressed&&ot_arb_zone(&a,0,OT_TOP,1,1)==OT_ZONE_SHOW);
 assert(ot_arb_zone(&a,1,OT_TOP,1,1)==OT_ZONE_OFF);
 a=ot_arbitrate(OT_SIG_PARK,true,false);
 assert(a.mode==OT_ARB_DEMO&&ot_arb_zone(&a,0,OT_TOP,3,31)==OT_ZONE_DEMO);
 a=ot_arbitrate(0,false,true);
 assert(!strcmp(ot_zone_mode_name(ot_arb_zone(&a,1,OT_REVERSE,2,4)),"native_show"));
 assert(!strcmp(ot_arb_mode_name(OT_ARB_VEHICLE),"vehicle"));
 assert(ot_arb_zone(&a,-1,OT_TOP,3,31)==OT_ZONE_OFF);
 assert(ot_arb_zone(&a,0,OT_SECTION_COUNT,3,31)==OT_ZONE_OFF);
 puts("Zone-local arbitration host tests passed.");return 0;
}
