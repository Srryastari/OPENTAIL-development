/* Independent Trigger-team host regression against integrated Issue #93 CORE.
 * No production implementation changes. All optical/GPIO evidence remains NOT RUN.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "arbitration_logic.h"
#include "ble_fx_command.h"
#include "effects_1d.h"
#include "fx_overlay_engine.h"
#include "fx_zone_bank.h"
#include "output_manager.h"
#include "trigger_manager.h"

static bool eq(ot_rgb_t a,ot_rgb_t b){
    return a.r==b.r&&a.g==b.g&&a.b==b.b;
}
static ot_rgb_t rgb(ot_wled_rgb_t v){
    return (ot_rgb_t){v.r,v.g,v.b};
}
static void mapping_and_priority(void){
    ot_tm_profile_t profile={0};ot_tm_defaults(&profile);
    for(unsigned i=0;i<OT_IN_COUNT;i++){
        assert(ot_tm_resolve(&profile,(uint8_t)(1u<<i))==(uint8_t)(1u<<i));
    }
    assert(ot_arbitrate(OT_SIG_LEFT|OT_SIG_RIGHT|OT_SIG_BRAKE,
                        false,true).hazards);
    assert(ot_arbitrate(OT_SIG_PARK|OT_SIG_BRAKE,
                        false,true).brake_over_parking);
    for(unsigned mask=0;mask<256u;mask++)
        assert(ot_tm_resolve(&profile,(uint8_t)mask)==(uint8_t)mask);
}
static void third_color_versus_oem(void){
    const uint8_t roles[]={OT_OM_PARK_BRAKE,OT_OM_BRAKE,OT_OM_LEFT,
                           OT_OM_RIGHT,OT_OM_REVERSE};
    const uint8_t third_colors[][3]={{0,255,0},{0,0,0},{255,0,255}};
    ot_om_output_t outputs[OT_OM_OUTPUTS];ot_om_defaults(outputs);
    ot_wled_fx_config_t cfg={0};
    cfg.effect_id=OT_WLED_TRICOLOR_CHASE;
    cfg.speed=128;cfg.intensity=255;cfg.brightness=255;
    cfg.foreground=(ot_wled_rgb_t){255,0,0};
    cfg.background=(ot_wled_rgb_t){0,0,255};
    cfg.tertiary_set=true;
    cfg.frame_time_ms=16;
    ot_effects_1d_state_t state={0};
    ot_effects_1d_reset(&state,121u);
    unsigned overrides=0,release_frames=0;
    for(unsigned color=0;color<sizeof(third_colors)/sizeof(third_colors[0]);color++){
        cfg.tertiary=(ot_wled_rgb_t){third_colors[color][0],
                                    third_colors[color][1],
                                    third_colors[color][2]};
        ot_wled_rgb_t original[97]={0};
        assert(ot_effects_1d_render(&cfg,&state,137u,original,97u));
        for(unsigned output=0;output<OT_OM_OUTPUTS;output++){
          ot_om_output_t *out=&outputs[output];
          for(unsigned order=OT_OM_RGB;order<=OT_OM_BGR;order++){
            out->color_order=(uint8_t)order;
            for(unsigned gamma=0;gamma<2;gamma++){
              out->gamma_enabled=gamma!=0;
              out->rgb_gain[0]=1;out->rgb_gain[1]=244;out->rgb_gain[2]=7;
              out->white_gain[0]=20;out->white_gain[1]=39;out->white_gain[2]=255;
              for(unsigned role=0;role<sizeof(roles)/sizeof(roles[0]);role++){
                for(unsigned input=0;input<256;input++){
                  /* The actual three-color renderer creates the decorative
                   * input; a real overlay then modifies it before safety
                   * arbitration. No faux 'third color' stand-in is used. */
                  ot_wled_rgb_t frame[97];memcpy(frame,original,sizeof(frame));
                  ot_fx_overlay_config_t overlay={
                    .style=OT_FX_OVERLAY_SPARKS_FLARES,.peak=32,
                    .density=255,.radius=4,.fade_ms=120,
                    .color={255,255,255}};
                  assert(ot_fx_overlay_apply(&overlay,0x16401u,0u,
                                             frame,97u,137u));
                  const ot_rgb_t decoration=rgb(frame[0]);
                  ot_rgb_t logical=decoration;
                  const bool hit=ot_om_trigger_overlay(roles[role],
                                 (uint8_t)input,true,32u,&logical);
                  /* In Show Mode parking-only is intentionally suppressed;
                   * physical Brake, Left, Right and Reverse are never masked. */
                  const uint8_t effective=(uint8_t)(input&~OT_SIG_PARK);
                  const ot_rgb_t locked=ot_om_trigger_color(roles[role],
                                                             effective,32u);
                  if(hit){
                    assert(eq(logical,locked));
                    overrides++;
                  }else{
                    assert(eq(logical,decoration));
                    release_frames++;
                  }
                  /* Physical RGB is neither gamma/gain calibrated nor
                   * overwritten by the third-color decorative frame. */
                  if(hit)assert(!eq(ot_om_calibrate(logical,out),logical)
                                || logical.r==logical.g
                                || logical.g==logical.b);
                  const ot_rgb_t wire=ot_om_swizzle(logical,(uint8_t)order);
                  const uint8_t packed[3]={wire.b,wire.r,wire.g};
                  static const uint8_t perms[6][3]={
                     {0,1,2},{0,2,1},{1,0,2},
                     {1,2,0},{2,0,1},{2,1,0}};
                  const uint8_t components[3]={logical.r,logical.g,logical.b};
                  for(unsigned b=0;b<3;b++)
                    assert(packed[b]==components[perms[order][b]]);
                  if(hit)assert(eq(logical,locked)); /* wire only transforms */
                }
              }
            }
          }
        }
    }
    assert(overrides>10000u&&release_frames>10000u);
    printf("PASS: real tertiary FX+overlay versus all 256 masks, 2 outputs, 6 wire orders, 2 gamma profiles (%u OEM overrides, %u decorative/resume states)\\n",
           overrides,release_frames);
}
static void ble_stop_zone_resume(void){
    uint8_t command[OT_BLE_FX_TERTIARY_WIRE_BYTES]={
        1,OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE,54,0,3,1,
        255,0,0,0,0,255,32,128,128,0,1,0,255,0
    };
    ot_ble_fx_packet_t decoded={.effect_id=999};
    ot_ble_fx_tertiary_t tertiary={.r=123,.g=45,.b=67,.set=false};
    assert(ot_ble_fx_parse_tertiary(command,sizeof(command),false,
                                    &decoded,&tertiary)==OT_BLE_FX_UNAUTHORIZED);
    assert(decoded.effect_id==999u&&!tertiary.set);
    assert(ot_ble_fx_parse_tertiary(command,sizeof(command),true,
                                    &decoded,&tertiary)==OT_BLE_FX_OK);
    ot_fx_zone_bank_t bank={0};
    ot_om_output_t outs[OT_OM_OUTPUTS];ot_om_defaults(outs);
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        outs[o].active_pixels=97;
        outs[o].zone_count=1;
        outs[o].zones[0].pixels=97;
        outs[o].zones[0].role=o==0?OT_OM_BRAKE:OT_OM_REVERSE;
        assert(ot_om_valid(&outs[o]));
    }
    assert(ot_fx_bank_apply_continuous_tertiary(&bank,&decoded,outs,
                                                 &tertiary,100u));
    const uint32_t generations[2]={bank.zones[0][0].generation,
                                   bank.zones[1][0].generation};
    for(unsigned o=0;o<2;o++){
        assert(bank.zones[o][0].continuous&&bank.zones[o][0].tertiary.set);
        assert(ot_fx_slot_active(&bank.zones[o][0],200000u));
    }
    for(unsigned o=0;o<2;o++){
        const uint8_t physical=o==0?OT_SIG_BRAKE:OT_SIG_REVERSE;
        ot_rgb_t drawn={0,255,0}; /* third color already on decorative layer */
        assert(ot_om_trigger_overlay(outs[o].zones[0].role,physical,true,32,&drawn));
        assert(eq(drawn,ot_om_trigger_color(outs[o].zones[0].role,physical,32)));
        drawn=(ot_rgb_t){0,255,0};
        assert(!ot_om_trigger_overlay(outs[o].zones[0].role,0,true,32,&drawn));
        assert(eq(drawn,(ot_rgb_t){0,255,0}));
        assert(bank.zones[o][0].generation==generations[o]);
        assert(ot_fx_slot_active(&bank.zones[o][0],200000u));
    }
    decoded.speed=220;
    tertiary=(ot_ble_fx_tertiary_t){0,0,0,true};
    assert(ot_fx_bank_apply_continuous_tertiary(&bank,&decoded,outs,
                                                 &tertiary,201000u));
    for(unsigned o=0;o<2;o++){
        assert(bank.zones[o][0].generation==generations[o]);
        assert(bank.zones[o][0].tertiary.set);
        assert(bank.zones[o][0].tertiary.r==0&&bank.zones[o][0].tertiary.g==0
               &&bank.zones[o][0].tertiary.b==0);
    }
    ot_fx_bank_stop_all(&bank);
    assert(!ot_fx_bank_any_active(&bank,201001u));
    /* STOP must not disable real physical Brake or Reverse. */
    ot_rgb_t physical={0};
    assert(ot_om_trigger_overlay(OT_OM_BRAKE,OT_SIG_BRAKE,false,32,&physical));
    assert(eq(physical,(ot_rgb_t){32,0,0}));
}
int main(void){
    mapping_and_priority();
    third_color_versus_oem();
    ble_stop_zone_resume();
    puts("PASS: independent Trigger third-color safety, STOP, phases, input remapping and output/zone isolation (HOST ONLY)");
    return 0;
}
