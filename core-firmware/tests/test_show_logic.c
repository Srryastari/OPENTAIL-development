#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "show_logic.h"
static int equal(ot_rgb_t a,ot_rgb_t b){return a.r==b.r && a.g==b.g && a.b==b.b;}
int main(void){
  ot_layout_t l={{6,6,6,6,6},30,32};
  ot_show_t s={.mode=OT_SHOW_SOLID,.color={255,0,0},.brightness=24,.speed=50,.chain_mask=3,.section_mask=31};
  assert(ot_show_valid(&s,&l));
  assert(strcmp(ot_show_name(s.mode),"solid")==0);
  assert(equal(ot_show_pixel(&s,&l,0,0,0),(ot_rgb_t){24,0,0}));
  assert(equal(ot_show_pixel(&s,&l,1,29,100),(ot_rgb_t){24,0,0}));
  assert(equal(ot_show_pixel(&s,&l,1,30,0),(ot_rgb_t){0,0,0}));
  s.chain_mask=1;assert(equal(ot_show_pixel(&s,&l,1,0,0),(ot_rgb_t){0,0,0}));
  s.chain_mask=3;s.section_mask=1;
  assert(equal(ot_show_pixel(&s,&l,0,6,0),(ot_rgb_t){0,0,0}));
  assert(equal(ot_show_pixel(&s,&l,0,0,0),(ot_rgb_t){24,0,0}));
  s.section_mask=31;
  s.mode=OT_SHOW_BREATHE;
  ot_rgb_t b0=ot_show_pixel(&s,&l,0,0,0),b1=ot_show_pixel(&s,&l,0,0,950);
  assert(b1.r>b0.r);
  s.mode=OT_SHOW_CHASE;
  assert(equal(ot_show_pixel(&s,&l,0,0,0),(ot_rgb_t){24,0,0}));
  assert(equal(ot_show_pixel(&s,&l,0,12,237),(ot_rgb_t){0,0,0}));
  s.mode=OT_SHOW_WIPE;
  assert(equal(ot_show_pixel(&s,&l,0,0,0),(ot_rgb_t){24,0,0}));
  assert(equal(ot_show_pixel(&s,&l,0,5,0),(ot_rgb_t){0,0,0}));
  s.mode=OT_SHOW_RAINBOW;s.brightness=32;
  ot_rgb_t rainbow=ot_show_pixel(&s,&l,0,0,100);
  assert(rainbow.r<=32 && rainbow.g<=32 && rainbow.b<=32);
  assert(!equal(rainbow,ot_show_pixel(&s,&l,0,0,600)));
  /* Effect catalog is ABI-stable for legacy BLE IDs 1..5; all added effects are bounded. */
  const char *extended[]={"strobe","scan","pulse","fade","blink","plasma_sweep",
                           "sparkle","meteor","color_waves","twinkle"};
  for(int mode=OT_SHOW_STROBE;mode<=OT_SHOW_TWINKLE;++mode){
    assert(strcmp(ot_show_name((ot_show_mode_t)mode),extended[mode-OT_SHOW_STROBE])==0);
    s.mode=(ot_show_mode_t)mode;
    assert(ot_show_valid(&s,&l));
    for(unsigned t=0;t<4000;t+=37){
      for(unsigned px=0;px<30;++px){
        ot_rgb_t out=ot_show_pixel(&s,&l,0,px,t);
        assert(out.r<=32&&out.g<=32&&out.b<=32);
        assert(equal(out,ot_show_pixel(&s,&l,0,px,t))); /* stateless deterministic */
      }
    }
    assert(equal(ot_show_pixel(&s,&l,0,30,30),(ot_rgb_t){0,0,0}));
    s.section_mask=1;
    assert(equal(ot_show_pixel(&s,&l,0,6,100),(ot_rgb_t){0,0,0}));
    s.section_mask=31;
  }
  s.mode=OT_SHOW_TWINKLE;
  s.brightness=33;assert(!ot_show_valid(&s,&l));
  s.brightness=32;s.section_mask=0;assert(!ot_show_valid(&s,&l));
  s.section_mask=31;s.speed=101;assert(!ot_show_valid(&s,&l));
  s.speed=50;s.chain_mask=4;assert(!ot_show_valid(&s,&l));
  puts("Native show logic host assertions passed.");return 0;
}
