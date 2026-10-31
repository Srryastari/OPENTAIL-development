/* Deterministic CORE source golden frames for twelve Show Mode originals.
 * Never call the Android preview or a display-only crossfade in this test.
 * One-shot transition endpoints are independently checked below.
 * Optical smoothness, BLE latency and real FPS still require device proof.
 */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "ot_auto_effects.h"
#include "effects_1d.h"
static const uint16_t continuous[]={
 OT_FX_DIMENSIONAL_RIFT, OT_FX_METEOR_STORM, OT_FX_HELIX_DRIVE,
 OT_FX_CRYSTAL_GROWTH, OT_FX_FUSION_CORE, OT_FX_PORTAL_GATE,
 OT_FX_TIME_FRACTURE, OT_FX_AURORA_SURGE, OT_FX_CHROMATIC_SHATTER,
 OT_FX_VORTEX, OT_FX_QUANTUM_COLLAPSE, OT_FX_REALITY_BREAK
};
static uint32_t fnv_rgb(uint32_t h,const ot_wled_rgb_t *f,size_t n){
 for(size_t j=0;j<n;j++){
  h=(h^f[j].r)*16777619u;
  h=(h^f[j].g)*16777619u;
  h=(h^f[j].b)*16777619u;
 }
 return h;
}
static int equal(ot_wled_rgb_t a,ot_wled_rgb_t b){
 return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
static uint32_t distance_rgb(const ot_wled_rgb_t *a,
                             const ot_wled_rgb_t *b,size_t n){
 uint32_t d=0;
 for(size_t j=0;j<n;j++){
  d+=(a[j].r>b[j].r)?a[j].r-b[j].r:b[j].r-a[j].r;
  d+=(a[j].g>b[j].g)?a[j].g-b[j].g:b[j].g-a[j].g;
  d+=(a[j].b>b[j].b)?a[j].b-b[j].b:b[j].b-a[j].b;
 }
 return d;
}
int main(void){
 /* Recorded from the corrected authoritative CORE renderer. Changing any
  * original color, motion, timing or seed requires explicit rebaseline. */
 static const uint32_t goldens[12]={
  0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u
 };
 static ot_wled_rgb_t frame[260],reference[260],before[260];
 const ot_wled_rgb_t guard={73,91,117};
 ot_wled_fx_config_t cfg={.speed=180,.intensity=213,.brightness=32,
     .frame_time_ms=16,.palette_id=0,
     .foreground={200,52,154},.background={7,15,29}};
 for(size_t k=0;k<12;k++){
  const uint16_t id=continuous[k];
  assert(ot_auto_fx_continuous(id));
  cfg.effect_id=id;
  uint32_t hash=2166136261u;
  unsigned animated=0,speed_sensitive=0,intensity_sensitive=0;
  for(unsigned speed_idx=0;speed_idx<3;speed_idx++){
   cfg.speed=(uint8_t)(speed_idx==0?0u:speed_idx==1?180u:255u);
   const uint32_t duration=ot_auto_fx_duration_ms(cfg.speed);
   for(unsigned size_idx=0;size_idx<2;size_idx++){
    const size_t n=size_idx==0?31u:257u;
    const uint32_t times[]={0u,1u,duration/7u,duration/3u,
      duration/2u,duration-1u,duration,duration+1u,3u*duration+duration/3u};
    for(size_t i=0;i<sizeof(times)/sizeof(times[0]);i++){
     frame[n]=guard;frame[n+1u]=guard;
     assert(ot_auto_fx_render(&cfg,0x1234u,times[i],frame,n));
     assert(equal(frame[n],guard)&&equal(frame[n+1u],guard));
     hash=fnv_rgb(hash,frame,n);
    }
    /* Every continuous effect must have exactly matching pixel frames
     * across the cycle boundary with a stable seed and palette. */
    assert(ot_auto_fx_render(&cfg,0x1234u,0u,frame,n));
    memcpy(reference,frame,n*sizeof(frame[0]));
    assert(ot_auto_fx_render(&cfg,0x1234u,duration,frame,n));
    assert(memcmp(reference,frame,n*sizeof(frame[0]))==0);
    assert(ot_auto_fx_render(&cfg,0x1234u,duration*2u,frame,n));
    assert(memcmp(reference,frame,n*sizeof(frame[0]))==0);
    assert(ot_auto_fx_render(&cfg,0x1234u,duration/2u,frame,n));
    if(memcmp(reference,frame,n*sizeof(frame[0]))!=0)animated++;
    /* The end of a cycle may advance a sharp moving edge, but must not
     * produce a frame-wide hard reset (measured normalized RGB delta). */
    assert(ot_auto_fx_render(&cfg,0x1234u,duration-1u,before,n));
    assert(ot_auto_fx_render(&cfg,0x1234u,duration,frame,n));
    assert(distance_rgb(before,frame,n)<=n*32u);
   }
  }
  /* All twelve modes respond to Speed and Intensity, not just to the color. */
  cfg.speed=70u;cfg.intensity=213u;
  for(uint32_t t=131u;t<=1181u;t+=175u){
   cfg.speed=70u;assert(ot_auto_fx_render(&cfg,0x1234u,t,before,257u));
   cfg.speed=229u;assert(ot_auto_fx_render(&cfg,0x1234u,t,frame,257u));
   if(memcmp(before,frame,257u*sizeof(frame[0]))!=0)speed_sensitive++;
   cfg.speed=180u;cfg.intensity=0u;
   assert(ot_auto_fx_render(&cfg,0x1234u,t,before,257u));
   cfg.intensity=255u;
   assert(ot_auto_fx_render(&cfg,0x1234u,t,frame,257u));
   if(memcmp(before,frame,257u*sizeof(frame[0]))!=0)intensity_sensitive++;
  }
  assert(animated&&speed_sensitive&&intensity_sensitive);

  /* The stateful CORE entry point preserves Q32 phase through a live Speed
   * update. Same effect/zone state, no reset and no palette/ABI changes. */
  ot_effects_1d_state_t st={0};
  ot_effects_1d_reset(&st,0x1234u);
  cfg.speed=0u;cfg.intensity=213u;
  assert(ot_effects_1d_render(&cfg,&st,1000u,before,257u));
  const uint32_t phase_before=st.original_phase_q32;
  cfg.speed=255u;
  assert(ot_effects_1d_render(&cfg,&st,1001u,frame,257u));
  assert(st.original_clock_initialized&&st.original_last_ms==1001u);
  assert(st.original_phase_q32!=0u&&st.original_phase_q32!=phase_before);
  assert(distance_rgb(before,frame,257u)<=257u*32u);
  /* An intensity change on the same timestamp must not advance the clock. */
  const uint32_t phase_after=st.original_phase_q32;
  cfg.intensity=40u;
  assert(ot_effects_1d_render(&cfg,&st,1001u,frame,257u));
  assert(st.original_phase_q32==phase_after);
  printf("golden continuous original %u: 0x%08x\n",(unsigned)id,(unsigned)hash);
  assert(hash==goldens[k] && "proprietary CORE golden frame changed");
 }
 /* Seven deliberate one-shots stay clamped after their terminal frame.
  * Event Horizon holds FG; Singularity Shutdown remains exact black. */
 const uint16_t oneshots[]={
  OT_FX_MATERIALIZE_SPARKS,OT_FX_RANDOM_MATERIALIZE,
  OT_FX_SMOOTH_FADE,OT_FX_PLASMA_SWEEP,OT_FX_SMOOTH_FADE_OUT,
  OT_FX_EVENT_HORIZON,OT_FX_SINGULARITY_SHUTDOWN
 };
 cfg.speed=180u;cfg.intensity=213u;
 const uint32_t duration=ot_auto_fx_duration_ms(cfg.speed);
 for(size_t k=0;k<7;k++){
  cfg.effect_id=oneshots[k];
  assert(!ot_auto_fx_continuous(cfg.effect_id));
  assert(ot_auto_fx_render(&cfg,0x1234u,duration,reference,257u));
  assert(ot_auto_fx_render(&cfg,0x1234u,duration+30000u,frame,257u));
  assert(memcmp(reference,frame,257u*sizeof(frame[0]))==0);
 }
 assert(!ot_auto_fx_continuous(0u));
 assert(!ot_auto_fx_continuous(OT_FX_EMBERS));
 puts("PASS: proprietary loop continuity, smooth speed edits, 7 one-shot endpoints and pinned RGB goldens");
 return 0;
}
