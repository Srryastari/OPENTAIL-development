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
    assert(safety_frames>10000u);
    assert(decorative_frames>10000u);
    printf("Effects/OEM compositing matrix: %u physical trigger overrides, %u decorative frames, 256 input masks, six wire orders, 2 gamma states, 3 gain profiles, 4 overlays PASS (host only)\n",
       safety_frames,decorative_frames);
    return 0;
}
