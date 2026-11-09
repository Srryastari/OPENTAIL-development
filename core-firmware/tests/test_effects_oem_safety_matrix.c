/* Effects-team integration regression only: do not change output/trigger code.
 * Exercise the real overlay compositor and OEM trigger color functions under
 * every input mask, all color orders and destructive decorative calibration.
 * The adjacent source-contract test proves CORE uses this guarded order.
 * Software tests do not prove actual LED optical output or GPIO ISR timing.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "output_manager.h"
#include "fx_overlay_engine.h"
#include "board_config.h"
#include "wled_fx_compat.h"

static int equal(ot_rgb_t a,ot_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
static ot_rgb_t composed(uint8_t role,uint8_t mask,int decorated,
                         const ot_om_output_t *output,
                         const ot_wled_rgb_t *frame,int *safety){
    ot_rgb_t rgb={0,0,0};
    *safety=ot_om_trigger_overlay(role,mask,decorated!=0,
                                 OT_MAX_BRIGHTNESS,&rgb);
    if(!*safety && decorated){
        rgb=(ot_rgb_t){frame->r,frame->g,frame->b};
        rgb=ot_om_calibrate(rgb,output);
    }
    return rgb;
}

/* The CORE render order is FX frame first, physical trigger overlay second.
 * Keep an uninterrupted reference renderer beside a live renderer whose
 * visible frame is overridden. When the input releases, the next decorative
 * frame must be byte-identical to the uninterrupted reference: no phase reset. */
static void test_phase_continuity_behind_physical_overrides(void){
    const struct {
        uint8_t role,mask;
        const char *name;
    } cases[]={
        {OT_OM_BRAKE,OT_SIG_BRAKE,"Brake"},
        {OT_OM_LEFT,OT_SIG_LEFT,"Left"},
        {OT_OM_RIGHT,OT_SIG_RIGHT,"Right"},
        {OT_OM_LEFT,(uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT),"Hazards-left"},
        {OT_OM_RIGHT,(uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT),"Hazards-right"},
        {OT_OM_REVERSE,OT_SIG_REVERSE,"Reverse"}
    };
    const ot_wled_fx_config_t cfg={
        .effect_id=OT_WLED_CHASE_COLOR,.speed=173u,.intensity=190u,
        .brightness=255u,.frame_time_ms=16u,.palette_id=0u,
        .foreground={255u,40u,12u},.background={0u,0u,0u},
        .tertiary={0u,90u,220u},.tertiary_set=true
    };
    unsigned verified=0u;
    for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);c++){
        ot_wled_fx_state_t live={0},reference={0};
        ot_wled_rgb_t live_frame[16],reference_frame[16];
        uint32_t calls_before_override=0u;
        for(unsigned frame=0;frame<3u;frame++){
            const uint32_t now=1000u+frame*16u;
            if(frame==1u)calls_before_override=live.call;
            assert(ot_wled_fx_render(&cfg,&live,now,live_frame,16u));
            assert(ot_wled_fx_render(&cfg,&reference,now,reference_frame,16u));
            /* Chase owns no dynamic scratch; compare the complete source state. */
            assert(live.scratch==NULL&&reference.scratch==NULL);
            assert(memcmp(&live,&reference,sizeof(live))==0);
            if(frame==1u){
                ot_rgb_t visible={live_frame[0].r,live_frame[0].g,live_frame[0].b};
                assert(ot_om_trigger_overlay(cases[c].role,cases[c].mask,
                                             true,OT_MAX_BRIGHTNESS,&visible));
                const ot_rgb_t expected=ot_om_trigger_color(cases[c].role,
                                             cases[c].mask,OT_MAX_BRIGHTNESS);
                assert(equal(visible,expected));
                /* Rendering continues behind the override; the state clock
                 * advances even though the driver-visible pixel is physical. */
                assert(live.call>calls_before_override);
                assert(memcmp(&live,&reference,sizeof(live))==0);
            }else if(frame==2u){
                /* Input has released: decoration is restored at the current
                 * phase, not the phase that existed before the override. */
                for(unsigned px=0;px<16u;px++)
                    assert(live_frame[px].r==reference_frame[px].r&&
                           live_frame[px].g==reference_frame[px].g&&
                           live_frame[px].b==reference_frame[px].b);
                verified++;
            }
        }
        ot_wled_fx_reset(&live);
        ot_wled_fx_reset(&reference);
        (void)cases[c].name;
    }
    assert(verified==sizeof(cases)/sizeof(cases[0]));
    printf("Effects phase restoration: %u Brake/Left/Right/Hazards/Reverse override cases resumed at uninterrupted phase (host only)\\n",verified);
}

int main(void){
    ot_om_output_t outputs[OT_OM_OUTPUTS];ot_om_defaults(outputs);
    const uint8_t roles[]={OT_OM_PARK_BRAKE,OT_OM_PARK,OT_OM_BRAKE,
       OT_OM_LEFT,OT_OM_RIGHT,OT_OM_REVERSE,OT_OM_DECORATIVE};
    const uint8_t styles[]={OT_FX_OVERLAY_OFF,OT_FX_OVERLAY_SPARKS,
                         OT_FX_OVERLAY_FLARES,OT_FX_OVERLAY_SPARKS_FLARES};
    const uint8_t gains[][3]={{255,255,255},{1,219,7},{100,0,255}};
    const uint8_t white[][3]={{255,255,255},{3,128,229},{0,255,47}};
    unsigned safety_frames=0u,decorative_frames=0u;
    const ot_rgb_t sentinel={41,23,11};
    for(unsigned order=OT_OM_RGB;order<=OT_OM_BGR;order++){
      for(unsigned gamma=0;gamma<2u;gamma++){
        for(unsigned gain=0;gain<3u;gain++){
          ot_om_output_t *output=&outputs[0];
          output->color_order=(uint8_t)order;
          output->gamma_enabled=gamma!=0u;
          memcpy(output->rgb_gain,gains[gain],3u);
          memcpy(output->white_gain,white[gain],3u);
          for(unsigned style=0;style<4u;style++){
            ot_fx_overlay_config_t overlay={.style=styles[style],
                .peak=32u,.density=255u,.radius=4u,.fade_ms=120u,
                .color={255u,255u,255u}};
            for(unsigned mask=0;mask<256u;mask++){
              for(unsigned role=0;role<sizeof(roles)/sizeof(roles[0]);role++){
                for(unsigned decorated=0;decorated<2u;decorated++){
                  /* Bounded zone with two sentinels; overlay must not leak
                   * into an adjacent vehicle-zone boundary. */
                  struct {ot_wled_rgb_t before,frame[7],after;} p={
                    .before={41u,23u,11u},.after={19u,37u,17u}};
                  for(unsigned i=0;i<7u;i++)
                    p.frame[i]=(ot_wled_rgb_t){24u,2u,16u};
                  if(decorated)assert(ot_fx_overlay_apply(&overlay,
                           0x16401u,0u,p.frame,7u,24u));
                  assert(p.before.r==sentinel.r && p.before.g==sentinel.g &&
                         p.before.b==sentinel.b);
                  assert(p.after.r==19u&&p.after.g==37u&&p.after.b==17u);
                  int hit=0;
                  const ot_rgb_t logical=composed(roles[role],(uint8_t)mask,
                        (int)decorated,output,&p.frame[0],&hit);
                  const uint8_t active=decorated?
                             (uint8_t)(mask&~OT_SIG_PARK):(uint8_t)mask;
                  const ot_rgb_t locked=ot_om_trigger_color(roles[role],
                                           active,OT_MAX_BRIGHTNESS);
                  if(hit){
                    /* Neither source decoration, gamma nor RGB/white gain
                     * may change physical Brake, Turn or Reverse channels. */
                    assert(equal(logical,locked));
                    if(roles[role]==OT_OM_BRAKE||
                       roles[role]==OT_OM_PARK_BRAKE||
                       roles[role]==OT_OM_LEFT||
                       roles[role]==OT_OM_RIGHT||
                       roles[role]==OT_OM_REVERSE)safety_frames++;
                  }else if(decorated){
                    const ot_rgb_t deco=ot_om_calibrate(
                        (ot_rgb_t){p.frame[0].r,p.frame[0].g,p.frame[0].b},
                        output);
                    assert(equal(logical,deco));decorative_frames++;
                  }
                  const ot_rgb_t swizzled=ot_om_swizzle(logical,output->color_order);
                  const uint8_t wire[3]={swizzled.b,swizzled.r,swizzled.g};
                  /* Wire bytes represent logical RGB channels in the chosen
                   * user-order. Unswizzled logical snapshot remains intact. */
                  static const uint8_t orders[6][3]={
                      {0,1,2},{0,2,1},{1,0,2},
                      {1,2,0},{2,0,1},{2,1,0}};
                  const uint8_t v[3]={logical.r,logical.g,logical.b};
                  for(unsigned i=0;i<3u;i++)
                      assert(wire[i]==v[orders[order][i]]);
                }
              }
            }
          }
        }
      }
    }
    test_phase_continuity_behind_physical_overrides();
    assert(safety_frames>10000u);
    assert(decorative_frames>10000u);
    printf("Effects/OEM compositing matrix: %u physical trigger overrides, %u decorative frames, 256 input masks, six wire orders, 2 gamma states, 3 gain profiles, 4 overlays PASS (host only)\n",
       safety_frames,decorative_frames);
    return 0;
}
