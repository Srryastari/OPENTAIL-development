#include <assert.h>
#include "palette_1d.h"
#include "effects_1d.h"
int main(void){
 ot_wled_rgb_t a={255,0,0},b={0,0,255};
 assert(ot_palette_1d_supported(OT_FX_EMBERS,0));
 assert(ot_palette_1d_supported(OT_FX_EMBERS,2));
 assert(ot_palette_1d_supported(OT_FX_EMBERS,3));
 assert(ot_palette_1d_supported(OT_FX_EMBERS,4));
 assert(!ot_palette_1d_supported(OT_WLED_WIPE,3));
 assert(!ot_palette_1d_supported(OT_FX_EMBERS,1));

 for(uint8_t palette=2u;palette<=72u;palette++){
   assert(ot_palette_1d_supported(OT_WLED_COLOR_CLOUDS,palette));
   assert(ot_palette_1d_supported(OT_WLED_FLOW_STRIPE,palette));
   assert(ot_palette_1d_supported(OT_WLED_GLITTER,palette));
   assert(ot_palette_1d_supported(OT_WLED_BLENDS,palette));
   assert(ot_palette_1d_supported(OT_WLED_PALETTE,palette));
   assert(ot_palette_1d_supported(OT_WLED_TWINKLEFOX,palette));
   assert(ot_palette_1d_supported(OT_WLED_TWINKLECAT,palette));
   assert(!ot_palette_1d_supported(OT_WLED_WIPE,palette));
 }
 assert(!ot_palette_1d_supported(OT_WLED_COLOR_CLOUDS,1u));
 assert(!ot_palette_1d_supported(OT_WLED_FLOW_STRIPE,73u));
 assert(!ot_palette_1d_supported(OT_FX_EMBERS,11u));
 assert(ot_palette_1d_sample(4,a,b,0).r==255);
 assert(ot_palette_1d_sample(4,a,b,255).b==255);
 assert(ot_palette_1d_sample(4,a,b,128).r>=126);
 assert(ot_palette_1d_sample(3,a,b,0).r==255);
 assert(ot_palette_1d_sample(3,a,b,255).b==255);
 assert(ot_palette_1d_sample(2,a,b,255).r==255);
 return 0;
}
