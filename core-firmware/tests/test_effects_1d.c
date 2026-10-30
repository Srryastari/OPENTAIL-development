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
  const ot_wled_audio_peak_t catalog_peak={220.0f,1800.0f};
  for(size_t i=0;i<ot_wled_1d_count();i++){
    const ot_wled_1d_meta_t *entry=ot_wled_1d_at(i);
    assert(entry&&ot_effects_1d_metadata(entry->id,&meta));
    cfg.effect_id=entry->id;
    cfg.audio_peak=entry->id==OT_WLED_ROCKTAVES?&catalog_peak:NULL;
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
  /* Recheck the complete source registry, not merely ten exemplar modes.
   * Every registered direct source port must render through the unified
   * dispatcher with the same zone-local state and RGB frame. Brightness 255
   * excludes only the separately tested CORE low-brightness safety clamp. */
  uint16_t source_ids[120]={0};
  const size_t source_total=ot_wled_fx_supported_ids(source_ids,120u);
  assert(source_total>0u && source_total<=120u);
  for(size_t mode_index=0;mode_index<source_total;mode_index++){
    ot_effects_1d_state_t actual={0};
    ot_wled_fx_state_t source={0};
    const ot_wled_audio_peak_t measured_audio={220.0f,1800.0f};
    cfg.effect_id=source_ids[mode_index];cfg.palette_id=0u;
    cfg.audio_peak=source_ids[mode_index]==OT_WLED_ROCKTAVES?&measured_audio:NULL;
    cfg.brightness=255u;cfg.speed=127u;cfg.intensity=192u;
    cfg.foreground=(ot_wled_rgb_t){123u,56u,9u};
    cfg.background=(ot_wled_rgb_t){3u,88u,174u};
    ot_effects_1d_reset(&actual,(uint32_t)mode_index+1u);
    ot_wled_rgb_t dispatched[31]={0};
    ot_wled_rgb_t direct[31]={0};
    for(size_t frame=0;frame<7u;frame++){
      const uint32_t time_ms=(const uint32_t[]){0u,17u,171u,389u,1500u,5000u,65530u}[frame];
      assert(ot_effects_1d_render(&cfg,&actual,time_ms,dispatched,31u));
      assert(ot_wled_fx_render(&cfg,&source,time_ms,direct,31u));
      if(memcmp(dispatched,direct,sizeof(direct))!=0){
        fprintf(stderr,"Unified effect dispatch diverged: source ID %u at %u ms\\n",
                (unsigned)source_ids[mode_index],(unsigned)time_ms);
        assert(0 && "Unified effects source dispatch parity");
      }
    }
    ot_effects_1d_reset(&actual,1u);
    ot_wled_fx_reset(&source);
  }
  cfg.audio_peak=NULL; /* Never retain a pointer to a loop-local audio frame. */
  printf("CORE unified dispatch: %zu source-derived effects match direct renderer.\\n",source_total);


  /* Palette selection has to pass through the authoritative CORE dispatcher,
   * not just pass the standalone source renderer. Test full 522 pixel zones. */
  for(unsigned variant=0u;variant<12u;variant++) {
    for(unsigned palette=2u;palette<=12u;palette+=5u) {
      ot_effects_1d_state_t actual={0};
      ot_wled_fx_state_t reference={0};
      cfg.effect_id=variant==0u?OT_WLED_COLOR_CLOUDS:
                    variant==1u?OT_WLED_FLOW_STRIPE:variant==2u?OT_WLED_GLITTER:variant==3u?OT_WLED_BLENDS:variant==4u?OT_WLED_PALETTE:variant==5u?OT_WLED_TWINKLEFOX:variant==6u?OT_WLED_TWINKLECAT:variant==7u?OT_WLED_AURORA:variant==8u?OT_WLED_COLORTWINKLE:variant==9u?OT_WLED_SLOW_TRANSITION:variant==10u?OT_WLED_DANCING_SHADOWS:OT_WLED_FIRE_2012;
      cfg.palette_id=(uint8_t)palette;
      cfg.brightness=32u;cfg.speed=24u;cfg.intensity=32u;
      ot_wled_rgb_t output[522]={0},expected[522]={0};
      for(unsigned k=0u;k<8u;k++) {
        const uint32_t ms=k*223u;
        assert(ot_effects_1d_render(&cfg,&actual,ms,output,522u));
        assert(ot_wled_fx_render(&cfg,&reference,ms,expected,522u));
        assert(memcmp(output,expected,sizeof(output))==0);
      }
      ot_effects_1d_reset(&actual,1u);
      ot_wled_fx_reset(&reference);
    }
  }
  /* Four newly ported source modes must accept their pinned fixed
   * palettes across the complete CORE dispatcher, including audio input. */
  const uint16_t completed_ids[]={OT_WLED_EXPLODING_FIREWORKS,
       OT_WLED_PACIFICA,OT_WLED_NOISE_PAL,OT_WLED_ROCKTAVES};
  const ot_wled_audio_peak_t peak={440.0f,1900.0f};
  for(size_t n=0u;n<sizeof(completed_ids)/sizeof(completed_ids[0]);n++){
    for(unsigned palette=2u;palette<=72u;palette+=35u) {
      ot_effects_1d_state_t unified={0};
      ot_wled_fx_state_t source={0};
      cfg.effect_id=completed_ids[n];cfg.palette_id=(uint8_t)palette;
      cfg.audio_peak=cfg.effect_id==OT_WLED_ROCKTAVES?&peak:NULL;
      cfg.brightness=255u;cfg.speed=128u;cfg.intensity=128u;
      ot_wled_rgb_t dispatched[71]={0},expected[71]={0};
      for(unsigned frame=0u;frame<12u;frame++) {
        const uint32_t ms=frame*471u;
        assert(ot_effects_1d_render(&cfg,&unified,ms,dispatched,71u));
        assert(ot_wled_fx_render(&cfg,&source,ms,expected,71u));
        assert(memcmp(dispatched,expected,sizeof(dispatched))==0);
      }
      ot_effects_1d_reset(&unified,1u);
      ot_wled_fx_reset(&source);
    }
  }
  cfg.audio_peak=NULL;
  cfg.palette_id=0u;

  /* The low-level palette adapter is not sufficient: the authoritative
   * unified dispatcher must accept the SAME caller-owned global frame
   * snapshot for independent zones, without per-zone palette advancement.
   * BLE packet validation remains fixed-only until profile transport exists. */
  {
    const uint16_t palette_modes[]={OT_WLED_PALETTE,OT_WLED_SLOW_TRANSITION,
                                   OT_WLED_GLITTER,OT_WLED_FIRE_2012};
    const uint8_t external_ids[]={1u,73u,200u,201u,255u};
    const uint8_t hsv[12]={16u,255u,200u,80u,200u,255u,
                           144u,255u,180u,232u,175u,220u};
    ot_wled_palette16_t generated={0};
    assert(ot_wled_palette_from_hsv4(hsv,&generated));
    for(size_t m=0;m<sizeof(palette_modes)/sizeof(palette_modes[0]);m++){
      for(size_t p=0;p<sizeof(external_ids)/sizeof(external_ids[0]);p++){
        ot_effects_1d_state_t zone_left={0},zone_right={0};
        ot_wled_fx_state_t ref_left={0},ref_right={0};
        ot_wled_palette_random_t shared={0};
        shared.current=generated;shared.target=generated;
        cfg.effect_id=palette_modes[m];cfg.palette_id=external_ids[p];
        cfg.brightness=255u;cfg.speed=127u;cfg.intensity=128u;
        cfg.frame_time_ms=23u;
        cfg.foreground=(ot_wled_rgb_t){123u,56u,9u};
        cfg.background=(ot_wled_rgb_t){3u,88u,174u};
        cfg.resolved_palette=NULL;
        ot_wled_rgb_t left[19]={0},right[71]={0};
        ot_wled_rgb_t ref_a[19]={0},ref_b[71]={0};
        left[0]=(ot_wled_rgb_t){7u,9u,11u};
        assert(!ot_effects_1d_render(&cfg,&zone_left,0u,left,19u));
        assert(left[0].r==7u&&left[0].g==9u&&left[0].b==11u);
        assert(zone_left.wled.call==0u);
        for(uint32_t tick=0;tick<5u;tick++){
          const uint32_t now=tick*611u;
          assert(ot_wled_palette_random_tick(&shared,now,1u,23u,700u,
                                             &generated));
          const ot_wled_palette_random_t prior=shared;
          cfg.resolved_palette=&shared.current;
          assert(ot_effects_1d_render(&cfg,&zone_left,now,left,19u));
          assert(ot_effects_1d_render(&cfg,&zone_right,now,right,71u));
          assert(memcmp(&shared,&prior,sizeof(shared))==0);
          assert(ot_wled_fx_render(&cfg,&ref_left,now,ref_a,19u));
          assert(ot_wled_fx_render(&cfg,&ref_right,now,ref_b,71u));
          if(memcmp(left,ref_a,sizeof(left))||
             memcmp(right,ref_b,sizeof(right))){
            fprintf(stderr,"Resolved palette dispatch diverged mode=%u id=%u ms=%u\\n",
                    (unsigned)cfg.effect_id,(unsigned)cfg.palette_id,
                    (unsigned)now);
            assert(0 && "two-zone external palette dispatch parity");
          }
        }
        ot_effects_1d_reset(&zone_left,1u);
        ot_effects_1d_reset(&zone_right,1u);
        ot_wled_fx_reset(&ref_left);
        ot_wled_fx_reset(&ref_right);
      }
    }
    /* A pointer alone must not enable dynamic IDs in original/unsupported
     * effects. It must not alter the existing fixed-palette render path. */
    ot_wled_fx_config_t invalid=cfg;
    invalid.resolved_palette=&generated;invalid.palette_id=1u;
    invalid.effect_id=OT_FX_EMBERS;
    ot_effects_1d_state_t rejection={0};
    ot_wled_rgb_t frame[19]={0};
    assert(!ot_effects_1d_render(&invalid,&rejection,0u,frame,19u));
    invalid.effect_id=OT_WLED_WIPE;
    assert(!ot_effects_1d_render(&invalid,&rejection,0u,frame,19u));
    invalid.effect_id=OT_WLED_PALETTE;invalid.palette_id=2u;
    ot_effects_1d_state_t fixed_a={0},fixed_b={0};
    ot_wled_rgb_t with_pointer[19]={0},without_pointer[19]={0};
    assert(ot_effects_1d_render(&invalid,&fixed_a,0u,with_pointer,19u));
    invalid.resolved_palette=NULL;
    assert(ot_effects_1d_render(&invalid,&fixed_b,0u,without_pointer,19u));
    assert(memcmp(with_pointer,without_pointer,sizeof(with_pointer))==0);
    ot_effects_1d_reset(&fixed_a,1u);
    ot_effects_1d_reset(&fixed_b,1u);
    printf("CORE unified resolved palette dispatch: 4 source modes, 5 external IDs, 2 independent zones, global frame-snapshot invariants PASS.\\n");
  }
  cfg.resolved_palette=NULL;

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
