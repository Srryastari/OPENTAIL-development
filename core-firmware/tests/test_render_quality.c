/* Renderer regression gates from the 2026-10-29 physical bench review.
 * This is a HOST test only. Physical FPS, strobe latency, output current
 * and perceptual low-brightness smoothness require real bench signoff.
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "effects_1d.h"
#include "ot_auto_effects.h"

static int same(ot_wled_rgb_t a,ot_wled_rgb_t b){
    return a.r==b.r && a.g==b.g && a.b==b.b;
}
int main(void){
    static ot_wled_rgb_t pixels[525];
    ot_effects_1d_state_t state={0};
    ot_wled_fx_config_t c={0};
    c.effect_id=OT_FX_SMOOTH_FADE_OUT;
    c.speed=180u;c.intensity=255u;c.brightness=16u;
    c.frame_time_ms=23u;c.palette_id=0u;
    c.foreground=(ot_wled_rgb_t){255u,90u,20u};
    c.background=(ot_wled_rgb_t){0u,0u,0u};
    const uint32_t duration=ot_auto_fx_duration_ms(c.speed);
    assert(duration==1400u);
    pixels[521]=(ot_wled_rgb_t){41,42,43};
    pixels[522]=(ot_wled_rgb_t){81,82,83};
    ot_effects_1d_reset(&state,7u);
    unsigned previous_red=255u,previous_green=255u,steps=0u;
    for(uint32_t t=0u;t<=duration;t+=2u){
        assert(ot_effects_1d_render(&c,&state,t,pixels,521u));
        assert(pixels[0].r<=previous_red);
        assert(pixels[0].g<=previous_green);
        assert(pixels[0].r<=16u&&pixels[0].g<=16u&&pixels[0].b<=16u);
        for(size_t i=1;i<521u;i++)assert(same(pixels[i],pixels[0]));
        if(pixels[0].r!=previous_red)steps++;
        previous_red=pixels[0].r;
        previous_green=pixels[0].g;
    }
    assert(steps>0u);
    assert(same(pixels[0],(ot_wled_rgb_t){0,0,0}));
    assert(same(pixels[521],(ot_wled_rgb_t){41,42,43}));
    assert(same(pixels[522],(ot_wled_rgb_t){81,82,83}));
    c.effect_id=OT_FX_SMOOTH_FADE;
    ot_effects_1d_reset(&state,7u);
    previous_red=0u;
    for(uint32_t t=0u;t<=duration;t+=2u){
        assert(ot_effects_1d_render(&c,&state,t,pixels,521u));
        assert(pixels[0].r>=previous_red);
        previous_red=pixels[0].r;
    }
    assert(pixels[0].r==16u);

    /* Strobes must switch at full brightness at the exact source-derived
     * timer boundaries; they must NOT inherit any decorative smoothing.
     * An actual MCU cannot output faster than its measured refresh rate. */
    c.effect_id=OT_WLED_STROBE;c.speed=255u;c.brightness=32u;
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,0};
    ot_effects_1d_reset(&state,5u);
    const uint32_t times[]={0,23,24,45,46,47,70,92,116};
    const uint8_t expected[]={32,32,0,0,32,32,0,32,0};
    for(size_t k=0;k<sizeof(times)/sizeof(times[0]);k++){
        assert(ot_effects_1d_render(&c,&state,times[k],pixels,11));
        for(size_t i=0;i<11;i++)assert(pixels[i].r==expected[k]);
    }
    /* Absent source port: Fireworks 1D is still a compatibility recipe,
     * NOT a pixel-identical upstream implementation. */
    ot_wled_fx_metadata_t meta={0};
    assert(!ot_wled_fx_metadata(90u,&meta));
    assert(ot_effects_1d_metadata(90u,&meta));
    puts("CORE quality: monotonic fade endpoints, untouched guards, crisp strobe edges, and Fireworks parity gate passed (host only)");
    return 0;
}
