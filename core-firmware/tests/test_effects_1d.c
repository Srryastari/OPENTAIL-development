#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "effects_1d.h"
#include "wled_1d.h"
int main(void){
  uint16_t ids[160]={0};
  size_t total=ot_effects_1d_supported_ids(ids,160);
  assert(total==ot_wled_1d_count()+20u);
  assert(total==140u);
  assert(ids[0]==0&&ids[1]==1&&ids[120]==OT_FX_EMBERS);
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
    /* Resetting an effect also invalidates the previous zone pixel frame
     * in the authoritative CORE LED task. Trailing effects may read the
     * retained frame only when rendering subsequent frames of that effect. */
    memset(pix,0,sizeof(pix));
    assert(ot_effects_1d_render(&cfg,&s,500,pix,31));
    for(unsigned k=0;k<31;k++){
      if(pix[k].r>cfg.brightness||pix[k].g>cfg.brightness||pix[k].b>cfg.brightness)
        fprintf(stderr,"effect %u (%s), pixel %u, RGB %u %u %u exceeds brightness %u\\n",
                (unsigned)entry->id,entry->name,k,pix[k].r,pix[k].g,pix[k].b,cfg.brightness);
      assert(pix[k].r<=cfg.brightness&&pix[k].g<=cfg.brightness&&pix[k].b<=cfg.brightness);
    }
  }
  /* The CORE dispatcher must invoke the source-derived per-frame stateful
   * renderer, not fall back to the catalog's per-pixel approximations. */
  for(unsigned ididx=0;ididx<10;ididx++){
    const uint16_t mode=(uint16_t[]){OT_WLED_SCAN,OT_WLED_DUAL_SCAN,OT_WLED_THEATER,OT_WLED_SOLID_PATTERN,OT_WLED_HEARTBEAT,OT_WLED_SPOTS,OT_WLED_SPOTS_FADE,OT_WLED_PERCENT,OT_WLED_CHASE_FLASH,OT_WLED_RUNNING_COLOR}[ididx];
    ot_effects_1d_state_t actual;
    ot_effects_1d_reset(&actual,13u);
    ot_wled_fx_state_t reference={0};
    cfg.effect_id=mode;cfg.palette_id=0;cfg.brightness=30;
    cfg.speed=255;cfg.intensity=128;
    ot_wled_rgb_t expected[31]={0};
    for(unsigned frame=0;frame<8;frame++){
      uint32_t ms=frame*75u;
      assert(ot_effects_1d_render(&cfg,&actual,ms,pix,31));
      assert(ot_wled_fx_render(&cfg,&reference,ms,expected,31));
      assert(memcmp(pix,expected,sizeof(pix))==0);
    }
  }
  /* Theater's phase is per-zone and is never shared across outputs. */
  ot_effects_1d_state_t zone_a,zone_b;
  ot_effects_1d_reset(&zone_a,1u);
  ot_effects_1d_reset(&zone_b,2u);
  cfg.effect_id=OT_WLED_THEATER;
  assert(ot_effects_1d_render(&cfg,&zone_a,0u,pix,31));
  assert(ot_effects_1d_render(&cfg,&zone_a,300u,pix,31));
  assert(zone_a.wled.aux0==1u && zone_b.wled.aux0==0u);
  assert(ot_effects_1d_render(&cfg,&zone_b,0u,pix,31));
  assert(zone_b.wled.aux0==0u);
  ot_effects_1d_reset(&s,1);assert(ot_particles_1d_count(&s.particles)==0);
  return 0;
}
