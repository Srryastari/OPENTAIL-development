#include "output_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static void color(ot_rgb_t c,unsigned r,unsigned g,unsigned b){assert(c.r==r&&c.g==g&&c.b==b);}
int main(void){
 ot_om_output_t out[OT_OM_OUTPUTS];ot_om_defaults(out);
 assert(ot_om_valid(&out[0])&&ot_om_valid(&out[1]));
 /* Fresh/unconfigured profiles must not infer any vehicle zone or role. */
 assert(!strcmp(out[0].name,"Pixel OUT 1"));
 assert(!strcmp(out[1].name,"Pixel OUT 2"));
 for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
   assert(out[i].active_pixels==1);
   assert(out[i].zone_count==1);
   assert(!strcmp(out[i].zones[0].name,"Zone 1"));
   assert(out[i].zones[0].pixels==1 && out[i].zones[0].role==OT_OM_OFF);
   assert(ot_om_section(&out[i],0)==0 && ot_om_section(&out[i],1)==-1);
   ot_rgb_t inactive={1,1,1};
   assert(!ot_om_trigger_overlay(out[i].zones[0].role,0xff,false,32,&inactive));
 }

 ot_layout_t layout;assert(ot_om_layout(&out[0],&layout));
 assert(ot_om_role_from_name("reverse")==OT_OM_REVERSE);
 assert(ot_om_order_from_name("BRG")==OT_OM_BRG);
 assert(ot_om_role_from_name("bad")==-1);
 out[0].zones[0].pixels=7;assert(!ot_om_valid(&out[0]));
 out[0].active_pixels=7;assert(ot_om_valid(&out[0]));
 out[0].active_pixels=1;out[0].zones[0].pixels=1;assert(ot_om_valid(&out[0]));
 out[0].color_order=9;assert(!ot_om_valid(&out[0]));out[0].color_order=OT_OM_GRB;
 strcpy(out[0].name,"<script>");assert(!ot_om_valid(&out[0]));strcpy(out[0].name,"Front DRL");assert(ot_om_valid(&out[0]));
 color(ot_om_trigger_color(OT_OM_PARK_BRAKE,0,32),0,0,0);
 color(ot_om_trigger_color(OT_OM_PARK_BRAKE,OT_SIG_PARK,32),7,0,0);
 color(ot_om_trigger_color(OT_OM_PARK_BRAKE,OT_SIG_PARK|OT_SIG_BRAKE,32),32,0,0);
 color(ot_om_trigger_color(OT_OM_REVERSE,OT_SIG_REVERSE,32),32,32,32);
 color(ot_om_trigger_color(OT_OM_AUX1,1u<<5,32),16,0,32);
 /* Locked Park baseline / selective Brake, Turn, Hazards, Reverse overlays. */
 ot_rgb_t over={0,0,0};
 assert(!ot_om_trigger_overlay(OT_OM_PARK_BRAKE,OT_SIG_PARK,true,32,&over));
 assert(ot_om_trigger_overlay(OT_OM_PARK_BRAKE,OT_SIG_PARK,false,32,&over));
 color(over,7,0,0);
 assert(ot_om_trigger_overlay(OT_OM_PARK_BRAKE,OT_SIG_PARK|OT_SIG_BRAKE,true,32,&over));
 color(over,32,0,0);
 assert(!ot_om_trigger_overlay(OT_OM_LEFT,OT_SIG_BRAKE,true,32,&over));
 assert(ot_om_trigger_overlay(OT_OM_LEFT,OT_SIG_LEFT|OT_SIG_RIGHT|OT_SIG_BRAKE,true,32,&over));
 color(over,32,19,0);
 assert(ot_om_trigger_overlay(OT_OM_RIGHT,OT_SIG_LEFT|OT_SIG_RIGHT,true,32,&over));
 assert(!ot_om_trigger_overlay(OT_OM_REVERSE,OT_SIG_PARK|OT_SIG_BRAKE,true,32,&over));
 assert(ot_om_trigger_overlay(OT_OM_REVERSE,OT_SIG_REVERSE|OT_SIG_PARK,true,32,&over));
 color(over,32,32,32);
 assert(!ot_om_trigger_overlay(OT_OM_DECORATIVE,0xff,true,32,&over));
 assert(!ot_om_trigger_overlay(OT_OM_OFF,0xff,false,32,&over));
 assert(ot_om_trigger_overlay(OT_OM_AUX1,(1u<<5)|OT_SIG_PARK,true,32,&over));
 color(over,16,0,32);
 ot_rgb_t v={11,22,33};
 const uint8_t seq[6][3]={{11,22,33},{11,33,22},{22,11,33},{22,33,11},{33,11,22},{33,22,11}};
 for(int i=0;i<6;i++){ot_rgb_t s=ot_om_swizzle(v,i);uint8_t actual[3]={s.b,s.r,s.g};assert(!memcmp(actual,seq[i],3));}
 /* Universal configuration accepts non-GS400 zones and unused (zero-pixel) slots. */
 ot_om_output_t custom[OT_OM_OUTPUTS];ot_om_defaults(custom);
 strcpy(custom[0].name,"Front Accent");custom[0].active_pixels=48;custom[0].zone_count=2;
 for(int i=0;i<OT_OM_ZONES;i++){custom[0].zones[i].pixels=0;custom[0].zones[i].role=OT_OM_OFF;}
 strcpy(custom[0].zones[0].name,"DRL Outer");custom[0].zones[0].pixels=36;custom[0].zones[0].role=OT_OM_DECORATIVE;
 strcpy(custom[0].zones[1].name,"Signal Inner");custom[0].zones[1].pixels=12;custom[0].zones[1].role=OT_OM_LEFT;
 assert(ot_om_valid(&custom[0]));
 assert(ot_om_section(&custom[0],35)==0);
 assert(ot_om_section(&custom[0],36)==1);
 assert(ot_om_section(&custom[0],47)==1);
 assert(ot_om_section(&custom[0],48)==-1);
 assert(ot_om_layout(&custom[0],&layout));
 custom[0].zone_count=1; custom[0].zones[0].pixels=48;
 assert(ot_om_valid(&custom[0])); /* Any user-selected one-zone length is valid within allocated capacity. */
 custom[0].active_pixels=173;custom[0].zones[0].pixels=173;
 assert(ot_om_valid(&custom[0]));
 custom[0].zone_count=8;
 for(int j=0;j<8;j++){snprintf(custom[0].zones[j].name,OT_OM_NAME_LEN,"Zone %d",j+1);custom[0].zones[j].pixels=(j==0?173:0);custom[0].zones[j].role=OT_OM_DECORATIVE;}
 assert(ot_om_valid(&custom[0]));
 custom[0].rgb_gain[1]=190; custom[0].white_gain[2]=200;
 color(ot_om_calibrate((ot_rgb_t){32,0,0},&custom[0]),32,0,0);
 color(ot_om_calibrate((ot_rgb_t){20,20,20},&custom[0]),20,15,16);
 /* Performance regression: sequential cursor must be pixel-identical to
  * the validated random-access zone lookup for arbitrary zone sizes,
  * including zero-sized intermediate zones and non-GS400 layouts. */
 for(unsigned trial=0;trial<96u;trial++){
    ot_om_output_t *o=&custom[0];
    const unsigned total=1u+(trial*37u)%522u;
    const unsigned zones=1u+(trial%OT_OM_ZONES);
    o->active_pixels=(uint16_t)total;
    o->zone_count=(uint8_t)zones;
    unsigned remaining=total;
    for(unsigned z=0;z<zones;z++){
       snprintf(o->zones[z].name,OT_OM_NAME_LEN,"Zone %u",z+1u);
       o->zones[z].role=OT_OM_DECORATIVE;
       unsigned n=z+1u==zones?remaining:
           (remaining?(trial*19u+z*47u)%(remaining+1u):0u);
       o->zones[z].pixels=(uint16_t)n;
       remaining-=n;
    }
    assert(remaining==0u&&ot_om_valid(o));
    assert(ot_om_layout(o,&layout));
    ot_om_zone_cursor_t cursor={0};
    for(unsigned px=0;px<total;px++){
       assert(ot_om_section_next(o,&cursor,px)==ot_om_section(o,px));
    }
    assert(ot_om_section_next(o,&cursor,total)==-1);
 }
 custom[0].zone_count=0;assert(!ot_om_valid(&custom[0]));
 printf("output manager: inert universal defaults, user-configurable zones, validation, trigger priority and six color permutations passed\n");
 return 0;
}
