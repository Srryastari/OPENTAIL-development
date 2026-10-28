#include <assert.h>
#include <stddef.h>
#include "effects_1d.h"
#include "wled_1d.h"
int main(void){
  uint16_t ids[128]={0};
  size_t total=ot_effects_1d_supported_ids(ids,128);
  assert(total==ot_wled_1d_count()+1u);
  assert(total==121u);
  assert(ids[0]==0&&ids[1]==1&&ids[total-1]==OT_FX_EMBERS);
  bool has_strobe=false;
  for(size_t i=0;i<total;i++)if(ids[i]==23u)has_strobe=true;
  assert(has_strobe);
  ot_wled_fx_metadata_t meta={0};
  assert(ot_effects_1d_metadata(OT_FX_EMBERS,&meta)&&meta.id==OT_FX_EMBERS);
  assert(ot_effects_1d_metadata(67,&meta)&&meta.id==67);
  assert(!ot_effects_1d_metadata(500,&meta));
  ot_effects_1d_state_t s;
  ot_effects_1d_reset(&s,987u);
  ot_wled_fx_config_t cfg={.effect_id=OT_FX_EMBERS,.speed=180,.intensity=255,
    .brightness=255,.palette_id=0,.frame_time_ms=23,.foreground={255,40,10}};
  ot_wled_rgb_t pix[31]={0};bool lit=false;
  for(unsigned i=0;i<120;i++){
    assert(ot_effects_1d_render(&cfg,&s,i*20u,pix,31));
    for(unsigned k=0;k<31;k++)if(pix[k].r){lit=true;break;}
  }
  assert(lit&&ot_particles_1d_count(&s.particles)<=OT_PARTICLES_1D_MAX);
  cfg.palette_id=4;assert(ot_effects_1d_render(&cfg,&s,500,pix,31));
  cfg.palette_id=1;assert(!ot_effects_1d_render(&cfg,&s,500,pix,31));
  cfg.palette_id=0;cfg.effect_id=OT_WLED_SOLID;
  assert(ot_effects_1d_render(&cfg,&s,0,pix,31));
  assert(pix[0].r==255&&pix[30].r==255);
  /* Every catalog ID must resolve and render a deterministic bounded frame. */
  cfg.palette_id=0;cfg.brightness=32;cfg.foreground=(ot_wled_rgb_t){255,45,10};
  cfg.background=(ot_wled_rgb_t){3,4,5};
  for(size_t i=0;i<ot_wled_1d_count();i++){
    const ot_wled_1d_meta_t *entry=ot_wled_1d_at(i);
    assert(entry&&ot_effects_1d_metadata(entry->id,&meta));
    cfg.effect_id=entry->id;
    ot_effects_1d_reset(&s,(uint32_t)i+1u);
    assert(ot_effects_1d_render(&cfg,&s,500,pix,31));
    for(unsigned k=0;k<31;k++)assert(pix[k].r<=32&&pix[k].g<=32&&pix[k].b<=32);
  }
  ot_effects_1d_reset(&s,1);assert(ot_particles_1d_count(&s.particles)==0);
  return 0;
}
