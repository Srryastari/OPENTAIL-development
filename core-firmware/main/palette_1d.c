#include "palette_1d.h"
#include "effects_1d.h"
bool ot_palette_1d_supported(uint16_t effect_id,uint8_t palette){
 if(palette==0)return true;
 /* Only these two source-pinned WLED adapters currently sample the
  * verified fixed 16-color palette table. Keep all other borrowed modes
  * and existing Embers settings fail-closed / backward compatible. */
 if(effect_id==OT_WLED_COLOR_CLOUDS||effect_id==OT_WLED_FLOW_STRIPE||
    effect_id==OT_WLED_GLITTER||effect_id==OT_WLED_BLENDS||
    effect_id==OT_WLED_PALETTE||effect_id==OT_WLED_TWINKLEFOX||
    effect_id==OT_WLED_TWINKLECAT||effect_id==OT_WLED_AURORA||
    effect_id==OT_WLED_COLORTWINKLE||
    effect_id==OT_WLED_SLOW_TRANSITION||
    effect_id==OT_WLED_DANCING_SHADOWS||effect_id==OT_WLED_FIRE_2012)
   return palette>=2u&&palette<=72u;
 return effect_id==OT_FX_EMBERS&&(palette==2u||palette==3u||palette==4u);
}
static uint8_t linear(uint8_t a,uint8_t b,uint8_t t){
 return (uint8_t)(((uint32_t)a*(255u-t)+(uint32_t)b*t+127u)/255u);
}
ot_wled_rgb_t ot_palette_1d_sample(uint8_t id,
  ot_wled_rgb_t fg,ot_wled_rgb_t bg,uint8_t pos){
 if(id==3)return pos<128?fg:bg; /* Colors 1 & 2 */
 if(id==4)return (ot_wled_rgb_t){linear(fg.r,bg.r,pos),
   linear(fg.g,bg.g,pos),linear(fg.b,bg.b,pos)};
 return fg; /* 0 default and 2 Color 1 */
}
