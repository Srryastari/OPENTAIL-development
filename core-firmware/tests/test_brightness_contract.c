#include <assert.h>
#include <stdio.h>
#include "board_config.h"
#include "output_manager.h"
#include "panel_zone.h"
#include "arbitration_logic.h"

static uint8_t peak(ot_rgb_t c){
    uint8_t p=c.r>c.g?c.r:c.g;
    return p>c.b?p:c.b;
}
static void same(ot_rgb_t c,uint8_t r,uint8_t g,uint8_t b){
    assert(c.r==r&&c.g==g&&c.b==b);
}
int main(void){
    const uint8_t requested[]={0u,1u,32u,64u,128u,200u,255u};
    for(unsigned i=0;i<sizeof(requested);i++){
        uint8_t expected=requested[i];
#if CONFIG_OPENTAIL_MAX_BRIGHTNESS_LIMIT < 255
        if(expected>OT_MAX_BRIGHTNESS_LIMIT)expected=OT_MAX_BRIGHTNESS_LIMIT;
#endif
        assert(ot_brightness_limit_request(requested[i])==expected);
    }
    const ot_rgb_t full[]={{255u,0u,0u},{0u,255u,0u},
                           {0u,0u,255u},{255u,255u,255u}};
    for(unsigned i=0;i<sizeof(full)/sizeof(full[0]);i++){
        const ot_rgb_t limited=ot_om_limit_brightness(full[i],OT_MAX_BRIGHTNESS_LIMIT);
        assert(peak(limited)==OT_MAX_BRIGHTNESS_LIMIT);
    }
    assert(peak(ot_om_limit_brightness((ot_rgb_t){1u,0u,0u},OT_MAX_BRIGHTNESS_LIMIT))==1u);
    const uint8_t expected_peak=200u<OT_MAX_BRIGHTNESS_LIMIT?200u:OT_MAX_BRIGHTNESS_LIMIT;
    assert(peak(ot_om_limit_brightness((ot_rgb_t){200u,100u,50u},OT_MAX_BRIGHTNESS_LIMIT))==expected_peak);
    assert(peak(ot_om_limit_brightness((ot_rgb_t){200u,100u,50u},0u))==0u);

    const ot_rgb_t trigger=ot_om_trigger_color(OT_OM_BRAKE,OT_SIG_BRAKE,255u);
    same(trigger,255u,0u,0u);
    same(ot_om_limit_brightness(trigger,OT_MAX_BRIGHTNESS_LIMIT),
         OT_MAX_BRIGHTNESS_LIMIT,0u,0u);
    const ot_rgb_t fallback=ot_pz_fallback_color(0u,OT_SIG_REVERSE,255u);
    same(fallback,255u,255u,255u);
    same(ot_om_limit_brightness(fallback,OT_MAX_BRIGHTNESS_LIMIT),
         OT_MAX_BRIGHTNESS_LIMIT,OT_MAX_BRIGHTNESS_LIMIT,OT_MAX_BRIGHTNESS_LIMIT);

    ot_om_output_t calibration={0};
    for(unsigned i=0;i<3u;i++)
        calibration.rgb_gain[i]=calibration.white_gain[i]=255u;
    calibration.gamma_enabled=true;
    same(ot_om_calibrate((ot_rgb_t){255u,255u,255u},&calibration),255u,255u,255u);
    same(ot_om_calibrate((ot_rgb_t){128u,128u,128u},&calibration),64u,64u,64u);

    ot_layout_t full_range={0};
    full_range.chain_pixels=1u;full_range.brightness_cap=255u;
    full_range.sections[0]=1u;
    assert(ot_layout_valid(&full_range));

    printf("PASS brightness contract: ceiling=%u; request 0..255; trigger/fallback/gamma 8-bit; final ratio-preserving cap\n",
           (unsigned)OT_MAX_BRIGHTNESS_LIMIT);
    return 0;
}
