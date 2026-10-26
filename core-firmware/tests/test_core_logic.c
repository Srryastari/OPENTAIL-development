#include <assert.h>
#include <stdio.h>
#include "core_logic.h"

static bool eq(ot_rgb_t c,int r,int g,int b) {return c.r==r&&c.g==g&&c.b==b;}
int main(void) {
  ot_layout_t L={{6,6,6,6,6},30,32};
  assert(ot_layout_valid(&L));
  assert(ot_section_for_pixel(&L,0)==OT_TOP);
  assert(ot_section_for_pixel(&L,5)==OT_TOP);
  assert(ot_section_for_pixel(&L,6)==OT_TURN);
  assert(ot_section_for_pixel(&L,12)==OT_REVERSE);
  assert(ot_section_for_pixel(&L,18)==OT_BOTTOM);
  assert(ot_section_for_pixel(&L,24)==OT_TRUNK);
  assert(ot_section_for_pixel(&L,30)==-1);
  L.sections[4]=7;assert(!ot_layout_valid(&L));L.sections[4]=6;
  ot_demo_t d={OT_DEMO_SOLID,{0,255,0},255};
  assert(eq(ot_pixel_color(&L,0,0,0,&d,0),0,32,0));
  assert(eq(ot_pixel_color(&L,0,0,OT_SIG_PARK,&d,0),7,0,0));
  assert(eq(ot_pixel_color(&L,0,0,OT_SIG_BRAKE,&d,0),32,0,0));
  assert(eq(ot_pixel_color(&L,0,0,OT_SIG_BRAKE|OT_SIG_PARK,&d,0),32,0,0));
  assert(eq(ot_pixel_color(&L,0,6,OT_SIG_LEFT,&d,0),32,19,0));
  assert(eq(ot_pixel_color(&L,1,6,OT_SIG_LEFT,&d,0),0,0,0));
  assert(eq(ot_pixel_color(&L,1,6,OT_SIG_RIGHT,&d,0),32,19,0));
  assert(eq(ot_pixel_color(&L,1,12,OT_SIG_REVERSE,&d,0),32,32,32));
  assert(eq(ot_pixel_color(&L,0,0,OT_SIG_REVERSE,&d,0),0,0,0));
  assert(eq(ot_pixel_color(&L,0,18,OT_SIG_PARK,&d,0),7,0,0));
  assert(eq(ot_pixel_color(&L,0,24,OT_SIG_BRAKE,&d,0),32,0,0));
  assert(eq(ot_pixel_color(&L,0,0,0,NULL,0),0,0,0));
  ot_inputs_t in={0}; bool levels[8]={0};
  levels[1]=true;
  for(int i=0;i<3;++i)assert(!(ot_debounce(&in,levels,4)&OT_SIG_BRAKE));
  assert(ot_debounce(&in,levels,4)&OT_SIG_BRAKE);
  levels[1]=false;
  for(int i=0;i<3;++i)assert(ot_debounce(&in,levels,4)&OT_SIG_BRAKE);
  assert(!(ot_debounce(&in,levels,4)&OT_SIG_BRAKE));
  printf("All 25 CORE host assertions passed.\n");
  return 0;
}
