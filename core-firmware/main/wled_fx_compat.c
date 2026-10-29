/*
 * Isolated adaptation of WLED wled00/FX.cpp:
 * mode_static(), blink(), mode_blink(), mode_strobe(),
 * color_wipe(), mode_color_wipe(), mode_color_sweep(), mode_fade(),
 * scan(), mode_scan(), mode_dual_scan(), running(), mode_theater_chase(),
 * mode_static_pattern(), mode_heartbeat(), spots_base(),
 * mode_spots(), mode_spots_fade(), mode_percent(),
 * mode_chase_flash(), mode_running_color(), mode_rainbow(),
 * mode_rainbow_cycle(), mode_theater_chase_rainbow(),
 * mode_blink_rainbow(), mode_strobe_rainbow(), mode_multi_strobe(),
 * chase(), mode_two_dots(), tricolor_chase(), mode_tricolor_wipe(),
 * mode_tricolor_fade(), mode_breath(), running_base(),
 * mode_tri_static_pattern(), mode_colorful(), gradient_base(),
 * and mode_sinewave(),
 * plus pinned wled_math.cpp sine helpers.
 *
 * WLED: copyright (c) 2016 Harm Aldick and individual WLED contributors.
 * Licensed under EUPL v. 1.2 or later; see third_party/wled/LICENSE.
 * Original: https://github.com/wled/WLED/blob/20e7a717664295eefc6ad77972cee1888d2da072/wled00/FX.cpp
 * This port omits WLED palette handling and uses direct foreground/background
 * for palette_id=0 only. No pixel I/O, allocations, Arduino or ESP-IDF dependencies.
 */
#include "wled_fx_compat.h"
#include <string.h>

static const ot_wled_fx_metadata_t supported[] = {
    {OT_WLED_SOLID,  "Solid",  "Solid",                         true, false, false, false, false},
    {OT_WLED_BLINK,  "Blink",  "Blink@!,Duty cycle;!,!;!;01",   true, true, true, true, false},
    {OT_WLED_WIPE,   "Wipe",   "Wipe@!,!;!,!;!",                true, true, true, true, false},
    {OT_WLED_SWEEP,  "Sweep",  "Sweep@!,!;!,!;!",              true, true, true, true, false},
    {OT_WLED_SCAN,   "Scan",   "Scan@!,# of dots,,,,,Overlay;!,!,!;!", true, true, true, true, false},
    {OT_WLED_DUAL_SCAN, "Scan Dual", "Scan Dual@!,# of dots,,,,,Overlay;!,!,!;!", true, true, true, true, false},
    {OT_WLED_FADE,   "Fade",   "Fade@!;!,!;!;01",              true, true, true, false, false},
    {OT_WLED_THEATER, "Theater", "Theater@!,Gap size;!,!;!", true, true, true, true, false},
    {OT_WLED_STROBE, "Strobe", "Strobe@!;!,!;!;01",            true, true, true, false, false},
    {OT_WLED_CHASE_FLASH, "Chase Flash", "Chase Flash@!;Bg,Fx;!",
     true, true, true, false, false},
    {OT_WLED_RUNNING_COLOR, "Chase 2", "Chase 2@!,Width;!,!;!",
     true, true, true, true, false},
    {OT_WLED_SOLID_PATTERN, "Solid Pattern", "Solid Pattern@Fg size,Bg size;Fg,!;!;;pal=0",
     true, true, true, true, false},
    {OT_WLED_SPOTS, "Spots", "Spots@Spread,Width,,,,,Overlay;!,!;!", 
     true, true, true, true, false},
    {OT_WLED_SPOTS_FADE, "Spots Fade", "Spots Fade@Spread,Width,,,,,Overlay;!,!;!",
     true, true, true, true, false},
    {OT_WLED_PERCENT, "Percent", "Percent@!,% of fill,,,,One color;!,!;!",
     true, true, true, true, false},
    {OT_WLED_HEARTBEAT, "Heartbeat", "Heartbeat@!,!;!,!;!;01;m12=1",
     true, true, true, true, false},
    /* Source: pinned FX.cpp mode_rainbow(), mode_rainbow_cycle(),
     * mode_theater_chase_rainbow(), mode_strobe_rainbow(), mode_blink_rainbow(). */
    {OT_WLED_COLORLOOP, "Colorloop", "Colorloop@!,Saturation;;!;01",
     true, false, true, true, false},
    {OT_WLED_RAINBOW, "Rainbow", "Rainbow@!,Size;;!",
     false, false, true, true, false},
    {OT_WLED_THEATER_RAINBOW, "Theater Rainbow", "Theater Rainbow@!,Gap size;,!;!",
     false, true, true, true, false},
    {OT_WLED_STROBE_RAINBOW, "Strobe Rainbow", "Strobe Rainbow@!;,!;!;01",
     false, true, true, false, false},
    {OT_WLED_BLINK_RAINBOW, "Blink Rainbow", "Blink Rainbow@Frequency,Blink duration;!,!;!;01",
     false, true, true, true, false},
    {OT_WLED_MULTI_STROBE, "Strobe Mega", "Strobe Mega@!,!;!,!;!;01",
     true, true, true, true, false},
    {OT_WLED_CHASE_COLOR, "Chase", "Chase@!,Width;!,!,!;!",
     true, true, true, true, false},
    {OT_WLED_CHASE_RAINBOW, "Chase Rainbow", "Chase Rainbow@!,Width;!,!;!",
     true, true, true, true, false},
    {OT_WLED_CHASE_RAINBOW_WHITE, "Rainbow Runner", "Rainbow Runner@!,Size;Bg;!",
     true, true, true, true, false},
    {OT_WLED_TWO_DOTS, "Two Dots", "Two Dots@!,Dot size,,,,,Overlay;1,2,Bg;!",
     true, true, true, true, false},
    {OT_WLED_TRICOLOR_CHASE, "Chase 3", "Chase 3@!,Size;1,2,3;!",
     true, true, true, true, false},
    {OT_WLED_TRICOLOR_WIPE, "Tri Wipe", "Tri Wipe@!;1,2,3;!",
     true, true, true, false, false},
    {OT_WLED_TRICOLOR_FADE, "Tri Fade", "Tri Fade@!;1,2,3;!",
     true, true, true, false, false},
    {OT_WLED_BREATHE, "Breathe", "Breathe@!;!,!;!;01",
     true, true, true, false, false},
    {OT_WLED_RUNNING_LIGHTS, "Running", "Running@!,Wave width;!,!;!",
     true, true, true, true, false},
    {OT_WLED_SAW, "Saw", "Saw@!,Width;!,!;!",
     true, true, true, true, false},
    {OT_WLED_RUNNING_DUAL, "Running Dual", "Running Dual@!,Wave width;L,!,R;!",
     true, true, true, true, false},
    {OT_WLED_SOLID_PATTERN_TRI, "Solid Pattern Tri", "Solid Pattern Tri@,Size;1,2,3;;;pal=0",
     true, true, false, true, false},
    {OT_WLED_COLORFUL, "Colorful", "Colorful@!,Saturation;1,2,3;!",
     true, true, true, true, false},
    {OT_WLED_GRADIENT, "Gradient", "Gradient@!,Spread;!,!;!;;ix=16",
     true, true, true, true, false},
    {OT_WLED_LOADING, "Loading", "Loading@!,Fade;!,!;!;;ix=16",
     true, true, true, true, false},
    {OT_WLED_SINEWAVE, "Sine", "Sine@!,Scale;;!",
     true, true, true, true, false},
    {OT_WLED_ANDROID, "Android", "Android@!,Width;!,!;!;;m12=1",
     true, true, true, true, false},
    {OT_WLED_WIPE_RANDOM, "Wipe Random", "Wipe Random@!;;!",
     false, false, true, true, false},
    {OT_WLED_RANDOM_COLORS, "Random Colors", "Random Colors@!,Fade time;;!;01",
     false, false, true, true, false},
    {OT_WLED_SPARKLE, "Sparkle", "Sparkle@!,,,,,,Overlay;!,!;!;;m12=0",
     true, true, true, false, false},
    {OT_WLED_SPARKLE_DARK, "Sparkle Dark", "Sparkle Dark@!,!,,,,,Overlay;Bg,Fx;!;;m12=0",
     true, true, true, true, false},
    {OT_WLED_SPARKLE_PLUS, "Sparkle+", "Sparkle+@!,!,,,,,Overlay;Bg,Fx;!;;m12=0",
     true, true, true, true, false},
    {OT_WLED_TRAFFIC_LIGHT, "Traffic Light", "Traffic Light@!,US style;,!;!",
     false, true, true, true, false},
    {OT_WLED_SWEEP_RANDOM, "Sweep Random", "Sweep Random@!;;!",
     false, false, true, true, false},
    {OT_WLED_SOLID_GLITTER, "Solid Glitter", "Solid Glitter@,!;Bg,,Glitter color;;;m12=0",
     true, true, false, true, false},
    {OT_WLED_CHASE_RANDOM, "Chase Random", "Chase Random@!,Width;!,,!;!",
     true, true, true, true, false},
    {OT_WLED_STREAM, "Stream", "Stream@!,Width;;!",
     false, false, true, true, false},
    {OT_WLED_STREAM2, "Stream 2", "Stream 2@!,Width;;!",
     false, false, true, true, false},
    {OT_WLED_LIGHTNING, "Lightning", "Lightning@!,!;!,!;!",
     true, true, true, true, false},
    {OT_WLED_ICU, "ICU", "ICU@!,Eye spacing;!,!;!",
     true, true, true, true, false},
    {OT_WLED_FIRE_FLICKER, "Fire Flicker", "Fire Flicker@!,!;!,!;!",
     true, false, true, true, false},
    {OT_WLED_WAVESINS, "Wavesins", "Wavesins@!,!;;!",
     true, false, true, true, false},
    {OT_WLED_BPM, "Bpm", "Bpm@!,!;;!",
     true, false, true, false, false},
    {OT_WLED_LAKE, "Lake", "Lake@!,!;;!",
     true, false, true, true, false},
    {OT_WLED_PLASMA, "Plasma", "Plasma@!,!;;!",
     true, false, true, true, false},
    {OT_WLED_OSCILLATE, "Oscillate", "Oscillate",
     true, true, true, true, false},
    {OT_WLED_SCANNER, "Scanner", "Scanner@!,Trail,Delay,,,Dual,Bi-delay;!,!,!;!;;m12=0,c1=0",
     true, true, true, true, false},
    {OT_WLED_LIGHTHOUSE, "Lighthouse", "Lighthouse@!,Trail;!,!;!",
     true, true, true, true, false},
    {OT_WLED_SCANNER_DUAL, "Scanner Dual", "Scanner Dual@!,Trail,Delay,,,Dual,Bi-delay;!,!,!;!;;m12=0,c1=0",
     true, true, true, true, false},
    {OT_WLED_SINELON, "Sinelon", "Sinelon@!,Trail;!,!;!",
     true, true, true, true, false},
    {OT_WLED_SINELON_DUAL, "Sinelon Dual", "Sinelon Dual@!,Trail;!,!;!",
     true, true, true, true, false},
    {OT_WLED_SINELON_RAINBOW, "Sinelon Rainbow", "Sinelon Rainbow@!,Trail;!,!;!",
     false, true, true, true, false}
};

bool ot_wled_fx_metadata(uint16_t id, ot_wled_fx_metadata_t *out) {
    if (!out) return false;
    for (size_t i=0; i<sizeof(supported)/sizeof(supported[0]); ++i) {
        if (supported[i].id==id) { *out=supported[i]; return true; }
    }
    return false;
}

size_t ot_wled_fx_supported_ids(uint16_t *dest,size_t max_ids){
    const size_t count=sizeof(supported)/sizeof(supported[0]);
    if(dest)for(size_t i=0;i<count&&i<max_ids;i++)dest[i]=supported[i].id;
    return count;
}

void ot_wled_fx_reset(ot_wled_fx_state_t *state) {
    if (state) {
        memset(state,0,sizeof(*state));
        state->rng_state=0x73A11C5Du;
    }
}

static ot_wled_rgb_t scaled(ot_wled_rgb_t c, uint8_t brightness) {
    return (ot_wled_rgb_t){
        (uint8_t)((((uint32_t)c.r*brightness)+127u)/255u),
        (uint8_t)((((uint32_t)c.g*brightness)+127u)/255u),
        (uint8_t)((((uint32_t)c.b*brightness)+127u)/255u)
    };
}

/*
 * Exact palette=0, RGB-only color_wheel arithmetic from pinned WLED
 * FX_fcn.cpp -> CHSV32 -> fastled_slim/fastled_slim.cpp hsv2rgb_rainbow.
 * WLED's raw wheel uses 8-bit hue in a 16-bit HSV representation.
 * No Arduino, palette, gamma or external runtime is used here.
 */
static ot_wled_rgb_t wheel_rgb(uint8_t hue) {
    const uint32_t offset=((uint32_t)hue<<8)&0x1fffu;
    const uint32_t thirds=offset*21846u;
    const uint8_t third=(uint8_t)(thirds>>21);
    uint8_t r,g,b;
    if (!(hue&0x80u)) {
        if (!(hue&0x40u)) {
            if (!(hue&0x20u)) { r=(uint8_t)(255u-third);g=third;b=0; }
            else {r=171;g=(uint8_t)(85u+third);b=0;}
        } else {
            if (!(hue&0x20u)) {
                const uint8_t twice=(uint8_t)(thirds>>20);
                r=(uint8_t)(171u-twice);g=(uint8_t)(170u+third);b=0;
            } else {r=0;g=(uint8_t)(255u-third);b=third;}
        }
    } else {
        if (!(hue&0x40u)) {
            if (!(hue&0x20u)) {
                const uint8_t twice=(uint8_t)(thirds>>20);
                r=0;g=(uint8_t)(171u-twice);b=(uint8_t)(85u+twice);
            } else {r=third;g=0;b=(uint8_t)(255u-third);}
        } else {
            if (!(hue&0x20u)) {
                r=(uint8_t)(85u+third);g=0;b=(uint8_t)(171u-third);
            } else {
                r=(uint8_t)(170u+third);g=0;b=(uint8_t)(85u-third);
            }
        }
    }
    return (ot_wled_rgb_t){r,g,b};
}

/* WLED color_blend(c1,c2,blend) arithmetic from the pinned FX/colors.cpp
 * version. Even blend=0 contributes 1/256 of c2. No palette or gamma here. */
static uint8_t mix_channel(uint8_t a, uint8_t b, uint8_t blend) {
    return (uint8_t)(((uint32_t)a*(256u-(uint32_t)blend)+
                     (uint32_t)b*(1u+(uint32_t)blend))>>8);
}
static ot_wled_rgb_t blend_colors(ot_wled_rgb_t a, ot_wled_rgb_t b, uint8_t blend) {
    return (ot_wled_rgb_t){mix_channel(a.r,b.r,blend),
                           mix_channel(a.g,b.g,blend),mix_channel(a.b,b.b,blend)};
}

/* Exact pinned fastled_slim.cpp triwave16(): ascending 0..65534;
 * descending 65535..1. The second half is intentionally one LSB above
 * the old mirrored shortcut, which affected Fade and Spots golden frames. */
static uint16_t wave16(uint16_t x) {
    if (x<0x8000u) return (uint16_t)((uint32_t)x*2u);
    return (uint16_t)(0xffffu-(((uint32_t)x-0x8000u)*2u));
}

/* WLED FX.cpp color_wipe(false/true,false), with direct primary/background
 * colors only. A length-one segment falls back to Solid upstream.
 * All intermediary arithmetic uses explicit width to avoid host-size drift. */
static void wipe_frame(const ot_wled_fx_config_t *cfg, uint32_t time_ms,
                       ot_wled_rgb_t *pixels, size_t count, bool sweep) {
    if (count<=1) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const uint32_t cycle=750u+(255u-(uint32_t)cfg->speed)*150u;
    const uint32_t progress=(uint32_t)(((uint64_t)(time_ms%cycle)*65535u)/cycle);
    const bool back=progress>32767u;
    const uint32_t half_progress=back?progress-32767u:progress;
    const size_t led_index=(size_t)(((uint64_t)half_progress*count)>>15);
    const uint16_t rem16=(uint16_t)((uint64_t)half_progress*count*2u);
    uint32_t mix=rem16/((uint32_t)cfg->intensity+1u);
    if(mix>255u)mix=255u;
    const ot_wled_rgb_t fg=cfg->foreground,bg=cfg->background;
    for(size_t i=0;i<count;i++){
        const size_t out=(sweep&&back)?count-1u-i:i;
        ot_wled_rgb_t c;
        if(i<led_index)c=back?bg:fg;
        else {
            c=back?fg:bg;
            if(i==led_index)c=blend_colors(back?fg:bg,back?bg:fg,(uint8_t)mix);
        }
        pixels[out]=scaled(c,cfg->brightness);
    }
}

/*
 * Source adaptation: pinned upstream FX.cpp scan(bool dual).
 * The OpenTail zone buffer is a WLED virtual 1D segment (not a physical bus);
 * only the upstream default Overlay=false and palette=0 cases are supported.
 */
static void scan_frame(const ot_wled_fx_config_t *cfg, uint32_t now,
                       ot_wled_rgb_t *pixels, size_t count, bool dual) {
    if (count==1u) { pixels[0]=scaled(cfg->foreground,cfg->brightness); return; }
    const uint32_t cycle=750u+(255u-(uint32_t)cfg->speed)*150u;
    const uint32_t progress=(uint32_t)(((uint64_t)(now%cycle)*65535u)/cycle);
    const size_t size=1u+(((size_t)cfg->intensity*count)>>9);
    const size_t travel=count-size;
    const size_t index=(size_t)(((uint64_t)progress*(2u*travel))>>16);
    const size_t offset=index>travel?index-travel:travel-index;
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for (size_t i=0;i<count;i++) pixels[i]=bg;
    for (size_t j=offset;j<offset+size && j<count;j++) {
        pixels[j]=fg;
        if (dual) pixels[count-1u-j]=fg;
    }
}

/*
 * Source adaptation: pinned upstream FX.cpp running(..., theatre=true).
 * WLED renders with the OLD aux0, then increments it once when the cycle
 * advances. This matters when frames skip cycles.
 */
static void theater_frame(const ot_wled_fx_config_t *cfg, ot_wled_fx_state_t *state,
                          uint32_t now, ot_wled_rgb_t *pixels, size_t count,
                          bool rainbow) {
    const uint32_t width=3u+((uint32_t)cfg->intensity>>4);
    const uint32_t iteration=now/(50u+255u-(uint32_t)cfg->speed);
    const ot_wled_rgb_t fg=scaled(rainbow ? wheel_rgb((uint8_t)state->step) : cfg->foreground,
                                  cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for (size_t i=0;i<count;i++) pixels[i]=(i%width)==state->aux0?fg:bg;
    if (iteration!=state->step) {
        state->aux0=(uint16_t)((state->aux0+1u)%width);
        state->step=iteration;
    }
}

/* Source adaptation: pinned FX.cpp mode_static_pattern(), default palette. */
static void solid_pattern_frame(const ot_wled_fx_config_t *cfg,
                                ot_wled_rgb_t *pixels, size_t count) {
    const uint32_t lit=1u+(uint32_t)cfg->speed;
    const uint32_t unlit=1u+(uint32_t)cfg->intensity;
    uint32_t run=0;
    bool drawing_lit=true;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for (size_t i=0;i<count;i++) {
        pixels[i]=drawing_lit?fg:bg;
        if (++run >= (drawing_lit?lit:unlit)) {
            run=0; drawing_lit=!drawing_lit;
        }
    }
}

/* Source adaptation: pinned FX.cpp mode_heartbeat() state machine. */
static void heartbeat_frame(const ot_wled_fx_config_t *cfg,
                            ot_wled_fx_state_t *state,uint32_t now,
                            ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t bpm=40u+((uint32_t)cfg->speed>>3);
    const uint32_t ms_per_beat=60000u/bpm;
    const uint32_t second_beat=ms_per_beat/3u;
    const uint32_t beat_timer=now-state->step;
    uint32_t lower=((uint32_t)state->aux1*2042u)/
                   (2048u+(uint32_t)cfg->intensity);
    state->aux1=(uint16_t)lower;
    if (beat_timer>second_beat && state->aux0==0u) {
        state->aux1=UINT16_MAX; state->aux0=1;
    }
    if (beat_timer>ms_per_beat) {
        state->aux1=UINT16_MAX; state->aux0=0; state->step=now;
    }
    const uint8_t fade=(uint8_t)(255u-((uint32_t)state->aux1>>8));
    const ot_wled_rgb_t c=scaled(blend_colors(cfg->foreground,cfg->background,fade),
                                 cfg->brightness);
    for (size_t i=0;i<count;i++)pixels[i]=c;
}

/*
 * Pinned FX.cpp spots_base(), mode_spots(), mode_spots_fade().
 * Uses source triangle-wave, per-zone positioning and 8-bit color_blend.
 * Only the upstream default overlay=false and palette=0 are available.
 */
static void spots_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                        ot_wled_rgb_t *pixels,size_t count,bool fade) {
    if (count==1u) { pixels[0]=scaled(cfg->foreground,cfg->brightness); return; }
    const ot_wled_rgb_t fg=cfg->foreground,bg=cfg->background;
    uint32_t threshold=(255u-(uint32_t)cfg->speed)<<8;
    if (fade) {
        const uint32_t counter=now*((uint32_t)(cfg->speed>>2)+8u);
        const uint32_t triangle=wave16((uint16_t)counter);
        threshold=(triangle>>1)+(triangle>>2);
    }
    const uint32_t max_zones=(uint32_t)count>>2;
    const uint32_t zones=1u+(((uint32_t)cfg->intensity*max_zones)>>8);
    const uint32_t zone_len=(uint32_t)count/zones;
    const uint32_t offset=((uint32_t)count-zones*zone_len)>>1;
    const ot_wled_rgb_t back=scaled(bg,cfg->brightness);
    for (size_t i=0;i<count;i++) pixels[i]=back;
    for (uint32_t z=0;z<zones;z++) {
        const uint32_t pos=offset+z*zone_len;
        for (uint32_t i=0;i<zone_len;i++) {
            const uint16_t counter=(uint16_t)(((uint64_t)i*65535u)/zone_len);
            const uint32_t wave=wave16(counter);
            if (wave>threshold) {
                const uint32_t index=pos+i;
                const uint32_t amount=((wave-threshold)*255u)/(65535u-threshold);
                pixels[index]=scaled(blend_colors(fg,bg,(uint8_t)(255u-amount)),
                                     cfg->brightness);
            }
        }
    }
}

/*
 * Pinned FX.cpp mode_percent() with default "One color" option disabled.
 * Upstream displays previous SEGENV.aux1 fill then advances toward the target.
 * CORE keeps aux1 per output/zone; no physical output or mask is hardcoded.
 */
static void percent_frame(const ot_wled_fx_config_t *cfg,ot_wled_fx_state_t *state,
                          ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t percent=cfg->intensity>200u?200u:cfg->intensity;
    const uint32_t fraction=percent<100u?percent:200u-percent;
    const uint32_t active=(uint32_t)(((uint64_t)count*fraction+50u)/100u);
    uint32_t rate=1u+(((uint32_t)cfg->speed*(uint32_t)count)>>11);
    if (cfg->speed==255u) rate=255u;
    const uint32_t filled=state->aux1>count?(uint32_t)count:state->aux1;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for (size_t i=0;i<count;i++)
        pixels[i]=(percent<=100u ? i<filled : i>=count-filled)?fg:bg;
    if (active>filled) {
        const uint32_t next=filled+rate;
        state->aux1=(uint16_t)(next>active?active:next);
    } else if (active<filled) {
        const uint32_t next=filled>rate?filled-rate:0u;
        state->aux1=(uint16_t)(next<active?active:next);
    }
}

/*
 * Pinned FX.cpp running(..., theatre=false), mode_running_color().
 * Uses the same SEGENV step/aux0 phase as Theater; the width differs.
 * Only WLED's palette=0/direct-color case is supported.
 */
static void running_color_frame(const ot_wled_fx_config_t *cfg,ot_wled_fx_state_t *state,
                                uint32_t now,ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t width=1u+((uint32_t)cfg->intensity>>4);
    const uint32_t cycle=50u+255u-(uint32_t)cfg->speed;
    const uint32_t iteration=now/cycle;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for (size_t i=0;i<count;i++) {
        const int32_t pos=(int32_t)(i%(width<<1));
        const int32_t phase=(int32_t)state->aux0;
        const int32_t w=(int32_t)width;
        pixels[i]=(pos<phase-w || (pos>=phase && pos<phase+w))?fg:bg;
    }
    if (iteration!=state->step) {
        state->aux0=(uint16_t)((state->aux0+1u)%(width<<1));
        state->step=iteration;
    }
}

/*
 * Pinned FX.cpp mode_chase_flash(): draw the current flash_step BEFORE
 * incrementing aux1; the current position advances only after eight flashes.
 * No segment or LED bus is owned by this source adaptation.
 */
static void chase_flash_frame(const ot_wled_fx_config_t *cfg,ot_wled_fx_state_t *state,
                              uint32_t now,ot_wled_rgb_t *pixels,size_t count) {
    if (count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const bool advance=now>=state->step;
    const uint32_t flash_step=state->aux1%9u; /* 4 on/off flashes + move */
    if (advance) state->aux1++;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for (size_t i=0;i<count;i++) pixels[i]=fg;
    const size_t n=state->aux0%count;
    const size_t m=(n+1u)%count;
    uint32_t delay=10u+((30u*(255u-(uint32_t)cfg->speed))/(uint32_t)count);
    if (flash_step<8u) {
        if ((flash_step&1u)==0u) {
            pixels[n]=bg; pixels[m]=bg;
            delay=20u;
        } else {
            delay=30u;
        }
    } else if (advance) {
        state->aux0=(uint16_t)m;
    }
    if (advance) state->step=now+delay;
}

/*
 * Pinned FX.cpp mode_multi_strobe() timing. The old SEGENV.aux1 phase
 * is drawn before advancing, including the intentional unsmoothed edges.
 */
static void multi_strobe_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t n=2u*((uint32_t)(cfg->intensity/10u)+1u);
    state->aux0=(uint16_t)(50u+20u*(255u-(uint32_t)cfg->speed));
    ot_wled_rgb_t color=cfg->background;
    if(state->aux1<n) {
        if((state->aux1&1u)==0u) {
            color=cfg->foreground;
            state->aux0=15u;
        } else state->aux0=50u;
    }
    const ot_wled_rgb_t shown=scaled(color,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=shown;
    if(now-(uint32_t)state->aux0>state->step) {
        state->aux1++;
        if(state->aux1>n)state->aux1=0;
        state->step=now;
    }
}

/*
 * Pinned FX.cpp chase() used by mode_chase_color(), mode_chase_rainbow(),
 * mode_chase_rainbow_white(). The original computes the rainbow hues from
 * OLD SEGENV.step and call before writing the new chase position.
 * The CORE 2-color profile uses WLED's default third color RGB black.
 */
static void chase_frame(const ot_wled_fx_config_t *cfg,
                        ot_wled_fx_state_t *state,uint32_t now,
                        uint32_t call,ot_wled_rgb_t *pixels,size_t count) {
    ot_wled_rgb_t base=cfg->background;
    ot_wled_rgb_t c2=cfg->foreground,c3=cfg->foreground;
    if(cfg->effect_id==OT_WLED_CHASE_RAINBOW) {
        uint32_t sep=256u/(uint32_t)count;
        if(sep==0u)sep=1u;
        base=wheel_rgb((uint8_t)((state->step*sep+call)&255u));
        c3=cfg->background;
    } else if(cfg->effect_id==OT_WLED_CHASE_RAINBOW_WHITE) {
        const uint32_t n=state->step,m=(n+1u)%(uint32_t)count;
        base=cfg->foreground;
        c2=wheel_rgb((uint8_t)(((n*256u/(uint32_t)count)+call)&255u));
        c3=wheel_rgb((uint8_t)(((m*256u/(uint32_t)count)+call)&255u));
    }
    const uint16_t counter=(uint16_t)(now*((uint32_t)(cfg->speed>>2)+1u));
    const size_t a=((uint32_t)counter*(uint32_t)count)>>16;
    const size_t width=1u+(((uint32_t)cfg->intensity*(uint32_t)count)>>10);
    state->step=(uint32_t)a;
    const ot_wled_rgb_t bg=scaled(base,cfg->brightness);
    const ot_wled_rgb_t fg=scaled(c2,cfg->brightness);
    const ot_wled_rgb_t tail=scaled(c3,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    for(size_t j=0;j<width;j++)pixels[(a+j)%count]=fg;
    for(size_t j=0;j<width;j++)pixels[(a+width+j)%count]=tail;
}

/* Source: pinned FX.cpp mode_two_dots(), default Overlay=false, RGB 3rd=black. */
static void two_dots_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                           ot_wled_rgb_t *pixels,size_t count) {
    if(count==1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    const uint32_t delay=1u+(((uint32_t)cfg->frame_time_ms<<3)/(uint32_t)count);
    const uint32_t period=(delay<<4)-
        ((uint32_t)cfg->speed*((delay<<4)-delay))/255u;
    const size_t offset=(now/period)%count;
    size_t width=(((uint32_t)count*((uint32_t)cfg->intensity+1u))>>9);
    if(!width)width=1;
    const bool black_bg=cfg->background.r==0 && cfg->background.g==0 &&
                        cfg->background.b==0;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t other=scaled(black_bg?cfg->foreground:cfg->background,
                                   cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=(ot_wled_rgb_t){0,0,0};
    for(size_t j=0;j<width;j++) {
        pixels[(offset+j)%count]=fg;
        pixels[(offset+j+count/2u)%count]=other;
    }
}

/* Source: pinned FX.cpp tricolor_chase(); third color defaults to black. */
static void tricolor_chase_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                                 ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t cycle=50u+((255u-(uint32_t)cfg->speed)<<1);
    const size_t width=1u+((size_t)cfg->intensity>>4);
    size_t idx=(now/cycle)%(width*3u);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++) {
        ot_wled_rgb_t c={0,0,0};
        if(idx>width*2u-1u)c=bg;
        else if(idx>width-1u)c=fg;
        pixels[count-1u-i]=c;
        if(++idx>=width*3u)idx=0;
    }
}

/* Source: pinned FX.cpp mode_tricolor_wipe(), third RGB color defaults black. */
static void tricolor_wipe_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                                ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t cycle=1000u+(255u-(uint32_t)cfg->speed)*200u;
    const uint32_t prog=(uint32_t)(((uint64_t)(now%cycle)*65535u)/cycle);
    const size_t index=(size_t)(((uint64_t)prog*(uint32_t)count*3u)>>16);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++) pixels[i]=(ot_wled_rgb_t){0,0,0};
    if(index<count) {
        for(size_t i=0;i<count;i++)pixels[i]=i>index?fg:bg;
    } else if(index<count*2u) {
        const size_t offset=index-count;
        for(size_t i=offset+1u;i<count;i++)pixels[i]=bg;
    } else {
        const size_t offset=index-count*2u;
        for(size_t i=0;i<count&&i<=offset;i++)pixels[i]=fg;
    }
}

/* Source: pinned FX.cpp mode_tricolor_fade(), default third RGB black. */
static void tricolor_fade_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                                ot_wled_rgb_t *pixels,size_t count) {
    const uint16_t t=(uint16_t)(now*((uint32_t)(cfg->speed>>3)+1u));
    const uint32_t progress=((uint32_t)t*768u)>>16;
    const uint8_t fraction=(uint8_t)progress;
    const ot_wled_rgb_t black={0,0,0};
    ot_wled_rgb_t a,b;
    if(progress<256u){a=cfg->foreground;b=cfg->background;}
    else if(progress<512u){a=cfg->background;b=black;}
    else {a=black;b=cfg->foreground;}
    const ot_wled_rgb_t c=scaled(blend_colors(a,b,fraction),cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=c;
}

/*
 * Pinned WLED wled_math.cpp sin16_t/sin8_t: Bhaskara-I integer math,
 * not a platform sine approximation. Never use floating-point per pixel.
 */
static int16_t wled_sin16(uint16_t theta) {
    int sign=1;
    if(theta>0x7fffu) {
        theta=(uint16_t)(0xffffu-theta);
        sign=-1;
    }
    const uint32_t precal=(uint32_t)theta*(0x7fffu-(uint32_t)theta);
    const uint64_t num=(uint64_t)precal*(4u*0x7fffu);
    const uint32_t denom=1342095361u-precal;
    const int16_t sine=(int16_t)(num/denom);
    return (int16_t)(sign*(int32_t)sine);
}
static uint8_t wled_sin8(uint8_t theta) {
    int32_t v=(int32_t)wled_sin16((uint16_t)theta*257u)+0x7fff+128;
    if(v>0xffff)v=0xffff;
    return (uint8_t)((uint32_t)v>>8);
}
static uint8_t wled_sin_gap(uint16_t angle) {
    if(angle&0x100u)return 0;
    return wled_sin8((uint8_t)(angle+192u));
}

/* Pinned FX.cpp mode_breath(): exactly the integer counter, mirror and
 * var/103 brightness ramp. Palette=0 uses foreground/background colors.
 */
static void breathe_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                          ot_wled_rgb_t *pixels,size_t count) {
    uint32_t v=0;
    uint32_t counter=(now*((uint32_t)(cfg->speed>>3)+10u))&0xffffu;
    counter=(counter>>2)+(counter>>4);
    if(counter<16384u) {
        if(counter>8192u)counter=8192u-(counter-8192u);
        v=(uint32_t)wled_sin16((uint16_t)counter)/103u;
    }
    const uint8_t lum=(uint8_t)(30u+v);
    const ot_wled_rgb_t c=scaled(blend_colors(cfg->background,
                                              cfg->foreground,lum),
                                 cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=c;
}

/*
 * Pinned FX.cpp running_base(saw,dual), palette=0 with the third RGB slot
 * at default black. WLED uses unsigned phase wrapping and signed map()
 * interpolation in Saw; emulate both with explicit integer widths.
 */
static void running_sine_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                               ot_wled_rgb_t *pixels,size_t count,
                               bool saw,bool dual) {
    const uint32_t x_scale=(uint32_t)cfg->intensity>>2;
    const uint32_t counter=(now*(uint32_t)cfg->speed)>>9;
    const ot_wled_rgb_t black={0,0,0};
    for(size_t i=0;i<count;i++) {
        uint32_t a=(uint32_t)i*x_scale-counter;
        if(saw) {
            a&=0xffu;
            if(a<16u) a=192u+a*8u;
            else a=64u+((a-16u)*128u)/239u;
            a=255u-a;
        }
        const uint8_t wave=dual?wled_sin_gap((uint16_t)a):
                                wled_sin8((uint8_t)a);
        ot_wled_rgb_t c=blend_colors(cfg->background,cfg->foreground,wave);
        if(dual) {
            const uint32_t b=((uint32_t)count-1u-(uint32_t)i)*x_scale-counter;
            const uint8_t r=wled_sin_gap((uint16_t)b);
            const ot_wled_rgb_t other=blend_colors(cfg->background,black,r);
            c=blend_colors(c,other,127u);
        }
        pixels[i]=scaled(c,cfg->brightness);
    }
}

/* Pinned FX.cpp mode_tri_static_pattern(), third RGB slot default black. */
static void tri_pattern_frame(const ot_wled_fx_config_t *cfg,
                              ot_wled_rgb_t *pixels,size_t count) {
    const size_t width=((size_t)cfg->intensity>>5)+1u;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++) {
        const size_t phase=(i/width)%3u;
        pixels[i]=phase==0u?fg:(phase==1u?bg:(ot_wled_rgb_t){0,0,0});
    }
}

/* Pinned FX.cpp mode_colorful(), default palette=0. The high-intensity
 * WLED third RGB color is black in current CORE's two-color profile.
 * Rotate the source four/pastel/three-color arrays once per cycle. */
static void colorful_frame(const ot_wled_fx_config_t *cfg,
                           ot_wled_fx_state_t *state,uint32_t now,
                           ot_wled_rgb_t *pixels,size_t count) {
    size_t colors=4u;
    ot_wled_rgb_t choices[4]={
        {255,0,0},{238,187,0},{0,238,0},{0,119,204}
    };
    if(cfg->intensity>160u) {
        colors=3u;
        choices[0]=cfg->foreground;
        choices[1]=cfg->background;
        choices[2]=(ot_wled_rgb_t){0,0,0};
    } else if(cfg->intensity<80u) {
        choices[0]=(ot_wled_rgb_t){255,128,64};
        choices[1]=(ot_wled_rgb_t){229,210,65};
        choices[2]=(ot_wled_rgb_t){119,255,119};
        choices[3]=(ot_wled_rgb_t){119,240,240};
    }
    const uint32_t cycle=50u+8u*(255u-(uint32_t)cfg->speed);
    const uint32_t iteration=now/cycle;
    if(iteration!=state->step) {
        if(cfg->speed>0u)state->aux0++;
        if(state->aux0>=colors)state->aux0=0;
        state->step=iteration;
    }
    for(size_t i=0;i<count;i++)
        pixels[i]=scaled(choices[(state->aux0+(i%colors))%colors],
                         cfg->brightness);
}

/* Pinned FX.cpp gradient_base(loading). In the pinned source,
 * '1 + loading ? intensity/2 : intensity/4' always takes intensity/2
 * because of operator precedence. Preserve it for source frame fidelity.
 */
static void gradient_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                           uint32_t call,ot_wled_rgb_t *pixels,size_t count,
                           bool loading) {
    if(count<=1u){
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const uint16_t counter=(uint16_t)(now*
                          ((uint32_t)(cfg->speed>>2)+1u));
    const int32_t pp=call==0u?0:
        (int32_t)(((uint32_t)counter*(uint32_t)count)>>16);
    const int32_t brd=(int32_t)cfg->intensity/2;
    const int32_t p1=pp-(int32_t)count,p2=pp+(int32_t)count;
    for(size_t i=0;i<count;i++) {
        const int32_t pos=(int32_t)i;
        int32_t d;
        if(loading) {
            const int32_t target=(pos>pp)?p2:pp;
            d=target>pos?target-pos:pos-target;
        } else {
            const int32_t a=pp>pos?pp-pos:pos-pp;
            const int32_t b=p1>pos?p1-pos:pos-p1;
            const int32_t c=p2>pos?p2-pos:pos-p2;
            d=a<b?a:b;
            if(c<d)d=c;
        }
        const uint8_t blend=(uint8_t)(brd>d?(d*255)/brd:255);
        pixels[i]=scaled(blend_colors(cfg->foreground,
                                      cfg->background,blend),
                         cfg->brightness);
    }
}

/*
 * Source adaptation: pinned WLED FX.cpp mode_sinewave() and pinned
 * fastled_slim/fastled_slim.cpp cubicwave8() / ease8InOutCubic().
 * The WLED palette=0 path always returns SEGCOLOR(0); CORE's RGB
 * two-color profile supplies SEGCOLOR(1) as background. No pixel I/O.
 */
static uint8_t wled_cubicwave8(uint8_t phase) {
    if (phase&0x80u) phase=(uint8_t)(255u-phase);
    const uint8_t triangle=(uint8_t)(phase<<1);
    const uint32_t square=(uint32_t)triangle*triangle;
    const uint32_t factor=(3u<<8)-((uint32_t)triangle<<1);
    return (uint8_t)((square*factor)>>16);
}
static void sinewave_frame(const ot_wled_fx_config_t *cfg,
                           ot_wled_fx_state_t *state,
                           ot_wled_rgb_t *pixels,size_t count) {
    state->step+=(uint32_t)cfg->speed/16u;
    const uint32_t frequency=(uint32_t)cfg->intensity/4u;
    for(size_t i=0;i<count;i++) {
        const uint8_t phase=(uint8_t)((uint32_t)i*frequency+state->step);
        const uint8_t amplitude=wled_cubicwave8(phase);
        pixels[i]=scaled(blend_colors(cfg->background,cfg->foreground,amplitude),
                         cfg->brightness);
    }
}

/*
 * Source: pinned FX.cpp mode_android(). The upstream effect allocates a
 * uint32_t frame counter in SEGENV.data per segment; CORE keeps that exact
 * state in its existing per-zone bank (never shared across outputs).
 * With palette=0, color_from_palette(...,1) is the secondary color.
 */
static void android_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          ot_wled_rgb_t *pixels,size_t count) {
    size_t size=(size_t)(state->aux1>>1);
    unsigned shrinking=state->aux1&1u;
    if(now>=state->step) {
        state->step=now+3u+
            (8u*(255u-(uint32_t)cfg->speed))/(uint32_t)count;
        if(size>((uint32_t)cfg->intensity*(uint32_t)count)/255u)
            shrinking=1u;
        else if(size<2u)
            shrinking=0u;
        if(!shrinking) {
            if(state->data_counter%3u==1u)state->aux0++;
            else size++;
        } else {
            state->aux0++;
            if(state->data_counter%3u!=1u)size--;
        }
        state->aux1=(uint16_t)((size<<1)|shrinking);
        state->data_counter++;
        if(state->aux0>=count)state->aux0=0u;
    }
    const size_t start=state->aux0;
    const size_t end=(start+size)%count;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++) {
        const bool lit=(start<end && i>=start && i<end) ||
                       (start>=end && (i>=start || i<end));
        pixels[i]=lit?fg:bg;
    }
}

/*
 * Randomized WLED FX ports. The effect maths and update order below are
 * adapted from pinned FX.cpp; the hardware RNG is represented by a per-zone
 * reproducible software stream until the full RNG injection API is ported.
 * This is SOURCE-DERIVED behavior, not yet a random-sequence parity claim.
 */
static uint32_t fx_prng(ot_wled_fx_state_t *s) {
    uint32_t x=s->rng_state;
    if(!x)x=0x73a11c5du;
    x^=x<<13;x^=x>>17;x^=x<<5;
    s->rng_state=x;
    return x;
}
static uint8_t fx_rand8(ot_wled_fx_state_t *s,uint32_t bound) {
    if(bound==0u)return 0u;
    return (uint8_t)(fx_prng(s)%bound);
}
static size_t fx_rand_index(ot_wled_fx_state_t *s,size_t count) {
    return (size_t)(fx_prng(s)%(uint32_t)count);
}
static uint8_t fx_new_wheel(ot_wled_fx_state_t *s,uint8_t prev) {
    /* Pinned util.cpp get_random_wheel_index: minimum circular distance 42. */
    for(unsigned attempts=0;attempts<256u;attempts++) {
        const uint8_t next=fx_rand8(s,256u);
        const unsigned delta=next>prev?(unsigned)(next-prev):(unsigned)(prev-next);
        if((delta<256u-delta?delta:256u-delta)>=42u)return next;
    }
    return (uint8_t)(prev+128u);
}
static void fx_random_wipe(const ot_wled_fx_config_t *cfg,
                           ot_wled_fx_state_t *state,uint32_t ms,
                           uint32_t old_call,ot_wled_rgb_t *pixels,
                           size_t count,bool reverse) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const uint32_t cycle=750u+(255u-(uint32_t)cfg->speed)*150u;
    const uint32_t progress=(uint32_t)(((uint64_t)(ms%cycle)*65535u)/cycle);
    const bool back=progress>32767u;
    const uint32_t prog=back?progress-32767u:progress;
    if(back) {
        if(state->step==0u)state->step=1u;
    } else {
        if(state->step==2u)state->step=3u;
    }
    if(old_call==0u) {
        state->aux0=fx_rand8(state,256u);
        state->step=3u;
    }
    if(state->step==1u) {
        state->aux1=fx_new_wheel(state,(uint8_t)state->aux0);
        state->step=2u;
    }
    if(state->step==3u) {
        state->aux0=fx_new_wheel(state,(uint8_t)state->aux1);
        state->step=0u;
    }
    const size_t led_index=(size_t)(((uint64_t)prog*count)>>15);
    const uint16_t r16=(uint16_t)((uint64_t)prog*count*2u);
    uint32_t rem=(uint32_t)r16/((uint32_t)cfg->intensity+1u);
    if(rem>255u)rem=255u;
    const ot_wled_rgb_t c1=wheel_rgb((uint8_t)state->aux1);
    const ot_wled_rgb_t c0=wheel_rgb((uint8_t)state->aux0);
    for(size_t i=0;i<count;i++) {
        const size_t idx=(reverse&&back)?count-i-1u:i;
        ot_wled_rgb_t col;
        if(i<led_index)col=back?c1:c0;
        else {
            col=back?c0:c1;
            if(i==led_index)
                col=blend_colors(back?c0:c1,back?c1:c0,(uint8_t)rem);
        }
        pixels[idx]=scaled(col,cfg->brightness);
    }
}
static void fx_random_colors(const ot_wled_fx_config_t *cfg,
                              ot_wled_fx_state_t *state,uint32_t now,
                              uint32_t old_call,ot_wled_rgb_t *pixels,
                              size_t count) {
    const uint32_t cycle=200u+(255u-(uint32_t)cfg->speed)*50u;
    const uint32_t it=now/cycle, rem=now%cycle;
    const uint32_t fade_duration=(cycle*(uint32_t)cfg->intensity)>>8;
    uint32_t fade=255u;
    if(fade_duration) {
        fade=(rem*255u)/fade_duration;
        if(fade>255u)fade=255u;
    }
    if(old_call==0u) {
        state->aux0=fx_rand8(state,256u);
        state->step=2u;
    }
    if(it!=state->step) {
        state->aux1=state->aux0;
        state->aux0=fx_new_wheel(state,(uint8_t)state->aux0);
        state->step=it;
    }
    const ot_wled_rgb_t c=scaled(blend_colors(
        wheel_rgb((uint8_t)state->aux1),
        wheel_rgb((uint8_t)state->aux0),(uint8_t)fade),cfg->brightness);
    for(size_t i=0;i<count;i++) pixels[i]=c;
}
static void fx_sparkle(const ot_wled_fx_config_t *cfg,
                        ot_wled_fx_state_t *state,uint32_t now,
                        ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t cycle=10u+(255u-(uint32_t)cfg->speed)*2u;
    const uint32_t it=now/cycle;
    if(it!=state->step) {
        state->aux0=(uint16_t)fx_rand_index(state,count);
        state->step=it;
    }
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    pixels[state->aux0%count]=fg;
}
static void fx_flash_sparkle(const ot_wled_fx_config_t *cfg,
                              ot_wled_fx_state_t *state,uint32_t now,
                              ot_wled_rgb_t *pixels,size_t count,
                              bool multiple) {
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=fg;
    if(now-(uint32_t)state->aux0>state->step) {
        if(fx_rand8(state,(255u-(uint32_t)cfg->intensity)>>4)==0u) {
            const size_t length=multiple?(count/3u?count/3u:1u):1u;
            for(size_t i=0;i<length;i++)pixels[fx_rand_index(state,count)]=bg;
        }
        state->step=now;
        state->aux0=(uint16_t)(255u-(uint32_t)cfg->speed);
    }
}
static void fx_traffic_light(const ot_wled_fx_config_t *cfg,
                              ot_wled_fx_state_t *state,uint32_t now,
                              ot_wled_rgb_t *pixels,size_t count) {
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    uint32_t delay=500u;
    for(size_t i=0;i+2u<count;i+=3u) {
        switch(state->aux0) {
            case 0:
                pixels[i]=scaled((ot_wled_rgb_t){255,0,0},cfg->brightness);
                delay=150u+100u*(255u-(uint32_t)cfg->speed);
                break;
            case 1:
                pixels[i]=scaled((ot_wled_rgb_t){255,0,0},cfg->brightness);
                pixels[i+1u]=scaled((ot_wled_rgb_t){238,204,0},cfg->brightness);
                delay=150u+20u*(255u-(uint32_t)cfg->speed);
                break;
            case 2:
                pixels[i+2u]=scaled((ot_wled_rgb_t){0,255,0},cfg->brightness);
                delay=150u+100u*(255u-(uint32_t)cfg->speed);
                break;
            default:
                /* Pinned stage 3 calls gamma32inv on Amber. Gamma LUT integration
                 * is still pending; uncorrected amber is the direct-RGB baseline. */
                pixels[i+1u]=scaled((ot_wled_rgb_t){238,204,0},cfg->brightness);
                delay=150u+20u*(255u-(uint32_t)cfg->speed);
                break;
        }
    }
    if(now-state->step>delay) {
        state->aux0++;
        if(state->aux0==1u&&cfg->intensity>140u)state->aux0=2u;
        if(state->aux0>3u)state->aux0=0u;
        state->step=now;
    }
}
static void fx_solid_glitter(const ot_wled_fx_config_t *cfg,
                              ot_wled_fx_state_t *state,
                              ot_wled_rgb_t *pixels,size_t count) {
    const ot_wled_rgb_t bg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t glitter=scaled((ot_wled_rgb_t){255,255,255},cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    /* FX.cpp glitter_base(): strict intensity > hw_random8 comparison. */
    if(cfg->intensity>fx_rand8(state,256u))
        pixels[fx_rand_index(state,count)]=glitter;
}

/*
 * Pinned FX.cpp mode_chase_random(): use the same chase geometry, but
 * choose distinct circular wheel indices at each wrap-around.
 */
static void chase_random_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               ot_wled_rgb_t *pixels,size_t count) {
    const uint16_t counter=(uint16_t)(now*((uint32_t)(cfg->speed>>2)+1u));
    const size_t a=((uint32_t)counter*(uint32_t)count)>>16;
    if(a<state->step) {
        state->aux1=state->aux0;
        state->aux0=fx_new_wheel(state,(uint8_t)state->aux0);
    }
    state->step=(uint32_t)a;
    const size_t width=1u+(((uint32_t)cfg->intensity*(uint32_t)count)>>10);
    const ot_wled_rgb_t base=scaled(wheel_rgb((uint8_t)state->aux0),cfg->brightness);
    const ot_wled_rgb_t old=scaled(wheel_rgb((uint8_t)state->aux1),cfg->brightness);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=i>=a?old:base;
    for(size_t i=0;i<width;i++)pixels[(a+i)%count]=fg;
    for(size_t i=0;i<width;i++)pixels[(a+width+i)%count]=fg;
}

/* Pinned FX.cpp mode_running_random() and its original 16-bit LCG.
 * The initial hw_random seed is represented by CORE's zone-local RNG. */
static void stream_frame(const ot_wled_fx_config_t *cfg,
                         ot_wled_fx_state_t *state,uint32_t now,
                         uint32_t old_call,ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t cycle=25u+3u*(255u-(uint32_t)cfg->speed);
    const uint32_t it=now/cycle;
    if(old_call==0u)state->aux0=(uint16_t)fx_prng(state);
    const unsigned size=((255u-(unsigned)cfg->intensity)>>4)+1u;
    uint16_t seed=state->aux0;
    unsigned z=it%size;
    bool new_zone=(z==0u && (it&65535u)!=state->aux1);
    for(size_t rev=count;rev>0;rev--) {
        const size_t i=rev-1u;
        if(new_zone || z>=size) {
            const unsigned last=seed>>8;
            int diff=0;
            unsigned attempts=0u;
            do {
                seed=(uint16_t)((uint32_t)seed*2053u+13849u);
                diff=(int)(seed>>8)-(int)last;
            } while(diff<42 && diff>-42 && ++attempts<65536u);
            if(new_zone) {
                state->aux0=seed;
                new_zone=false;
            }
            z=0;
        }
        pixels[i]=scaled(wheel_rgb((uint8_t)(seed>>8)),cfg->brightness);
        z++;
    }
    state->aux1=(uint16_t)it;
}
/* Pinned prng.h random16(): seed = seed*3001+31683; seed ^= seed>>7.
 * mode_random_chase() installs a local seed for repeatable strip patterns. */
static uint16_t stream2_next(uint16_t *seed) {
    uint16_t n=(uint16_t)((uint32_t)(*seed)*3001u+31683u);
    n^=(uint16_t)(n>>7);
    *seed=n;
    return n;
}
static uint8_t stream2_random8(uint16_t *seed) {
    return (uint8_t)stream2_next(seed);
}
static uint8_t stream2_bounded(uint16_t *seed,uint8_t bound) {
    return (uint8_t)(((uint16_t)stream2_random8(seed)*bound)>>8);
}
static void stream2_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          uint32_t old_call,ot_wled_rgb_t *pixels,
                          size_t count) {
    if(old_call==0u) {
        const uint8_t r=fx_rand8(state,256),g=fx_rand8(state,256),
                      b=fx_rand8(state,256);
        state->step=((uint32_t)r<<16)|((uint32_t)g<<8)|b;
        state->aux0=(uint16_t)fx_prng(state);
    }
    const uint32_t it=now/(25u+3u*(255u-(uint32_t)cfg->speed));
    uint32_t color=state->step;
    uint16_t seed=state->aux0;
    for(size_t rev=count;rev>0u;rev--) {
        const size_t i=rev-1u;
        const uint8_t r=stream2_bounded(&seed,6u)!=0u?
             (uint8_t)(color>>16):stream2_random8(&seed);
        const uint8_t g=stream2_bounded(&seed,6u)!=0u?
             (uint8_t)(color>>8):stream2_random8(&seed);
        const uint8_t b=stream2_bounded(&seed,6u)!=0u?
             (uint8_t)color:stream2_random8(&seed);
        color=((uint32_t)r<<16)|((uint32_t)g<<8)|b;
        pixels[i]=scaled((ot_wled_rgb_t){r,g,b},cfg->brightness);
        if(i==count-1u&&state->aux1!=(uint16_t)it) {
            state->step=color;
            state->aux0=seed;
        }
    }
    state->aux1=(uint16_t)it;
}

/* FX.cpp mode_lightning() adapted without modifying vehicle-trigger
 * arbitration; this effect only colors its caller-owned zone framebuffer. */
static void lightning_frame(const ot_wled_fx_config_t *cfg,
                            ot_wled_fx_state_t *state,uint32_t now,
                            ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    const size_t ledstart=fx_rand_index(state,count);
    const size_t ledlen=1u+fx_rand_index(state,count-ledstart);
    const uint8_t divisor=(uint8_t)(1u+fx_rand8(state,2u));
    uint8_t bri=(uint8_t)(255u/divisor);
    if(state->aux1==0u) {
        state->aux1=(uint16_t)(2u*(4u+fx_rand8(state,cfg->intensity/20u)));
        bri=52u;
        state->aux0=200u;
    }
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    if(state->aux1>3u && !(state->aux1&1u)) {
        const ot_wled_rgb_t color=scaled(
             scaled(cfg->foreground,bri),cfg->brightness);
        for(size_t i=ledstart;i<ledstart+ledlen;i++)pixels[i]=color;
        state->aux1--;
        state->step=now;
    } else if(now-state->step>state->aux0) {
        state->aux1--;
        if(state->aux1<2u)state->aux1=0u;
        state->aux0=(uint16_t)(50u+fx_rand8(state,100u));
        if(state->aux1==2u)
            state->aux0=(uint16_t)(fx_rand8(state,255u-(uint32_t)cfg->speed)*100u);
        state->step=now;
    }
}

/* FX.cpp mode_icu(), all positions and stage delays kept in SEGENV.step,
 * aux0 and aux1, preserving the upstream 16-bit wrap-safe time comparison. */
static void icu_frame(const ot_wled_fx_config_t *cfg,
                      ot_wled_fx_state_t *state,uint32_t now,
                      ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    const uint16_t t=(uint16_t)now;
    size_t dest=state->aux1;
    const size_t space=((size_t)cfg->intensity>>3)+2u;
    const size_t offset=count/space;
    uint16_t stage=(uint16_t)(state->step>>16);
    uint16_t next=(uint16_t)state->step;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    if(stage!=1u) {
        if(dest<count)pixels[dest]=fg;
        if(dest+offset<count)pixels[dest+offset]=fg;
        if(stage==3u) {
            if(state->aux0>state->aux1)dest++;
            else if(state->aux0<state->aux1 && dest>0u)dest--;
            if(dest<count)pixels[dest]=fg;
            if(dest+offset<count)pixels[dest+offset]=fg;
        }
    }
    if((int16_t)(t-next)>=0) {
        switch(stage) {
            case 0u:
                stage++;
                if(fx_rand8(state,6u)==0u) {
                    next=(uint16_t)(t+200u);
                    break;
                }
                /* Upstream no-blink path enters stage 1 immediately.
                 * Explicit branch avoids -Wimplicit-fallthrough under CI. */
                next=(uint16_t)(t+500u+fx_rand_index(state,1000u));
                stage++;
                break;
            case 1u:
                next=(uint16_t)(t+500u+fx_rand_index(state,1000u));
                stage++;
                break;
            case 2u:
                state->aux0=(uint16_t)fx_rand_index(state,count-offset);
                next=t;stage++;
                break;
            default:
                state->aux1=(uint16_t)dest;
                next=(uint16_t)(t+5u+(50u*(255u-(uint32_t)cfg->speed))/(uint32_t)count);
                if(state->aux0==dest) {
                    next=(uint16_t)(t+500u+fx_rand_index(state,1000u));
                    stage=0u;
                }
                break;
        }
    }
    state->step=((uint32_t)stage<<16)|next;
}

/*
 * Pinned FX.cpp mode_fire_flicker(), palette=0 RGB behavior. WLED holds
 * the previous frame between cycle boundaries; CORE replays the same
 * random sequence from the held per-zone seed so a fresh caller buffer
 * still renders the identical frame, without dynamic allocation.
 */
static void fire_flicker_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t cycle=40u+255u-(uint32_t)cfg->speed;
    const uint32_t it=now/cycle;
    const bool fresh=(state->call==1u || state->step!=it);
    if(fresh)state->held_rng_seed=state->rng_state;
    ot_wled_fx_state_t sample={0};
    sample.rng_state=state->held_rng_seed;
    unsigned maxc=cfg->foreground.r;
    if(cfg->foreground.g>maxc)maxc=cfg->foreground.g;
    if(cfg->foreground.b>maxc)maxc=cfg->foreground.b;
    const unsigned lum=maxc/(((256u-(unsigned)cfg->intensity)/16u)+1u);
    for(size_t i=0;i<count;i++) {
        const unsigned flicker=fx_rand8(&sample,lum);
        const ot_wled_rgb_t col={
            (uint8_t)(cfg->foreground.r>flicker?cfg->foreground.r-flicker:0u),
            (uint8_t)(cfg->foreground.g>flicker?cfg->foreground.g-flicker:0u),
            (uint8_t)(cfg->foreground.b>flicker?cfg->foreground.b-flicker:0u)
        };
        pixels[i]=scaled(col,cfg->brightness);
    }
    if(fresh){state->rng_state=sample.rng_state;state->step=it;}
}

/* Pinned FX.cpp mode_wavesins(), palette=0 direct foreground.
 * WLED's index modulation is ignored by the source itself when palette=0:
 * the pixel receives SEGCOLOR(0) at the sin8_t brightness. */
static void wavesins_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                           ot_wled_rgb_t *pixels,size_t count) {
    for(size_t i=0;i<count;i++) {
        const uint8_t bri=wled_sin8((uint8_t)(now/4u+(uint32_t)i*cfg->intensity));
        pixels[i]=scaled(scaled(cfg->foreground,bri),cfg->brightness);
    }
}

/*
 * Pinned util.cpp beat8 / beatsin8_t and fastled_slim scale8, used by
 * WLED FX.cpp BPM and Lake. Integer wrap behavior matches upstream.
 */
static uint8_t fx_beatsin8(uint16_t bpm,uint8_t lowest,
                           uint8_t highest,uint32_t now) {
    const uint16_t beat16=(uint16_t)(
         ((uint32_t)(now*(uint32_t)(bpm<<8)*280u))>>16);
    const uint8_t wave=wled_sin8((uint8_t)(beat16>>8));
    const uint8_t range=(uint8_t)(highest-lowest);
    const uint8_t scaled_wave=(uint8_t)(((uint16_t)wave*(1u+(unsigned)range))>>8);
    return (uint8_t)(lowest+scaled_wave);
}
static uint8_t fx_cos8(uint8_t phase) {
    return wled_sin8((uint8_t)(phase+64u));
}
/* Source mode_bpm() palette=0: beat index is ignored and WLED uses
 * primary (SEGCOLOR0) with its per-pixel beat-derived brightness. */
static void bpm_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                      ot_wled_rgb_t *pixels,size_t count) {
    const uint8_t phase=(uint8_t)(now/20u);
    const uint8_t beat=fx_beatsin8(cfg->speed,64u,255u,now);
    for(size_t i=0;i<count;i++) {
        const uint8_t intensity=(uint8_t)((uint32_t)beat-phase+i*10u);
        pixels[i]=scaled(scaled(cfg->foreground,intensity),cfg->brightness);
    }
}
/* Source mode_lake() palette=0: keep WLED's signed-range beatsin8
 * casts, cosine/cubic wave summation, and per-index brightness threshold. */
static void lake_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                       ot_wled_rgb_t *pixels,size_t count) {
    const uint16_t sp=(uint16_t)(cfg->speed/10u);
    const uint8_t wave1=fx_beatsin8((uint16_t)(sp+2u),
                       (uint8_t)192u,(uint8_t)64u,now);
    const uint8_t wave2=fx_beatsin8((uint16_t)(sp+1u),
                       (uint8_t)192u,(uint8_t)64u,now);
    const uint8_t wave3=fx_beatsin8((uint16_t)(sp+2u),0u,80u,now);
    for(size_t i=0;i<count;i++) {
        const uint8_t a=fx_cos8((uint8_t)(i*15u+wave1));
        const uint8_t b=wled_cubicwave8((uint8_t)(i*23u+wave2));
        const unsigned idx=(unsigned)a/2u+(unsigned)b/2u;
        const uint8_t intensity=(uint8_t)(idx>wave3?idx-wave3:0u);
        pixels[i]=scaled(scaled(cfg->foreground,intensity),cfg->brightness);
    }
}

/* Pinned FX.cpp mode_plasma(): independently phased cubic/cosine wave
 * channels; the palette=0 fallback consumes brightness of SEGCOLOR(0). */
static void plasma_frame(const ot_wled_fx_config_t *cfg,
                         ot_wled_fx_state_t *state,uint32_t now,
                         uint32_t old_call,ot_wled_rgb_t *pixels,
                         size_t count) {
    if(old_call==0u)state->aux0=fx_rand8(state,2u);
    const uint8_t phase1=fx_beatsin8((uint16_t)(6u+state->aux0),192u,64u,now);
    const uint8_t phase2=fx_beatsin8((uint16_t)(7u+state->aux0),192u,64u,now);
    const uint8_t threshold=fx_beatsin8(7u,0u,
                        (uint8_t)(128u-(cfg->intensity>>1)),now);
    const unsigned frequency=(unsigned)cfg->speed>>5;
    for(size_t i=0;i<count;i++) {
        const uint8_t wave1=wled_cubicwave8((uint8_t)(i*(2u+3u*frequency)+phase1));
        const uint8_t wave2=fx_cos8((uint8_t)(i*(1u+2u*frequency)+phase2));
        const unsigned index=(unsigned)wave1/2u+(unsigned)wave2/2u;
        const uint8_t brightness=(uint8_t)(index>threshold?index-threshold:0u);
        pixels[i]=scaled(scaled(cfg->foreground,brightness),cfg->brightness);
    }
}
/* Pinned FX.cpp mode_oscillate(), including signed two's-complement
 * position wrap, per-bar source speed changes and RGB black third slot. */
static void oscillate_frame(const ot_wled_fx_config_t *cfg,
                             ot_wled_fx_state_t *state,uint32_t now,
                             uint32_t old_call,ot_wled_rgb_t *pixels,
                             size_t count) {
    if(old_call==0u) {
        state->oscillators[0]=(ot_wled_oscillator_t){
            (uint16_t)(count/4u),(uint8_t)(count/8u),1,1};
        state->oscillators[1]=(ot_wled_oscillator_t){
            (uint16_t)((count/4u)*3u),(uint8_t)(count/8u),1,2};
        state->oscillators[2]=(ot_wled_oscillator_t){
            (uint16_t)((count/4u)*2u),(uint8_t)(count/8u),-1,1};
    }
    const uint32_t iteration=now/(20u+2u*(255u-(uint32_t)cfg->speed));
    for(unsigned j=0;j<3u;j++) {
        ot_wled_oscillator_t *o=&state->oscillators[j];
        if(iteration!=state->step)
            o->pos=(uint16_t)((int32_t)o->pos+(int32_t)o->dir*o->speed);
        o->size=(uint8_t)(count/(3u+(uint32_t)cfg->intensity/8u));
        if(o->dir==-1 && (uint32_t)o->pos>2u*(uint32_t)count) {
            o->pos=0;o->dir=1;
            o->speed=(uint8_t)((cfg->speed>100u?2u:1u)+
                 fx_rand8(state,2u));
        }
        if(o->dir==1 && o->pos>=count-1u) {
            o->pos=(uint16_t)(count-1u);o->dir=-1;
            o->speed=(uint8_t)((cfg->speed>100u?2u:1u)+
                 fx_rand8(state,2u));
        }
    }
    const ot_wled_rgb_t cols[3]={cfg->foreground,cfg->background,{0,0,0}};
    for(size_t i=0;i<count;i++) {
        ot_wled_rgb_t c={0,0,0};
        for(unsigned j=0;j<3u;j++) {
            const ot_wled_oscillator_t *o=&state->oscillators[j];
            if((int32_t)i>=(int32_t)o->pos-o->size &&
               i<=(uint32_t)o->pos+o->size) {
                const bool black=(c.r|c.g|c.b)==0u;
                c=black?cols[j]:blend_colors(c,cols[j],128u);
            }
        }
        pixels[i]=scaled(c,cfg->brightness);
    }
    state->step=iteration;
}

/*
 * Pinned WLED FX_fcn.cpp Segment::fade_out(), RGB three-channel adaptation.
 * The controller now retains each active zone's previous frame in CORE.
 * WLED brightness caps remain a separate full-parity release requirement.
 */
static void fx_fade_previous(const ot_wled_fx_config_t *cfg,
                             ot_wled_rgb_t *pixels,size_t count,uint8_t rate) {
    const unsigned fade_rate=(256u-(unsigned)rate)>>1;
    const int mapped=(int)(256u/(fade_rate+1u));
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++) {
        uint8_t *r=&pixels[i].r,*g=&pixels[i].g,*b=&pixels[i].b;
        uint8_t *lanes[3]={r,g,b};
        const uint8_t dst[3]={bg.r,bg.g,bg.b};
        for(unsigned k=0;k<3u;k++) {
            const int before=*lanes[k],target=dst[k];
            if(before==target)continue;
            int delta=(target-before)*mapped/256;
            if(delta==0)delta=target>before?1:-1;
            *lanes[k]=(uint8_t)(before+delta);
        }
    }
}
/* Source FX.cpp mode_larson_scanner() and dual variant with native
 * per-zone retained trail. No WLED pixel bus ownership. */
static void scanner_frame(const ot_wled_fx_config_t *cfg,
                           ot_wled_fx_state_t *state,uint32_t now,
                           ot_wled_rgb_t *pixels,size_t count,bool dual) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    const unsigned rate=(unsigned)cfg->frame_time_ms*
                (96u-(94u*(unsigned)cfg->speed)/255u);
    const size_t advance=count/(rate?rate:1u);
    fx_fade_previous(cfg,pixels,count,(uint8_t)(255u-cfg->intensity));
    if(state->step>now)return;
    size_t index=(size_t)state->aux1+advance;
    if(advance==0u) {
        const unsigned frames=rate/(unsigned)count;
        if(state->step++<frames)return;
        state->step=0u;index++;
    }
    if(index>count) {
        state->aux0=(uint16_t)!state->aux0;
        state->aux1=0u;
        /* FX.cpp custom1 is zero by default; pause becomes immediate. */
        state->step=0u;
    } else {
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=state->aux1;i<index;i++) {
            const size_t j=state->aux0?i:count-1u-i;
            pixels[j]=fg;
            if(dual)pixels[count-1u-j]=fg;
        }
        state->aux1=(uint16_t)index;
    }
}
/* Source FX.cpp mode_comet() fades retained previous frames, then paints
 * every intermediate index when frame pacing skips an expected LED. */
static void lighthouse_frame(const ot_wled_fx_config_t *cfg,
                              ot_wled_fx_state_t *state,uint32_t now,
                              uint32_t old_call,ot_wled_rgb_t *pixels,
                              size_t count) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    const uint16_t phase=(uint16_t)(now*((uint32_t)(cfg->speed>>2)+1u));
    const size_t index=((uint32_t)phase*(uint32_t)count)>>16;
    if(old_call==0u)state->aux0=(uint16_t)index;
    fx_fade_previous(cfg,pixels,count,cfg->intensity);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    pixels[index]=fg;
    if(index>state->aux0) {
        for(size_t i=state->aux0;i<index;i++)pixels[i]=fg;
    } else if(index<state->aux0 && index<10u) {
        for(size_t i=0;i<index;i++)pixels[i]=fg;
    }
    state->aux0=(uint16_t)index;
}
/* Pinned util.cpp beatsin16_t(), scale16, wled_math.cpp sin16_t. */
static uint16_t fx_beatsin16(uint16_t bpm,uint16_t max,
                             uint32_t now) {
    const uint16_t q88=(uint16_t)(bpm<256u?bpm<<8:bpm);
    const uint16_t beat=(uint16_t)(
         ((uint32_t)(now*(uint32_t)q88*280u))>>16);
    const uint16_t sine=(uint16_t)((int32_t)wled_sin16(beat)+32768);
    return (uint16_t)(((uint32_t)sine*(1u+(uint32_t)max))>>16);
}
/* Source FX.cpp sinelon_base() and wrappers. The prior color trail is
 * retained per zone and uses WLED's exact fade_out target computation. */
static void sinelon_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          uint32_t old_call,ot_wled_rgb_t *pixels,
                          size_t count,bool dual,bool rainbow) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    fx_fade_previous(cfg,pixels,count,cfg->intensity);
    const size_t pos=fx_beatsin16((uint16_t)(cfg->speed/10u),
                          (uint16_t)(count-1u),now);
    if(old_call==0u)state->aux0=(uint16_t)pos;
    const ot_wled_rgb_t col=scaled(rainbow?
           wheel_rgb((uint8_t)((pos&7u)*32u)):cfg->foreground,cfg->brightness);
    pixels[pos]=col;
    if(dual)pixels[count-1u-pos]=col;
    if(state->aux0!=pos) {
        if(state->aux0<pos) {
            for(size_t i=state->aux0;i<pos;i++) {
                pixels[i]=col;
                if(dual)pixels[count-1u-i]=col;
            }
        } else {
            for(size_t i=state->aux0;i>pos;i--) {
                if(i<count) {
                    pixels[i]=col;
                    if(dual)pixels[count-1u-i]=col;
                }
            }
        }
        state->aux0=(uint16_t)pos;
    }
}

bool ot_wled_fx_render(const ot_wled_fx_config_t *cfg, ot_wled_fx_state_t *state,
                       uint32_t time_ms, ot_wled_rgb_t *pixels, size_t count) {
    if (!cfg || !state || !pixels || count==0 || cfg->frame_time_ms==0 || cfg->palette_id!=0)
        return false;
    ot_wled_fx_metadata_t meta;
    if (!ot_wled_fx_metadata(cfg->effect_id, &meta)) return false;

    if (count>65535u) return false; /* 16-bit WLED segment indexing boundary */
    /* Per-zone WLED SEGENV.call advances once per rendered frame; capture
     * the old count because rainbow blink/strobe read it BEFORE advancing. */
    const uint32_t call=state->call++;
    if(cfg->effect_id==OT_WLED_SCANNER ||
       cfg->effect_id==OT_WLED_SCANNER_DUAL) {
        scanner_frame(cfg,state,time_ms,pixels,count,
                      cfg->effect_id==OT_WLED_SCANNER_DUAL);
        return true;
    }
    if(cfg->effect_id==OT_WLED_LIGHTHOUSE) {
        lighthouse_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_SINELON ||
       cfg->effect_id==OT_WLED_SINELON_DUAL ||
       cfg->effect_id==OT_WLED_SINELON_RAINBOW) {
        sinelon_frame(cfg,state,time_ms,call,pixels,count,
             cfg->effect_id==OT_WLED_SINELON_DUAL,
             cfg->effect_id==OT_WLED_SINELON_RAINBOW);
        return true;
    }
    if(cfg->effect_id==OT_WLED_PLASMA) {
        plasma_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_OSCILLATE) {
        oscillate_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_BPM) {
        bpm_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_LAKE) {
        lake_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_FIRE_FLICKER) {
        fire_flicker_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_WAVESINS) {
        wavesins_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_LIGHTNING) {
        lightning_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_ICU) {
        icu_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_CHASE_RANDOM) {
        chase_random_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_STREAM) {
        stream_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_STREAM2) {
        stream2_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_WIPE_RANDOM ||
       cfg->effect_id==OT_WLED_SWEEP_RANDOM) {
        fx_random_wipe(cfg,state,time_ms,call,pixels,count,
                       cfg->effect_id==OT_WLED_SWEEP_RANDOM);
        return true;
    }
    if(cfg->effect_id==OT_WLED_RANDOM_COLORS) {
        fx_random_colors(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_SPARKLE) {
        fx_sparkle(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_SPARKLE_DARK ||
       cfg->effect_id==OT_WLED_SPARKLE_PLUS) {
        fx_flash_sparkle(cfg,state,time_ms,pixels,count,
                         cfg->effect_id==OT_WLED_SPARKLE_PLUS);
        return true;
    }
    if(cfg->effect_id==OT_WLED_TRAFFIC_LIGHT) {
        fx_traffic_light(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_SOLID_GLITTER) {
        fx_solid_glitter(cfg,state,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_ANDROID) {
        android_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_SINEWAVE) {
        sinewave_frame(cfg,state,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_COLORFUL) {
        colorful_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_GRADIENT ||
        cfg->effect_id==OT_WLED_LOADING) {
        gradient_frame(cfg,time_ms,call,pixels,count,
                       cfg->effect_id==OT_WLED_LOADING);
        return true;
    }
    if (cfg->effect_id==OT_WLED_BREATHE) {
        breathe_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_RUNNING_LIGHTS ||
        cfg->effect_id==OT_WLED_SAW ||
        cfg->effect_id==OT_WLED_RUNNING_DUAL) {
        running_sine_frame(cfg,time_ms,pixels,count,
                           cfg->effect_id==OT_WLED_SAW,
                           cfg->effect_id==OT_WLED_RUNNING_DUAL);
        return true;
    }
    if (cfg->effect_id==OT_WLED_SOLID_PATTERN_TRI) {
        tri_pattern_frame(cfg,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_MULTI_STROBE) {
        multi_strobe_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_CHASE_COLOR ||
        cfg->effect_id==OT_WLED_CHASE_RAINBOW ||
        cfg->effect_id==OT_WLED_CHASE_RAINBOW_WHITE) {
        chase_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_TWO_DOTS) {
        two_dots_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_TRICOLOR_CHASE) {
        tricolor_chase_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_TRICOLOR_WIPE) {
        tricolor_wipe_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_TRICOLOR_FADE) {
        tricolor_fade_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_COLORLOOP || cfg->effect_id==OT_WLED_RAINBOW) {
        /* Pinned FX.cpp mode_rainbow() and mode_rainbow_cycle(): 16-bit
         * wrap, speed timing and intensity-selected rainbow spacing. */
        const uint16_t counter=(uint16_t)(time_ms*
            ((uint32_t)(cfg->speed>>2)+2u));
        const uint8_t base=(uint8_t)(counter>>8);
        if (cfg->effect_id==OT_WLED_COLORLOOP) {
            ot_wled_rgb_t color=wheel_rgb(base);
            if (cfg->intensity<128u)
                color=blend_colors(color,(ot_wled_rgb_t){255,255,255},
                                   (uint8_t)(128u-cfg->intensity));
            color=scaled(color,cfg->brightness);
            for (size_t i=0;i<count;i++) pixels[i]=color;
        } else {
            const uint32_t spread=16u<<(cfg->intensity/29u);
            for (size_t i=0;i<count;i++) {
                const uint8_t index=(uint8_t)(((uint32_t)i*spread)/(uint32_t)count+base);
                pixels[i]=scaled(wheel_rgb(index),cfg->brightness);
            }
        }
        return true;
    }

    if (cfg->effect_id==OT_WLED_WIPE || cfg->effect_id==OT_WLED_SWEEP) {
        wipe_frame(cfg,time_ms,pixels,count,cfg->effect_id==OT_WLED_SWEEP);
        return true;
    }
    if (cfg->effect_id==OT_WLED_SCAN || cfg->effect_id==OT_WLED_DUAL_SCAN) {
        scan_frame(cfg,time_ms,pixels,count,cfg->effect_id==OT_WLED_DUAL_SCAN);
        return true;
    }
    if (cfg->effect_id==OT_WLED_CHASE_FLASH) {
        chase_flash_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_RUNNING_COLOR) {
        running_color_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_THEATER ||
        cfg->effect_id==OT_WLED_THEATER_RAINBOW) {
        theater_frame(cfg,state,time_ms,pixels,count,
                      cfg->effect_id==OT_WLED_THEATER_RAINBOW);
        return true;
    }
    if (cfg->effect_id==OT_WLED_SOLID_PATTERN) {
        solid_pattern_frame(cfg,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_SPOTS || cfg->effect_id==OT_WLED_SPOTS_FADE) {
        spots_frame(cfg,time_ms,pixels,count,cfg->effect_id==OT_WLED_SPOTS_FADE);
        return true;
    }
    if (cfg->effect_id==OT_WLED_PERCENT) {
        percent_frame(cfg,state,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_HEARTBEAT) {
        heartbeat_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if (cfg->effect_id==OT_WLED_FADE) {
        uint16_t counter=(uint16_t)(time_ms*((uint32_t)(cfg->speed>>3)+10u));
        uint8_t blend=(uint8_t)(wave16(counter)>>8);
        ot_wled_rgb_t c=scaled(blend_colors(cfg->background,cfg->foreground,blend),
                               cfg->brightness);
        for(size_t i=0;i<count;i++) pixels[i]=c;
        return true;
    }
    bool on=true;
    if (cfg->effect_id==OT_WLED_BLINK || cfg->effect_id==OT_WLED_STROBE ||
        cfg->effect_id==OT_WLED_BLINK_RAINBOW ||
        cfg->effect_id==OT_WLED_STROBE_RAINBOW) {
        /* Mirror WLED's 32-bit integer arithmetic in blink():
         * cycleTime=(255-speed)*20;
         * onTime=FRAMETIME + (strobe?0:(cycleTime*intensity)>>8);
         * cycleTime+=FRAMETIME*2;
         * on=(cycleIndex!=SEGENV.step || phase<=onTime).
         * Updating step ONCE per frame preserves WLED's sparse-frame catch-up.
         */
        uint32_t cycle_time=(255u-(uint32_t)cfg->speed)*20u;
        uint32_t on_time=(uint32_t)cfg->frame_time_ms;
        if (cfg->effect_id==OT_WLED_BLINK ||
            cfg->effect_id==OT_WLED_BLINK_RAINBOW)
            on_time += (cycle_time * (uint32_t)cfg->intensity)>>8;
        cycle_time += (uint32_t)cfg->frame_time_ms*2u;
        uint32_t cycle_index=time_ms/cycle_time;
        uint32_t phase=time_ms%cycle_time;
        on=(cycle_index!=state->step || phase<=on_time);
        state->step=cycle_index;
    }

    const bool rainbow_blink=(cfg->effect_id==OT_WLED_BLINK_RAINBOW ||
                              cfg->effect_id==OT_WLED_STROBE_RAINBOW);
    const ot_wled_rgb_t color=scaled(on ? (rainbow_blink ? wheel_rgb((uint8_t)call) :
                                                        cfg->foreground) :
                                            cfg->background,cfg->brightness);
    for (size_t i=0;i<count;++i) pixels[i]=color;
    return true;
}
