/* Tests the deterministic WLED Blink/Strobe timing adaptation. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "wled_fx_compat.h"

static int eq(ot_wled_rgb_t a,ot_wled_rgb_t b) {
    return a.r==b.r && a.g==b.g && a.b==b.b;
}

int main(void) {
    ot_wled_fx_metadata_t meta={0};
    assert(ot_wled_fx_metadata(0,&meta) && !strcmp(meta.name,"Solid") && !meta.supports_background);
    assert(ot_wled_fx_metadata(1,&meta) && !strcmp(meta.name,"Blink") && meta.supports_intensity);
    assert(ot_wled_fx_metadata(23,&meta) && !strcmp(meta.name,"Strobe") && !meta.supports_intensity);
    assert(!ot_wled_fx_metadata(255,&meta));
    assert(!ot_wled_fx_metadata(1,NULL));

    ot_wled_fx_config_t c={
        .effect_id=OT_WLED_BLINK,.speed=128,.intensity=128,.brightness=255,
        .palette_id=0,.frame_time_ms=23,
        .foreground={200,20,5},.background={0,3,40}
    };
    ot_wled_fx_state_t s={0};
    ot_wled_rgb_t p[31]={0};
    /* (255-128)*20=2540; 2540+23*2=2586; on=23+(2540*128>>8)=1293 */
    assert(ot_wled_fx_render(&c,&s,0,p,31) && eq(p[0],c.foreground));
    assert(eq(p[0],p[30]));
    assert(ot_wled_fx_render(&c,&s,1293,p,31) && eq(p[2],c.foreground));
    assert(ot_wled_fx_render(&c,&s,1294,p,31) && eq(p[2],c.background));
    assert(ot_wled_fx_render(&c,&s,2585,p,31) && eq(p[30],c.background));
    assert(ot_wled_fx_render(&c,&s,2586,p,31) && eq(p[15],c.foreground));

    ot_wled_fx_reset(&s);
    /* Missing the opening frame still flashes for a single frame on new iteration,
       matching WLED's SEGENV.step protection. */
    assert(ot_wled_fx_render(&c,&s,3880,p,31) && eq(p[0],c.foreground));
    assert(ot_wled_fx_render(&c,&s,3881,p,31) && eq(p[0],c.background));

    c.effect_id=OT_WLED_STROBE;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,23,p,31) && eq(p[8],c.foreground));
    assert(ot_wled_fx_render(&c,&s,24,p,31) && eq(p[8],c.background));

    c.effect_id=OT_WLED_SOLID;c.brightness=127;
    assert(ot_wled_fx_render(&c,&s,1200,p,31));
    assert(eq(p[0],(ot_wled_rgb_t){100,10,2}));

    c.effect_id=42;assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.effect_id=OT_WLED_BLINK;c.palette_id=1;
    assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.palette_id=0;c.frame_time_ms=0;assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.frame_time_ms=23;assert(!ot_wled_fx_render(&c,&s,0,p,0));

    /* High-speed edge case: cycle remains nonzero even at speed 255. */
    c.speed=255;c.intensity=255;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,45,p,31));
    assert(ot_wled_fx_render(&c,&s,46,p,31));
    puts("WLED FX compatibility: Solid, Blink, Strobe timing and validation passed.");
    return 0;
}
