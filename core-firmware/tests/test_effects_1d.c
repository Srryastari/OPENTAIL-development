#include <assert.h>
#include <stddef.h>
#include "effects_1d.h"
int main(void){
  uint16_t ids[9]={0};
  assert(ot_effects_1d_supported_ids(ids,9)==7);
  assert(ids[0]==0&&ids[5]==23&&ids[6]==OT_FX_EMBERS);
  ot_wled_fx_metadata_t meta={0};
  assert(ot_effects_1d_metadata(OT_FX_EMBERS,&meta)&&meta.id==OT_FX_EMBERS);
  assert(!ot_effects_1d_metadata(500,&meta));
  ot_effects_1d_state_t s;
  ot_effects_1d_reset(&s,987u);
  ot_wled_fx_config_t cfg={.effect_id=OT_FX_EMBERS,
   .speed=180,.intensity=255,.brightness=255,.palette_id=0,
   .frame_time_ms=23,.foreground={255,40,10}};
  ot_wled_rgb_t pix[31]={0};
  bool lit=false;
  for(unsigned i=0;i<120;i++){
    assert(ot_effects_1d_render(&cfg,&s,i*20u,pix,31));
    for(unsigned k=0;k<31;k++)if(pix[k].r){lit=true;break;}
  }
  assert(lit);
  assert(ot_particles_1d_count(&s.particles)<=OT_PARTICLES_1D_MAX);
  cfg.palette_id=1;assert(!ot_effects_1d_render(&cfg,&s,500,pix,31));
  cfg.palette_id=0;cfg.effect_id=OT_WLED_SOLID;
  assert(ot_effects_1d_render(&cfg,&s,0,pix,31));
  assert(pix[0].r==255&&pix[30].r==255);
  ot_effects_1d_reset(&s,1);
  assert(ot_particles_1d_count(&s.particles)==0);
  return 0;
}
