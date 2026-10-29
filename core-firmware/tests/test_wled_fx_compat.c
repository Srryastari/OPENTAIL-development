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
    /* The catalog grows toward 120 source-derived modes; test all registered
     * IDs without forcing CI changes for every newly ported mode. */
    uint16_t ids[120]={0};
    const size_t count=ot_wled_fx_supported_ids(NULL,0);
    assert(count>=105u && count<=sizeof(ids)/sizeof(ids[0]));
    assert(ot_wled_fx_supported_ids(ids,sizeof(ids)/sizeof(ids[0]))==count);
    for(size_t i=0;i<count;i++){
        ot_wled_fx_metadata_t entry={0};
        assert(ot_wled_fx_metadata(ids[i],&entry) && entry.id==ids[i]);
        for(size_t j=0;j<i;j++)assert(ids[i]!=ids[j]);
    }
    assert(ids[0]==0 && ids[1]==1 && ids[2]==3 && ids[3]==6 &&
           ids[4]==10 && ids[5]==11 && ids[6]==12 &&
           ids[7]==13 && ids[8]==23 && ids[9]==31 && ids[10]==37 && ids[11]==83 &&
           ids[12]==85 && ids[13]==86 && ids[14]==98 && ids[15]==100 &&
           ids[16]==8 && ids[17]==9 && ids[18]==14 &&
           ids[19]==24 && ids[20]==26 &&
           ids[21]==25 && ids[22]==28 && ids[23]==30 && ids[24]==33 &&
           ids[25]==50 && ids[26]==54 && ids[27]==55 && ids[28]==56 &&
           ids[29]==2 && ids[30]==15 && ids[31]==16 &&
           ids[32]==52 && ids[33]==84 &&
           ids[34]==34 && ids[35]==46 && ids[36]==47 &&
           ids[37]==108 && ids[38]==27 &&
           ids[39]==4 && ids[40]==5 && ids[41]==20 && ids[42]==21 &&
           ids[43]==22 && ids[44]==35 && ids[45]==36 && ids[46]==103 &&
           ids[47]==29 && ids[48]==39 && ids[49]==61 &&
           ids[50]==57 && ids[51]==58 && ids[52]==45 && ids[53]==184 &&
           ids[54]==68 && ids[55]==75 && ids[56]==97 && ids[57]==62 &&
           ids[58]==40 && ids[59]==41 && ids[60]==60 &&
           ids[61]==92 && ids[62]==93 && ids[63]==94 &&
           ids[64]==17 && ids[65]==32 && ids[66]==59 && ids[67]==111 &&
           ids[68]==88 && ids[69]==7 && ids[70]==117 &&
           ids[71]==18 && ids[72]==19 && ids[73]==102 && ids[74]==76 &&
           ids[75]==106 && ids[76]==78 && ids[77]==113 &&
           ids[78]==110 && ids[79]==104 && ids[80]==79 && ids[81]==99 &&
           ids[82]==42 && ids[83]==43 && ids[84]==64 &&
           ids[85]==69 && ids[86]==70 && ids[87]==71 &&
           ids[88]==72 && ids[89]==73 && ids[90]==105 && ids[91]==109 && ids[92]==63 && ids[93]==67 && ids[94]==49 && ids[95]==51 && ids[96]==91 && ids[97]==82 && ids[98]==48 && ids[99]==95 && ids[100]==96 && ids[101]==116 && ids[102]==218 && ids[103]==89 && ids[104]==44);
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

    /* Pinned upstream WLED FX.cpp deterministic 1D golden math:
     * FADE triangular wave period, Wipe/Sweep in/out positions, and
     * single-pixel upstream fallback to Solid (no palette enabled). */
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    c.brightness=255;c.speed=0;c.intensity=0;
    c.effect_id=OT_WLED_FADE;
    assert(ot_wled_fx_metadata(OT_WLED_FADE,&meta) && !meta.supports_intensity);
    assert(ot_wled_fx_render(&c,&s,0,p,4) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,3277,p,4) && eq(p[0],c.foreground));
    assert(eq(p[0],p[3]));
    assert(ot_wled_fx_render(&c,&s,6554,p,4) && p[0].r<2 && p[0].b>253);
    /* Wipe has a full cycle of 750 ms at speed=255. */
    c.effect_id=OT_WLED_WIPE;c.speed=255;
    assert(ot_wled_fx_metadata(OT_WLED_WIPE,&meta) && meta.supports_intensity);
    assert(ot_wled_fx_render(&c,&s,0,p,4));
    for(int k=0;k<4;k++)assert(eq(p[k],c.background));
    assert(ot_wled_fx_render(&c,&s,375,p,4));
    for(int k=0;k<4;k++)assert(eq(p[k],c.foreground));
    assert(ot_wled_fx_render(&c,&s,562,p,4));
    assert(eq(p[0],c.background) && eq(p[3],c.foreground));
    c.effect_id=OT_WLED_SWEEP;
    assert(ot_wled_fx_metadata(OT_WLED_SWEEP,&meta));
    assert(ot_wled_fx_render(&c,&s,562,p,4));
    assert(eq(p[0],c.foreground) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,562,p,1) && eq(p[0],c.foreground));
    /* Nontrivial fade at the partially covered wipe pixel. */
    c.effect_id=OT_WLED_WIPE;c.intensity=255;
    assert(ot_wled_fx_render(&c,&s,188,p,4));
    assert(eq(p[0],c.foreground) && p[2].r>0 && p[2].r<255);
    /* Pure metadata/renderer feature gating: unsupported palette fails. */
    c.palette_id=3;assert(!ot_wled_fx_render(&c,&s,188,p,4));
    c.palette_id=0;
    assert(!ot_wled_fx_render(&c,&s,188,p,65536u));

    /*
     * Pinned upstream FX.cpp scan(bool): with speed=255, intensity=0,
     * one illuminated pixel makes a round trip over a 4-pixel zone.
     * Dual Scan mirrors that exact logical position.
     */
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    c.brightness=255;c.speed=255;c.intensity=0;
    c.effect_id=OT_WLED_SCAN;
    assert(ot_wled_fx_metadata(OT_WLED_SCAN,&meta) && meta.supports_background);
    assert(ot_wled_fx_render(&c,&s,0,p,4));
    assert(eq(p[3],c.foreground) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,375,p,4));
    assert(eq(p[1],c.foreground) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,376,p,4));
    assert(eq(p[0],c.foreground) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,749,p,4));
    assert(eq(p[2],c.foreground));
    c.effect_id=OT_WLED_DUAL_SCAN;
    assert(ot_wled_fx_render(&c,&s,0,p,4));
    assert(eq(p[0],c.foreground) && eq(p[3],c.foreground));
    assert(eq(p[1],c.background) && eq(p[2],c.background));
    assert(ot_wled_fx_render(&c,&s,375,p,4));
    assert(eq(p[1],c.foreground) && eq(p[2],c.foreground));
    assert(eq(p[0],c.background) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,10,p,1) && eq(p[0],c.foreground));

    /* Pinned upstream running(..., theatre=true): render before advancing
     * SEGENV.aux0. Skipping an interval advances only one step per frame. */
    c.effect_id=OT_WLED_THEATER;c.intensity=0;c.speed=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[0],c.foreground) && eq(p[3],c.foreground));
    assert(eq(p[1],c.background));
    assert(ot_wled_fx_render(&c,&s,50,p,7));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background));
    assert(s.step==1 && s.aux0==1);
    assert(ot_wled_fx_render(&c,&s,51,p,7));
    assert(eq(p[1],c.foreground) && eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,250,p,7));
    assert(eq(p[1],c.foreground) && s.aux0==2);
    ot_wled_fx_reset(&s);
    assert(s.step==0 && s.aux0==0);
    /* Brightness and a nonzero intensity (wider scan dot) stay zone-local. */
    c.effect_id=OT_WLED_SCAN;c.intensity=255;c.brightness=127;
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[6],(ot_wled_rgb_t){127,0,0}));
    assert(eq(p[0],(ot_wled_rgb_t){0,0,127}));

    /* Upstream mode_static_pattern: speed+1 lit, intensity+1 unlit.
     * No dependence on GS400 wiring or absolute pixel offsets. */
    c.effect_id=OT_WLED_SOLID_PATTERN;c.speed=1;c.intensity=0;
    c.brightness=255;
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[0],c.foreground) && eq(p[1],c.foreground));
    assert(eq(p[2],c.background) && eq(p[3],c.foreground));
    assert(eq(p[5],c.background) && eq(p[6],c.foreground));
    c.speed=255;c.intensity=255;
    assert(ot_wled_fx_render(&c,&s,999,p,7));
    for(int k=0;k<7;k++)assert(eq(p[k],c.foreground));

    /* Upstream mode_heartbeat: 40 BPM at speed zero, second beat after
     * 500 ms and a new first beat after 1500 ms. Effects remain stateful. */
    c.effect_id=OT_WLED_HEARTBEAT;c.speed=0;c.intensity=128;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[0],c.background) && s.aux1==0);
    assert(ot_wled_fx_render(&c,&s,501,p,7));
    assert(s.aux0==1 && s.aux1==UINT16_MAX && p[0].r>250);
    assert(ot_wled_fx_render(&c,&s,502,p,7));
    assert(s.aux0==1 && s.aux1<UINT16_MAX);
    assert(ot_wled_fx_render(&c,&s,1501,p,7));
    assert(s.aux0==0 && s.aux1==UINT16_MAX && s.step==1501);
    ot_wled_fx_reset(&s);
    assert(s.step==0 && s.aux0==0 && s.aux1==0);

    /* Pinned FX.cpp spots_base() and its phase-varying threshold. */
    c.effect_id=OT_WLED_SPOTS;c.speed=255;c.intensity=0;
    c.brightness=255;c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    assert(ot_wled_fx_render(&c,&s,0,p,8));
    assert(eq(p[0],c.background));
    assert(p[4].r>=253 && p[4].b<=2);
    assert(p[2].r>100 && p[2].r<150);
    c.speed=0;
    assert(ot_wled_fx_render(&c,&s,0,p,8));
    assert(p[2].r==0 && p[4].r>=253); /* narrow threshold retains peak */
    c.effect_id=OT_WLED_SPOTS_FADE;
    assert(ot_wled_fx_render(&c,&s,0,p,8));
    assert(p[2].r>100 && p[2].r<150);
    assert(ot_wled_fx_render(&c,&s,4096,p,8));
    assert(eq(p[2],c.background) && p[4].r>=253);
    assert(ot_wled_fx_render(&c,&s,0,p,1));
    assert(eq(p[0],c.foreground));
    /* Multiple spots use the intensity-selected zone count, not a
     * hardcoded wiring layout or fixed pixel sequence. */
    c.effect_id=OT_WLED_SPOTS;c.speed=255;c.intensity=255;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.background) && eq(p[4],c.background));
    assert(p[2].r>200 && p[6].r>200);

    /* Pinned FX.cpp Percent uses the OLD aux1 before adjusting the fill,
     * so its animation state persists per-zone across frame callbacks. */
    c.effect_id=OT_WLED_PERCENT;c.speed=0;c.intensity=50;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    for(int k=0;k<7;k++)assert(eq(p[k],c.background));
    assert(s.aux1==1);
    assert(ot_wled_fx_render(&c,&s,20,p,7));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background) && s.aux1==2);
    assert(ot_wled_fx_render(&c,&s,40,p,7) && s.aux1==3);
    assert(ot_wled_fx_render(&c,&s,60,p,7) && s.aux1==4);
    assert(ot_wled_fx_render(&c,&s,80,p,7) && s.aux1==4);
    for(int k=0;k<7;k++)assert(eq(p[k],k<4?c.foreground:c.background));
    c.intensity=150;
    assert(ot_wled_fx_render(&c,&s,100,p,7));
    for(int k=0;k<7;k++)assert(eq(p[k],k>=3?c.foreground:c.background));
    c.intensity=200;
    assert(ot_wled_fx_render(&c,&s,120,p,7) && s.aux1==3);
    c.speed=255;c.intensity=100;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7) && s.aux1==7);
    assert(ot_wled_fx_render(&c,&s,20,p,7));
    for(int k=0;k<7;k++)assert(eq(p[k],c.foreground));
    ot_wled_fx_reset(&s);
    assert(s.aux1==0);

    /* Pinned FX.cpp running(..., theatre=false): the OLD SEGENV.aux0
     * phase renders before a once-per-cycle increment, even on frame skip. */
    c.effect_id=OT_WLED_RUNNING_COLOR;c.speed=255;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background));
    assert(eq(p[2],c.foreground) && eq(p[3],c.background));
    assert(ot_wled_fx_render(&c,&s,50,p,7));
    assert(eq(p[0],c.foreground) && s.aux0==1u && s.step==1u);
    assert(ot_wled_fx_render(&c,&s,51,p,7));
    assert(eq(p[0],c.background) && eq(p[1],c.foreground));
    assert(ot_wled_fx_render(&c,&s,250,p,7) && s.aux0==0);
    assert(ot_wled_fx_render(&c,&s,251,p,7));
    assert(eq(p[0],c.foreground));
    c.intensity=255;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    for(int k=0;k<16;k++)assert(eq(p[k],c.foreground));
    assert(eq(p[16],c.background));

    /* Pinned FX.cpp mode_chase_flash(): four flashes at one position,
     * paired on/off events and only then advance to the next pixel. */
    c.effect_id=OT_WLED_CHASE_FLASH;c.speed=255;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,7));
    assert(eq(p[0],c.background) && eq(p[1],c.background));
    assert(eq(p[2],c.foreground) && s.aux0==0 && s.aux1==1 && s.step==20);
    assert(ot_wled_fx_render(&c,&s,1,p,7));
    for(int k=0;k<7;k++)assert(eq(p[k],c.foreground));
    assert(s.aux1==1 && s.step==20);
    const uint32_t flash_times[]={20,50,70,100,120,150,170,200};
    for(size_t t=0;t<sizeof(flash_times)/sizeof(flash_times[0]);t++)
        assert(ot_wled_fx_render(&c,&s,flash_times[t],p,7));
    assert(s.aux0==1 && s.aux1==9 && s.step==210);
    assert(ot_wled_fx_render(&c,&s,210,p,7));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background) && eq(p[2],c.background));
    assert(ot_wled_fx_render(&c,&s,211,p,1));
    assert(eq(p[0],c.foreground));

    /* Fireworks (ID 42) has since been implemented: reject an actually
     * unknown ID instead of treating a supported source effect as invalid. */
    c.effect_id=OT_WLED_FIREWORKS;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    c.effect_id=UINT16_MAX;assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.effect_id=OT_WLED_BLINK;c.palette_id=1;
    assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.palette_id=0;c.frame_time_ms=0;assert(!ot_wled_fx_render(&c,&s,0,p,31));
    c.frame_time_ms=23;assert(!ot_wled_fx_render(&c,&s,0,p,0));

    /* High-speed edge case: cycle remains nonzero even at speed 255. */
    c.speed=255;c.intensity=255;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,45,p,31));
    assert(ot_wled_fx_render(&c,&s,46,p,31));
    /* Source-pin FX.cpp mode_rainbow() (Colorloop): intensity below 128
     * blends the exact FastLED hue-wheel red against RGB white. */
    c.effect_id=OT_WLED_COLORLOOP;c.speed=0;c.intensity=0;
    c.brightness=255;c.palette_id=0;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,128,128}));
    assert(eq(p[15],p[0]));
    c.intensity=255;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(ot_wled_fx_render(&c,&s,512,p,16));
    assert(!eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(s.call==3u);

    /* FX.cpp Rainbow: exact integer intensity-spread quantization and hue
     * arithmetic at palette=0, independent of physical pixel count. */
    c.effect_id=OT_WLED_RAINBOW;c.speed=0;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(eq(p[8],(ot_wled_rgb_t){234,21,0}));
    assert(s.call==1u);
    c.intensity=29;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[4],(ot_wled_rgb_t){234,21,0}));

    /* Theater Rainbow reads the OLD step as its hue, then increments the
     * phase ONCE per iteration. Per-zone states must not interfere. */
    c.effect_id=OT_WLED_THEATER_RAINBOW;c.speed=0;c.intensity=0;
    c.background=(ot_wled_rgb_t){0,0,0};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(ot_wled_fx_render(&c,&s,306,p,16) && s.step==1u);
    assert(ot_wled_fx_render(&c,&s,307,p,16));
    assert(eq(p[1],(ot_wled_rgb_t){253,2,0}));

    /* Blink/Strobe Rainbow follow the same WLED time gate as Blink/Strobe,
     * but choose the frame's hue from SEGENV.call, not elapsed milliseconds. */
    c.effect_id=OT_WLED_BLINK_RAINBOW;c.speed=255;c.intensity=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(ot_wled_fx_render(&c,&s,1,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){253,2,0}));
    c.effect_id=OT_WLED_STROBE_RAINBOW;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(ot_wled_fx_render(&c,&s,24,p,16));
    assert(eq(p[0],c.background));
    assert(ot_wled_fx_render(&c,&s,46,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){250,5,0}));
    ot_wled_fx_reset(&s);
    assert(s.call==0u);


    /* Pinned FX.cpp Strobe Mega renders its current aux1 phase before the
     * source-timed state transition, without decorative smoothing. */
    c.effect_id=OT_WLED_MULTI_STROBE;c.speed=255;c.intensity=20;
    c.foreground=(ot_wled_rgb_t){7,80,120};
    c.background=(ot_wled_rgb_t){2,3,4};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[7],c.foreground));
    assert(ot_wled_fx_render(&c,&s,1,p,16));
    assert(eq(p[7],c.background));

    /* Chase uses upstream 16-bit wrapped position, two adjacent color runs
     * and one source-compatible secondary background. */
    c.effect_id=OT_WLED_CHASE_COLOR;c.speed=0;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.foreground) && eq(p[1],c.foreground));
    assert(eq(p[2],c.background) && s.step==0u);
    c.effect_id=OT_WLED_CHASE_RAINBOW;
    c.foreground=(ot_wled_rgb_t){0,0,255};
    c.background=(ot_wled_rgb_t){0,255,0};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background));
    assert(eq(p[2],(ot_wled_rgb_t){255,0,0}));
    c.effect_id=OT_WLED_CHASE_RAINBOW_WHITE;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(eq(p[2],c.foreground));

    /* Two Dots uses the native frame-time-dependent period and half-strip
     * offset. Overlay off means untouched positions are black. */
    c.effect_id=OT_WLED_TWO_DOTS;c.speed=0;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.foreground));
    assert(eq(p[8],c.background));
    assert(eq(p[4],(ot_wled_rgb_t){0,0,0}));

    /* Three-color default is RGB black, not the vehicle's configured
     * second color; no GS400 geometry or output assumption is baked in. */
    c.effect_id=OT_WLED_TRICOLOR_CHASE;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[15],(ot_wled_rgb_t){0,0,0}));
    assert(eq(p[14],c.foreground));
    assert(eq(p[13],c.background));
    c.effect_id=OT_WLED_TRICOLOR_WIPE;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.background) && eq(p[1],c.foreground));
    c.effect_id=OT_WLED_TRICOLOR_FADE;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.foreground) && eq(p[15],c.foreground));

    /* Breathe: 30/255 base luminance from source, with the pinned integer
     * Bhaskara sine curve increasing the blend at one-quarter cycle. */
    c.effect_id=OT_WLED_BREATHE;
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    c.brightness=255;c.speed=0;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){30,0,225}));
    assert(ot_wled_fx_render(&c,&s,2621,p,16));
    assert(p[0].r>230 && p[0].b<25);

    /* Running + Saw preserve WLED's integer sine and saw-phase choices.
     * Speed zero freezes the wave, independent of output length. */
    c.effect_id=OT_WLED_RUNNING_LIGHTS;c.speed=0;c.intensity=0;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],p[15]));
    c.effect_id=OT_WLED_SAW;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(p[0].r>230 && p[0].b<25);
    c.effect_id=OT_WLED_RUNNING_DUAL;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],p[15]));
    c.effect_id=OT_WLED_SOLID_PATTERN_TRI;
    c.intensity=32;
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.foreground) && eq(p[1],c.foreground));
    assert(eq(p[2],c.background) && eq(p[3],c.background));
    assert(eq(p[4],(ot_wled_rgb_t){0,0,0}));

    /* Pinned Colorful base and high-intensity three-color phase. */
    c.effect_id=OT_WLED_COLORFUL;c.speed=0;c.intensity=0;
    c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],(ot_wled_rgb_t){255,128,64}));
    assert(eq(p[1],(ot_wled_rgb_t){229,210,65}));
    assert(eq(p[4],p[0]));
    c.intensity=200;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background));
    assert(eq(p[2],(ot_wled_rgb_t){0,0,0}));

    /* Gradient/Loading share pinned integer source maths. Gradient starts
     * from primary at index zero; Loading uses a different distance field. */
    c.effect_id=OT_WLED_GRADIENT;c.intensity=128;c.speed=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(p[0].r>240 && p[15].r<p[0].r);
    c.effect_id=OT_WLED_LOADING;ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(p[1].b>p[15].b);
    c.effect_id=OT_WLED_GRADIENT;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,16));
    assert(eq(p[0],c.background) && eq(p[15],c.background));

    /* Sine 108 source math: cubicwave8(0)=0, cubicwave8(64)=128;
     * per-frame step increases by speed/16, not elapsed_ms. */
    c.effect_id=OT_WLED_SINEWAVE;c.speed=0;c.intensity=16;
    c.brightness=255;c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(eq(p[0],(ot_wled_rgb_t){0,0,255}));
    assert(eq(p[16],(ot_wled_rgb_t){128,0,127}));
    assert(s.step==0u);
    c.speed=16;
    assert(ot_wled_fx_render(&c,&s,1000,p,31));
    assert(s.step==1u);
    assert(ot_wled_fx_render(&c,&s,1001,p,31));
    assert(s.step==2u);
    ot_wled_fx_state_t other={0};
    assert(ot_wled_fx_render(&c,&other,1001,p,31) && other.step==1u);
    assert(s.step==2u);

    /* Android 27 holds its allocation-backed state within the CORE zone.
     * The first frame extends one pixel, then independently tracks counter. */
    c.effect_id=OT_WLED_ANDROID;c.speed=255;c.intensity=128;
    c.brightness=255;c.foreground=(ot_wled_rgb_t){255,0,0};
    c.background=(ot_wled_rgb_t){0,0,255};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(eq(p[0],c.foreground) && eq(p[1],c.background));
    assert(s.data_counter==1u && s.aux1==2u);
    assert(ot_wled_fx_render(&c,&s,4,p,31) && s.data_counter==2u);
    ot_wled_fx_state_t separate={0};
    assert(ot_wled_fx_render(&c,&separate,0,p,31));
    assert(separate.data_counter==1u && s.data_counter==2u);
    ot_wled_fx_reset(&s);
    assert(s.data_counter==0u && s.aux0==0u && s.aux1==0u);

    /* Source-derived random effects follow per-zone deterministic RNG
     * streams, but hardware random-number parity is deliberately not claimed. */
    c.speed=220;c.intensity=192;c.brightness=255;
    c.foreground=(ot_wled_rgb_t){210,12,3};
    c.background=(ot_wled_rgb_t){7,8,9};
    for(size_t k=39;k<47;k++) {
        c.effect_id=ids[k];
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,1234,p,31));
        ot_wled_rgb_t first[31];
        memcpy(first,p,sizeof(first));
        uint32_t old_rng=s.rng_state;
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,1234,p,31));
        assert(old_rng==s.rng_state);
        assert(memcmp(first,p,sizeof(first))==0);
    }
    c.effect_id=OT_WLED_SPARKLE;c.speed=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(eq(p[0],c.foreground));
    assert(eq(p[1],c.background));
    c.effect_id=OT_WLED_TRAFFIC_LIGHT;c.speed=255;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(eq(p[0],(ot_wled_rgb_t){255,0,0}));
    assert(eq(p[1],c.background));
    assert(ot_wled_fx_render(&c,&s,151,p,31));
    assert(s.aux0==1u);
    assert(ot_wled_fx_render(&c,&s,152,p,31));
    assert(eq(p[1],(ot_wled_rgb_t){238,204,0}));
    c.effect_id=OT_WLED_SOLID_GLITTER;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    for(size_t i=0;i<31;i++)assert(eq(p[i],c.foreground));

    /* The Chase Random and both Stream source ports remain deterministic
     * for a given local RNG seed and output length, with no bus hardcoding. */
    for(size_t k=47;k<50;k++) {
        c.effect_id=ids[k];c.speed=170;c.intensity=85;c.brightness=255;
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,1024,p,31));
        ot_wled_rgb_t first[31];
        memcpy(first,p,sizeof(first));
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,1024,p,31));
        assert(memcmp(first,p,sizeof(first))==0);
        assert(ot_wled_fx_render(&c,&s,2048,p,31));
    }
    c.effect_id=OT_WLED_STREAM;c.speed=210;c.intensity=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(ot_wled_fx_render(&c,&s,3000,p,31));
    c.effect_id=OT_WLED_STREAM2;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(ot_wled_fx_render(&c,&s,3000,p,31));

    /* Lightning and ICU use isolated per-zone random and phase state.
     * Their visual frames remain bounded at arbitrary configurable counts. */
    for(size_t k=50;k<52;k++) {
        c.effect_id=ids[k];c.speed=60;c.intensity=128;c.brightness=255;
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        ot_wled_rgb_t baseline[31];
        memcpy(baseline,p,sizeof(baseline));
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        assert(memcmp(baseline,p,sizeof(baseline))==0);
        for(uint32_t ms=25u;ms<=3000u;ms+=125u)
            assert(ot_wled_fx_render(&c,&s,ms,p,31));
    }
    /* Fire Flicker must redraw exactly the held random values in a new
     * caller-provided frame until the original update interval expires. */
    c.effect_id=OT_WLED_FIRE_FLICKER;c.speed=0;c.intensity=127;
    c.foreground=(ot_wled_rgb_t){255,120,48};c.brightness=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    ot_wled_rgb_t held[31];
    memcpy(held,p,sizeof(held));
    assert(ot_wled_fx_render(&c,&s,10,p,31));
    assert(memcmp(held,p,sizeof(held))==0);
    assert(ot_wled_fx_render(&c,&s,310,p,31));
    c.effect_id=OT_WLED_WAVESINS;c.speed=125;c.intensity=64;
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(ot_wled_fx_render(&c,&s,1234,p,31));
    /* Palette-zero BPM/Lake source math must render for arbitrary zone
     * lengths, speed/intensity extremes, and wrapped millisecond inputs. */
    for(size_t k=54;k<56;k++) {
        c.effect_id=ids[k];c.brightness=255;
        for(unsigned speed=0;speed<=255;speed+=85) {
            c.speed=(uint8_t)speed;c.intensity=(uint8_t)(255u-speed);
            ot_wled_fx_reset(&s);
            assert(ot_wled_fx_render(&c,&s,0,p,31));
            assert(ot_wled_fx_render(&c,&s,9876543u,p,31));
        }
    }
    /* Plasma and Oscillate: multiple randomized zones are isolated,
     * every render fills all caller pixels, and configurations are universal. */
    for(size_t k=56;k<58;k++) {
        c.effect_id=ids[k];c.intensity=121;c.speed=195;c.brightness=255;
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        ot_wled_rgb_t first[31];
        memcpy(first,p,sizeof(first));
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        assert(memcmp(first,p,sizeof(first))==0);
        for(uint32_t ms=35;ms<=1200;ms+=115)
            assert(ot_wled_fx_render(&c,&s,ms,p,31));
    }
    /* Trailing modes receive a retained caller frame (matching CORE render
     * lifecycle); remain bounded when presented with a fresh frame. */
    for(size_t k=58;k<64;k++) {
        c.effect_id=ids[k];c.intensity=120;c.speed=170;c.brightness=255;
        c.background=(ot_wled_rgb_t){0,0,0};
        ot_wled_fx_reset(&s);
        memset(p,0,sizeof(p));
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        for(uint32_t ms=17u;ms<=400u;ms+=17u)
            assert(ot_wled_fx_render(&c,&s,ms,p,31));
        ot_wled_fx_reset(&s);
        memset(p,0,sizeof(p));
        assert(ot_wled_fx_render(&c,&s,0,p,31));
    }
    /* Stateful source effects draw onto a persistent caller-owned frame;
     * none may rely on hardcoded pixels, output count or global RNG. */
    for(size_t k=64;k<68;k++) {
        c.effect_id=ids[k];c.brightness=255;c.intensity=170;c.speed=195;
        c.foreground=(ot_wled_rgb_t){255,0,0};
        c.background=(ot_wled_rgb_t){0,0,0};
        memset(p,0,sizeof(p));
        ot_wled_fx_reset(&s);
        for(uint32_t t=0;t<2500u;t+=41u)
            assert(ot_wled_fx_render(&c,&s,t,p,31));
        ot_wled_fx_reset(&s);
        memset(p,0,sizeof(p));
        assert(ot_wled_fx_render(&c,&s,0,p,31));
    }
    /* Candle retains source-defined luminance until its 23-ms update gate. */
    c.effect_id=OT_WLED_CANDLE;c.brightness=255;
    c.speed=120;c.intensity=140;
    ot_wled_fx_reset(&s);
    memset(p,0,sizeof(p));
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(eq(p[0],(ot_wled_rgb_t){0,0,0}));
    assert(ot_wled_fx_render(&c,&s,23,p,31));
    ot_wled_rgb_t candle_col=p[0];
    assert(ot_wled_fx_render(&c,&s,30,p,31));
    assert(eq(p[0],candle_col));
    /* Dynamic/Dissolve source states allocate only their configured zone
     * size and are safely released on replacement or explicit reset. */
    for(size_t k=69;k<73;k++) {
        c.effect_id=ids[k];c.speed=125;c.intensity=195;c.brightness=255;
        c.foreground=(ot_wled_rgb_t){200,10,20};
        c.background=(ot_wled_rgb_t){1,2,3};
        ot_wled_fx_reset(&s);
        memset(p,0,sizeof(p));
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        assert(s.scratch!=NULL);
        assert(s.scratch_bytes>=31u);
        uint8_t *scratch_before=s.scratch;
        for(uint32_t t=1u;t<2000u;t+=29u)
            assert(ot_wled_fx_render(&c,&s,t,p,31));
        assert(s.scratch==scratch_before);
        ot_wled_fx_reset(&s);
        assert(s.scratch==NULL && s.scratch_bytes==0u);
    }
    /* Scratch-backed WLED Candle Multi and Meteor retain independent
     * per-pixel state, with arbitrary configured 1D zone length. */
    for(size_t k=73;k<75;k++) {
        c.effect_id=ids[k];c.speed=128;c.intensity=220;c.brightness=255;
        c.foreground=(ot_wled_rgb_t){230,120,45};
        c.background=(ot_wled_rgb_t){0,0,0};
        ot_wled_fx_reset(&s);
        memset(p,0,sizeof(p));
        for(uint32_t t=0;t<1500u;t+=25u)
            assert(ot_wled_fx_render(&c,&s,t,p,31));
        assert(s.scratch!=NULL && s.scratch_bytes>=31u);
        ot_wled_fx_reset(&s);
        assert(s.scratch==NULL && s.scratch_bytes==0u);
    }
    c.effect_id=OT_WLED_TWINKLEUP;c.speed=175;c.intensity=128;
    c.brightness=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    ot_wled_rgb_t twinkleup_first[31];
    memcpy(twinkleup_first,p,sizeof(p));
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(memcmp(twinkleup_first,p,sizeof(p))==0);
    assert(ot_wled_fx_render(&c,&s,10000,p,31));
    /* Source-derived geometry modes: exercise 1D zones of varied lengths
     * and WLED's positive and negative integer phase transitions. */
    for(size_t k=76;k<80;k++) {
        c.effect_id=ids[k];c.brightness=255;c.intensity=140;
        for(unsigned speed=0u;speed<=255u;speed+=85u) {
            c.speed=(uint8_t)speed;
            ot_wled_fx_reset(&s);
            memset(p,0,sizeof(p));
            assert(ot_wled_fx_render(&c,&s,0,p,31));
            assert(ot_wled_fx_render(&c,&s,123456u,p,31));
        }
    }
    for(size_t k=80;k<82;k++) {
        c.effect_id=ids[k];c.brightness=255;c.intensity=220;c.speed=120;
        c.foreground=(ot_wled_rgb_t){100,150,200};
        c.background=(ot_wled_rgb_t){0,0,0};
        ot_wled_fx_reset(&s);
        for(uint32_t ms=0u;ms<4000u;ms+=23u)
            assert(ot_wled_fx_render(&c,&s,ms,p,31));
        assert(s.scratch!=NULL && s.scratch_bytes>=sizeof(uint32_t));
        ot_wled_fx_reset(&s);
        assert(s.scratch==NULL);
    }
    /* Retained source FX 1D Fireworks/Rain/Juggle: verify no pixel escapes
     * output's configured length and effect state is independently reset. */
    for(size_t k=82;k<85;k++) {
        c.effect_id=ids[k];c.intensity=180;c.speed=120;c.brightness=255;
        c.foreground=(ot_wled_rgb_t){220,90,30};
        c.background=(ot_wled_rgb_t){0,0,0};
        memset(p,0,sizeof(p));
        ot_wled_fx_reset(&s);
        for(uint32_t ms=0u;ms<1200u;ms+=23u)
            assert(ot_wled_fx_render(&c,&s,ms,p,31));
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
    }
    /* Juggle overlaps OR-composited dots. Individual pre-scaled colors must
     * not combine into channels brighter than the requested output cap. */
    c.effect_id=OT_WLED_JUGGLE;c.brightness=32;c.intensity=255;c.speed=120;
    memset(p,0,sizeof(p));ot_wled_fx_reset(&s);
    for(uint32_t ms=0;ms<1800u;ms+=23u){
        assert(ot_wled_fx_render(&c,&s,ms,p,31));
        for(size_t i=0;i<31;i++)
            assert(p[i].r<=32u && p[i].g<=32u && p[i].b<=32u);
    }
    /* Five noise FX use original 2D/3D fixed-point Perlin gradients.
     * The RGB palette-zero profile must remain bounded and deterministic. */
    for(size_t k=85;k<90;k++) {
        c.effect_id=ids[k];c.brightness=255;c.intensity=200;c.speed=175;
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        assert(ot_wled_fx_render(&c,&s,10000u,p,31));
        ot_wled_fx_reset(&s);
        assert(ot_wled_fx_render(&c,&s,0,p,31));
    }
    /* Phase state is per zone: a different zone's call cannot advance it. */
    c.effect_id=OT_WLED_PHASED;c.speed=32;c.intensity=255;
    c.foreground=(ot_wled_rgb_t){200,20,5};
    c.background=(ot_wled_rgb_t){0,3,40};
    c.brightness=255;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    uint32_t first_phase=s.step;
    assert(ot_wled_fx_render(&c,&s,23,p,31));
    assert(s.step!=first_phase);
    ot_wled_fx_state_t phased_independent={0};
    assert(ot_wled_fx_render(&c,&phased_independent,23,p,31));
    assert(phased_independent.step==first_phase);
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    assert(s.step==first_phase);

    /* Phased Noise must use the 1D upstream Perlin field, with per-zone
     * phase advancing on each call and no cross-zone state sharing. */
    c.effect_id=OT_WLED_PHASED_NOISE;c.speed=96;c.intensity=220;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    uint32_t noise_first_phase=s.step;
    assert(ot_wled_fx_render(&c,&s,23,p,31));
    assert(s.step!=noise_first_phase);
    ot_wled_fx_state_t noise_independent={0};
    assert(ot_wled_fx_render(&c,&noise_independent,0,p,31));
    assert(noise_independent.step==noise_first_phase);

    /* Pride 2015 must animate its own pseudotime/hue state rather than
     * borrow another zone's frame counter or allocations. */
    c.effect_id=OT_WLED_PRIDE_2015;c.speed=96;c.intensity=200;
    c.brightness=255;
    memset(p,0,sizeof(p));
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    const uint32_t pride_first_step=s.step;
    assert(pride_first_step!=0u);
    assert(ot_wled_fx_render(&c,&s,23,p,31));
    assert(s.step!=pride_first_step);
    ot_wled_fx_state_t pride_other={0};
    assert(ot_wled_fx_render(&c,&pride_other,0,p,31));
    assert(pride_other.step==pride_first_step);

    /* Colorwaves shares the upstream phase/hue engine but uses video-faded
     * foreground with a different blend strength. Keep zones isolated. */
    c.effect_id=OT_WLED_COLORWAVES;c.speed=80;c.intensity=128;
    c.brightness=255;
    memset(p,0,sizeof(p));ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    const uint32_t waves_first_step=s.step;
    assert(waves_first_step!=0u);
    assert(ot_wled_fx_render(&c,&s,23,p,31));
    assert(s.step!=waves_first_step);
    ot_wled_fx_state_t waves_other={0};
    assert(ot_wled_fx_render(&c,&waves_other,0,p,31));
    assert(waves_other.step==waves_first_step);

    /* Fairy 49, source flasher grouping and per-zone scratch:
     * - intensity=0 leaves its original foreground base, no allocation;
     * - high intensity allocates one flasher per source distance;
     * - zone state and seeded sequences are independent and repeatable;
     * - rendering never writes outside a caller-supplied LED count. */
    c.effect_id=OT_WLED_FAIRY;c.brightness=255;
    c.foreground=(ot_wled_rgb_t){134,45,23};
    c.background=(ot_wled_rgb_t){4,5,6};
    c.speed=120;c.intensity=0;
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,p,31));
    for(size_t i=0;i<31u;i++)assert(eq(p[i],c.foreground));
    assert(s.scratch==NULL);
    c.intensity=255u;
    ot_wled_fx_reset(&s);
    ot_wled_rgb_t guard[33];
    for(size_t i=0;i<33u;i++)guard[i]=(ot_wled_rgb_t){88u,99u,77u};
    assert(ot_wled_fx_render(&c,&s,0,guard+1u,31u));
    assert(eq(guard[0],(ot_wled_rgb_t){88u,99u,77u}));
    assert(eq(guard[32],(ot_wled_rgb_t){88u,99u,77u}));
    assert(s.scratch!=NULL && s.scratch_bytes>=32u*4u);
    ot_wled_rgb_t first_frame[31];
    memcpy(first_frame,guard+1u,sizeof(first_frame));
    ot_wled_fx_state_t fairy_other={0};
    ot_wled_rgb_t other_pixels[31]={0};
    assert(ot_wled_fx_render(&c,&fairy_other,0,other_pixels,31u));
    assert(!memcmp(first_frame,other_pixels,sizeof(first_frame)));
    for(uint32_t ms=23u;ms<1600u;ms+=23u) {
        assert(ot_wled_fx_render(&c,&s,ms,guard+1u,31u));
        assert(eq(guard[0],(ot_wled_rgb_t){88u,99u,77u}));
        assert(eq(guard[32],(ot_wled_rgb_t){88u,99u,77u}));
    }
    ot_wled_fx_reset(&fairy_other);
    ot_wled_fx_reset(&s);
    assert(s.scratch==NULL);

    /* Fairytwinkle 51 shares the upstream 4-byte Flasher record but keeps
     * one instance per LED; guard output boundaries, repeat initial state
     * deterministically, and check no state leaks between two zones. */
    c.effect_id=OT_WLED_FAIRYTWINKLE;c.brightness=255;
    c.speed=64;c.intensity=192;
    ot_wled_rgb_t fairy_guard[33];
    for(size_t i=0;i<33u;i++)fairy_guard[i]=(ot_wled_rgb_t){77u,88u,99u};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,fairy_guard+1u,31u));
    assert(s.scratch!=NULL && s.scratch_bytes>=31u*4u);
    assert(eq(fairy_guard[0],(ot_wled_rgb_t){77u,88u,99u}));
    assert(eq(fairy_guard[32],(ot_wled_rgb_t){77u,88u,99u}));
    ot_wled_rgb_t fairytwinkle_first[31];
    memcpy(fairytwinkle_first,fairy_guard+1u,sizeof(fairytwinkle_first));
    ot_wled_fx_state_t fairytwinkle_other={0};
    ot_wled_rgb_t fairytwinkle_pixels[31]={0};
    assert(ot_wled_fx_render(&c,&fairytwinkle_other,0,
                             fairytwinkle_pixels,31u));
    assert(!memcmp(fairytwinkle_first,fairytwinkle_pixels,
                   sizeof(fairytwinkle_first)));
    for(uint32_t ms=23u;ms<1900u;ms+=23u) {
        assert(ot_wled_fx_render(&c,&s,ms,fairy_guard+1u,31u));
        assert(eq(fairy_guard[0],(ot_wled_rgb_t){77u,88u,99u}));
        assert(eq(fairy_guard[32],(ot_wled_rgb_t){77u,88u,99u}));
    }
    ot_wled_fx_reset(&s);
    ot_wled_fx_reset(&fairytwinkle_other);
    assert(s.scratch==NULL && fairytwinkle_other.scratch==NULL);

    /* Bouncing Balls 91: original 16 source balls, per-zone owned state.
     * Guard against out-of-zone drawing at all speeds and during bounce
     * relaunch, verify byte-for-byte deterministic reset and single pixel
     * source fallback. Pixel count comes only from caller, not GS400. */
    c.effect_id=OT_WLED_BOUNCING_BALLS;c.brightness=255;
    c.foreground=(ot_wled_rgb_t){210,45,10};
    c.background=(ot_wled_rgb_t){0,4,30};
    c.intensity=255;c.speed=128;
    ot_wled_rgb_t balls_guard[33];
    for(size_t i=0;i<33;i++)balls_guard[i]=(ot_wled_rgb_t){80,70,60};
    ot_wled_fx_reset(&s);
    assert(ot_wled_fx_render(&c,&s,0,balls_guard+1,31));
    assert(s.scratch && s.scratch_bytes>=16u*3u*sizeof(uint32_t));
    assert(eq(balls_guard[0],(ot_wled_rgb_t){80,70,60}));
    assert(eq(balls_guard[32],(ot_wled_rgb_t){80,70,60}));
    ot_wled_rgb_t bounce_initial[31];
    memcpy(bounce_initial,balls_guard+1,sizeof(bounce_initial));
    ot_wled_fx_state_t balls_other={0};
    ot_wled_rgb_t balls_other_pixels[31]={0};
    assert(ot_wled_fx_render(&c,&balls_other,0,balls_other_pixels,31));
    assert(!memcmp(bounce_initial,balls_other_pixels,sizeof(bounce_initial)));
    for(uint32_t ms=23u;ms<5000u;ms+=23u) {
        assert(ot_wled_fx_render(&c,&s,ms,balls_guard+1,31));
        assert(eq(balls_guard[0],(ot_wled_rgb_t){80,70,60}));
        assert(eq(balls_guard[32],(ot_wled_rgb_t){80,70,60}));
    }
    for(unsigned speed=0;speed<=255u;speed+=85u) {
        c.speed=(uint8_t)speed;
        for(unsigned intensity=0;intensity<=255u;intensity+=85u) {
            c.intensity=(uint8_t)intensity;
            ot_wled_fx_reset(&s);
            assert(ot_wled_fx_render(&c,&s,0,balls_guard+1,31));
            assert(ot_wled_fx_render(&c,&s,1500,balls_guard+1,31));
        }
    }
    ot_wled_fx_reset(&s);ot_wled_fx_reset(&balls_other);
    assert(s.scratch==NULL && balls_other.scratch==NULL);
    assert(ot_wled_fx_render(&c,&s,0,balls_guard+1,1));
    assert(eq(balls_guard[1],c.foreground));
    assert(s.scratch==NULL);

    /* Halloween Eyes 82: 1D source state with WLED short-segment static
     * fallback, zero-bound random draws and on/blink/off times. Tests use
     * several arbitrary LED counts (no GS400-dependent zone constants). */
    c.effect_id=OT_WLED_HALLOWEEN_EYES;c.speed=32u;c.intensity=0u;
    c.brightness=32u;c.foreground=(ot_wled_rgb_t){200u,40u,10u};
    c.background=(ot_wled_rgb_t){4u,20u,50u};
    ot_wled_rgb_t eyes[260];
    for(size_t length=1u;length<=5u;length++){
        ot_wled_fx_reset(&s);
        for(size_t pxi=0;pxi<260u;pxi++)eyes[pxi]=(ot_wled_rgb_t){111u,99u,77u};
        assert(ot_wled_fx_render(&c,&s,0u,eyes+1u,length));
        assert(eq(eyes[0],(ot_wled_rgb_t){111u,99u,77u}));
        assert(eq(eyes[length+1u],(ot_wled_rgb_t){111u,99u,77u}));
        if(length<5u) {
            assert(eq(eyes[1],(ot_wled_rgb_t){25u,5u,1u}));
            assert(s.scratch==NULL);
        } else {
            assert(s.scratch!=NULL && s.scratch_bytes>=16u);
        }
    }
    c.speed=200u;c.intensity=255u;
    for(size_t length=7u;length<=257u;length=length==7u?31u:
                                      length==31u?257u:258u) {
        ot_wled_fx_reset(&s);
        for(uint32_t ms=0u;ms<60000u;ms+=911u) {
            assert(ot_wled_fx_render(&c,&s,ms,eyes+1u,length));
            for(size_t px=1u;px<=length;px++)
                assert(eyes[px].r<=32u && eyes[px].g<=32u && eyes[px].b<=32u);
        }
    }
    ot_wled_fx_reset(&s);
    assert(s.scratch==NULL);


    /* Rolling Balls is a real per-zone source port, not a catalog alias.
     * Independent zones seeded alike must reproduce the same trajectories,
     * with no outside-buffer writes and with scratch released at reset. */
    c.effect_id=OT_WLED_ROLLING_BALLS;
    c.speed=222u;c.intensity=255u;c.brightness=32u;
    ot_wled_fx_state_t rolling_a={0},rolling_b={0};
    ot_wled_rgb_t rolling_aa[33],rolling_bb[33];
    for(unsigned k=0u;k<40u;k++) {
        const uint32_t ms=k*913u;
        for(size_t px=0;px<33u;px++) {
            rolling_aa[px]=(ot_wled_rgb_t){99,88,77};
            rolling_bb[px]=(ot_wled_rgb_t){99,88,77};
        }
        assert(ot_wled_fx_render(&c,&rolling_a,ms,&rolling_aa[1],31u));
        assert(ot_wled_fx_render(&c,&rolling_b,ms,&rolling_bb[1],31u));
        assert(memcmp(rolling_aa,rolling_bb,sizeof(rolling_aa))==0);
        assert(eq(rolling_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(rolling_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1;px<=31u;px++) {
            assert(rolling_aa[px].r<=32u &&
                   rolling_aa[px].g<=32u &&
                   rolling_aa[px].b<=32u);
        }
    }
    assert(rolling_a.scratch && rolling_a.scratch_bytes>=16u*16u);
    ot_wled_fx_reset(&rolling_a);
    ot_wled_fx_reset(&rolling_b);
    assert(!rolling_a.scratch && !rolling_b.scratch);


    /* Popcorn source mode: 21 independent Spark records per active zone,
     * deterministic per-zone RNG, guard bytes and complete scratch cleanup. */
    c.effect_id=OT_WLED_POPCORN;
    c.speed=243u;c.intensity=240u;c.brightness=32u;
    ot_wled_fx_state_t popcorn_a={0},popcorn_b={0};
    ot_wled_rgb_t popcorn_aa[33],popcorn_bb[33];
    for(unsigned k=0u;k<60u;k++) {
        const uint32_t ms=k*839u;
        for(size_t px=0;px<33u;px++) {
            popcorn_aa[px]=(ot_wled_rgb_t){99,88,77};
            popcorn_bb[px]=(ot_wled_rgb_t){99,88,77};
        }
        assert(ot_wled_fx_render(&c,&popcorn_a,ms,&popcorn_aa[1],31u));
        assert(ot_wled_fx_render(&c,&popcorn_b,ms,&popcorn_bb[1],31u));
        assert(memcmp(popcorn_aa,popcorn_bb,sizeof(popcorn_aa))==0);
        assert(eq(popcorn_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(popcorn_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1u;px<=31u;px++)
            assert(popcorn_aa[px].r<=32u &&
                   popcorn_aa[px].g<=32u &&
                   popcorn_aa[px].b<=32u);
    }
    assert(popcorn_a.scratch && popcorn_a.scratch_bytes>=420u);
    ot_wled_fx_reset(&popcorn_a);
    ot_wled_fx_reset(&popcorn_b);
    assert(!popcorn_a.scratch && !popcorn_b.scratch);


    /* Drip source mode: four independent Spark records per zone, deterministic
     * physics and source RNG, bounds and scratch release on replacement. */
    c.effect_id=OT_WLED_DRIP;
    c.speed=241u;c.intensity=255u;c.brightness=32u;
    ot_wled_fx_state_t drip_a={0},drip_b={0};
    ot_wled_rgb_t drip_aa[33],drip_bb[33];
    for(unsigned k=0u;k<60u;k++) {
        const uint32_t ms=k*917u;
        for(size_t px=0;px<33u;px++) {
            drip_aa[px]=(ot_wled_rgb_t){99,88,77};
            drip_bb[px]=(ot_wled_rgb_t){99,88,77};
        }
        assert(ot_wled_fx_render(&c,&drip_a,ms,&drip_aa[1],31u));
        assert(ot_wled_fx_render(&c,&drip_b,ms,&drip_bb[1],31u));
        assert(memcmp(drip_aa,drip_bb,sizeof(drip_aa))==0);
        assert(eq(drip_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(drip_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1u;px<=31u;px++)
            assert(drip_aa[px].r<=32u &&
                   drip_aa[px].g<=32u &&
                   drip_aa[px].b<=32u);
    }
    assert(drip_a.scratch && drip_a.scratch_bytes>=80u);
    ot_wled_fx_reset(&drip_a);
    ot_wled_fx_reset(&drip_b);
    assert(!drip_a.scratch && !drip_b.scratch);


    /* TV Simulator scene state stays independent for each zone, preserves
     * bounded RGB and uses stable source RNG/timing, without pixel leakage. */
    c.effect_id=OT_WLED_TV_SIMULATOR;
    c.speed=128u;c.intensity=200u;c.brightness=32u;
    ot_wled_fx_state_t tv_a={0},tv_b={0};
    ot_wled_rgb_t tv_aa[33],tv_bb[33];
    for(unsigned k=0u;k<60u;k++) {
        const uint32_t ms=k*1013u;
        for(size_t px=0;px<33u;px++) {
            tv_aa[px]=(ot_wled_rgb_t){99,88,77};
            tv_bb[px]=(ot_wled_rgb_t){99,88,77};
        }
        assert(ot_wled_fx_render(&c,&tv_a,ms,&tv_aa[1],31u));
        assert(ot_wled_fx_render(&c,&tv_b,ms,&tv_bb[1],31u));
        assert(memcmp(tv_aa,tv_bb,sizeof(tv_aa))==0);
        assert(eq(tv_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(tv_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1u;px<=31u;px++)
            assert(tv_aa[px].r<=32u && tv_aa[px].g<=32u && tv_aa[px].b<=32u);
    }
    assert(tv_a.scratch && tv_a.scratch_bytes>=40u);
    ot_wled_fx_reset(&tv_a);
    ot_wled_fx_reset(&tv_b);
    assert(!tv_a.scratch && !tv_b.scratch);


    /* Color Clouds must not reuse global random offsets between zones or
     * accidentally leak beyond exact zero-based 1D frame boundaries. */
    c.effect_id=OT_WLED_COLOR_CLOUDS;
    c.speed=24u;c.intensity=32u;c.brightness=32u;
    ot_wled_fx_state_t cloud_a={0},cloud_b={0};
    ot_wled_rgb_t cloud_aa[33],cloud_bb[33];
    for(unsigned k=0u;k<60u;k++) {
        const uint32_t ms=k*1051u;
        for(size_t px=0;px<33u;px++) {
            cloud_aa[px]=(ot_wled_rgb_t){99,88,77};
            cloud_bb[px]=(ot_wled_rgb_t){99,88,77};
        }
        assert(ot_wled_fx_render(&c,&cloud_a,ms,&cloud_aa[1],31u));
        assert(ot_wled_fx_render(&c,&cloud_b,ms,&cloud_bb[1],31u));
        assert(memcmp(cloud_aa,cloud_bb,sizeof(cloud_aa))==0);
        assert(eq(cloud_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(cloud_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1u;px<=31u;px++)
            assert(cloud_aa[px].r<=32u &&
                   cloud_aa[px].g<=32u &&
                   cloud_aa[px].b<=32u);
    }
    ot_wled_fx_reset(&cloud_a);
    ot_wled_fx_reset(&cloud_b);


    /* Source particles: verify repeatability per zone, no cross-zone state,
     * bounded writes, ESP32 source star allocation and reset. */
    c.effect_id=OT_WLED_STARBURST;
    c.speed=255u;c.intensity=255u;c.brightness=32u;
    ot_wled_fx_state_t burst_a={0},burst_b={0};
    ot_wled_rgb_t burst_aa[33],burst_bb[33];
    for(unsigned k=0u;k<250u;k++) {
        const uint32_t ms=1u+k*171u; /* nonzero birth timestamps */
        for(size_t px=0;px<33u;px++) {
            burst_aa[px]=(ot_wled_rgb_t){99,88,77};
            burst_bb[px]=(ot_wled_rgb_t){99,88,77};
        }
        assert(ot_wled_fx_render(&c,&burst_a,ms,&burst_aa[1],31u));
        assert(ot_wled_fx_render(&c,&burst_b,ms,&burst_bb[1],31u));
        assert(memcmp(burst_aa,burst_bb,sizeof(burst_aa))==0);
        assert(eq(burst_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(burst_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1u;px<=31u;px++)
            assert(burst_aa[px].r<=32u &&
                   burst_aa[px].g<=32u &&
                   burst_aa[px].b<=32u);
    }
    assert(burst_a.scratch && burst_a.scratch_bytes==4u*60u);
    ot_wled_fx_reset(&burst_a);
    ot_wled_fx_reset(&burst_b);
    assert(!burst_a.scratch && !burst_b.scratch);


    /* Tetrix: the single-zone 20-byte falling-stack state is independent,
     * reuses persistent frames, and never reaches frame guard pixels. */
    c.effect_id=OT_WLED_TETRIX;
    c.speed=255u;c.intensity=255u;c.brightness=32u;
    ot_wled_fx_state_t tetr_a={0},tetr_b={0};
    ot_wled_rgb_t tetr_aa[33],tetr_bb[33];
    for(size_t px=0;px<33u;px++) {
        tetr_aa[px]=(ot_wled_rgb_t){99,88,77};
        tetr_bb[px]=(ot_wled_rgb_t){99,88,77};
    }
    for(unsigned k=0u;k<240u;k++) {
        const uint32_t ms=k*61u;
        tetr_aa[0]=tetr_aa[32]=(ot_wled_rgb_t){99,88,77};
        tetr_bb[0]=tetr_bb[32]=(ot_wled_rgb_t){99,88,77};
        assert(ot_wled_fx_render(&c,&tetr_a,ms,&tetr_aa[1],31u));
        assert(ot_wled_fx_render(&c,&tetr_b,ms,&tetr_bb[1],31u));
        assert(memcmp(tetr_aa,tetr_bb,sizeof(tetr_aa))==0);
        assert(eq(tetr_aa[0],(ot_wled_rgb_t){99,88,77}));
        assert(eq(tetr_aa[32],(ot_wled_rgb_t){99,88,77}));
        for(size_t px=1u;px<=31u;px++)
            assert(tetr_aa[px].r<=99u &&
                   tetr_aa[px].g<=88u &&
                   tetr_aa[px].b<=77u);
    }
    assert(tetr_a.scratch && tetr_a.scratch_bytes==20u);
    ot_wled_fx_reset(&tetr_a);
    ot_wled_fx_reset(&tetr_b);
    assert(!tetr_a.scratch && !tetr_b.scratch);

    /* Every source adapter must actually dispatch and honor zero brightness;
     * this keeps the release inventory from drifting into name-only claims. */
    c.brightness=0;
    for(size_t k=0;k<count;k++) {
        c.effect_id=ids[k];
        ot_wled_fx_reset(&s);
        for(size_t i=0;i<31;i++)p[i]=(ot_wled_rgb_t){99,99,99};
        assert(ot_wled_fx_render(&c,&s,0,p,31));
        for(size_t i=0;i<31;i++)assert(eq(p[i],(ot_wled_rgb_t){0,0,0}));
    }
    printf("CORE source-derived effects: %zu modes, timing, zone limits and validation passed.\n",count);
    return 0;
}
