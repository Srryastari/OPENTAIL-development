#include "palette_1d.h"
#include "effects_1d.h"
bool ot_palette_1d_supported(uint16_t effect_id,uint8_t palette){
 if(palette==0)return true;
 if(effect_id==OT_FX_EMBERS)return palette>=2u&&palette<=4u;
 /* Source-ported palette modes use the frame-owned global or persisted
  * custom snapshots. Usermod IDs remain unsupported without a provider. */
 ot_wled_fx_metadata_t meta={0};
 if(!ot_wled_fx_metadata(effect_id,&meta)||!meta.supports_palette)return false;
 return palette<=72u||(palette>=73u&&palette<=200u);
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
