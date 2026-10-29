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
#include <stdlib.h>
#include <math.h>

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
     false, true, true, true, false},
    {OT_WLED_TWINKLE, "Twinkle", "Twinkle@!,Density;!,!;!",
     true, true, true, true, false},
    {OT_WLED_CHASE_FLASH_RANDOM, "Chase Flash Rnd", "Chase Flash Rnd@!;!,!;!",
     true, true, true, false, false},
    {OT_WLED_MULTI_COMET, "Multi Comet", "Multi Comet@!,Fade;!,!;!",
     true, true, true, true, false},
    {OT_WLED_CHUNCHUN, "Chunchun", "Chunchun@!,Spread;!,!;!",
     true, true, true, true, false},
    {OT_WLED_CANDLE, "Candle", "Candle@!,!;!,!;!",
     true, true, true, true, false},
    {OT_WLED_DYNAMIC, "Dynamic", "Dynamic@!,!,,,,Smooth;;!",
     false, false, true, true, false},
    {OT_WLED_DYNAMIC_SMOOTH, "Dynamic Smooth", "Dynamic Smooth@!,!;;!",
     false, false, true, true, false},
    {OT_WLED_DISSOLVE, "Dissolve", "Dissolve@Repeat speed,Dissolve speed,,,,Random,Complete;!,!;!",
     true, true, true, true, false},
    {OT_WLED_DISSOLVE_RANDOM, "Dissolve Rnd", "Dissolve Rnd@Repeat speed,Dissolve speed;,!;!",
     false, true, true, true, false},
    {OT_WLED_CANDLE_MULTI, "Candle Multi", "Candle Multi@!,!;!,!;!",
     true, true, true, true, false},
    {OT_WLED_METEOR, "Meteor", "Meteor@!,Trail;!,!;!",
     true, true, true, true, false},
    {OT_WLED_TWINKLEUP, "Twinkleup", "Twinkleup@!,Intensity;!,!;!;;m12=0",
     true, true, true, true, false},
    {OT_WLED_RAILWAY, "Railway", "Railway@!,Smoothness;1,2;!;;pal=3",
     true, true, true, true, false},
    {OT_WLED_WASHING_MACHINE, "Washing Machine", "Washing Machine@!,!;;!",
     true, true, true, true, false},
    {OT_WLED_FLOW, "Flow", "Flow@!,Zones;;!;;m12=1",
     true, true, true, true, false},
    {OT_WLED_SUNRISE, "Sunrise", "Sunrise@Time [min],Width;;!;;pal=35,sx=60",
     true, true, true, true, false},
    {OT_WLED_RIPPLE, "Ripple", "Ripple@!,Frequency;!,!;!",
     true, true, true, true, false},
    {OT_WLED_RIPPLE_RAINBOW, "Ripple Rainbow", "Ripple Rainbow@!,Frequency;!,!;!",
     true, true, true, true, false},
    {OT_WLED_FIREWORKS, "Fireworks", "Fireworks@,Frequency;!,!;!;12;ix=192,pal=11",
     true, true, true, true, false},
    {OT_WLED_RAIN, "Rain", "Rain@,Frequency;!,!;!;12;ix=192,pal=11",
     true, true, true, true, false},
    {OT_WLED_JUGGLE, "Juggle", "Juggle@!,Trail;;!;;sx=64,ix=128",
     true, true, true, true, false},
    {OT_WLED_FILL_NOISE, "Fill Noise", "Fill Noise@!,Scale;;!",
     true, false, true, true, false},
    {OT_WLED_NOISE_1, "Noise 1", "Noise 1@!,Scale;;!",
     true, false, true, true, false},
    {OT_WLED_NOISE_2, "Noise 2", "Noise 2@!,Scale;;!",
     true, false, true, true, false},
    {OT_WLED_NOISE_3, "Noise 3", "Noise 3@!,Scale;;!",
     true, false, true, true, false},
    {OT_WLED_NOISE_4, "Noise 4", "Noise 4@!,Scale;;!",
     true, false, true, true, false},
    {OT_WLED_PHASED, "Phased", "Phased@!,!;!,!;!",
     true, true, true, true, false},
    {OT_WLED_PHASED_NOISE, "Phased Noise", "Phased Noise@!,!;!,!;!",
     true, true, true, true, false},
    {OT_WLED_PRIDE_2015, "Pride 2015", "Pride 2015@!;;",
     false, false, true, false, false},
    {OT_WLED_COLORWAVES, "Colorwaves", "Colorwaves@!,Hue;!;!;;pal=26",
     true, false, true, true, false},
    {OT_WLED_FAIRY, "Fairy", "Fairy@!,# of flashers;!,!;!",
     true, true, true, true, false},
    {OT_WLED_FAIRYTWINKLE, "Fairytwinkle", "Fairytwinkle@!,!;!,!;!;;m12=0",
     true, true, true, true, false},
    {OT_WLED_BOUNCING_BALLS, "Bouncing Balls", "Bouncing Balls@Gravity,# of balls,,,,,Overlay;!,!,!;!;1;m12=1",
     true, true, true, true, false},
    {OT_WLED_HALLOWEEN_EYES, "Halloween Eyes", "Halloween Eyes@Eye off time,Eye on time,,,,,Overlay;!,!;!;12",
     true, true, true, true, false},
    {OT_WLED_ROLLING_BALLS, "Rolling Balls", "Rolling Balls@!,# of balls,,,,Collide,Overlay,Trails;!,!,!;!;1;m12=1",
     true, true, true, true, false},
    {OT_WLED_POPCORN, "Popcorn", "Popcorn@!,!,,,,,Overlay;!,!,!;!;;m12=1",
     true, true, true, true, false},
    {OT_WLED_DRIP, "Drip", "Drip@Gravity,# of drips,,,,,Overlay;!,!;!;;m12=1",
     true, true, true, true, false},
    {OT_WLED_TV_SIMULATOR, "TV Simulator", "TV Simulator@!,!;;!;01",
     false, false, true, true, false},
    {OT_WLED_COLOR_CLOUDS, "Color Clouds", "Color Clouds@!,!,Clouds,Colors,Distance,,,Cozy;;!;;sx=24,ix=32,c1=48,c2=64,c3=12,pal=0",
     false, false, true, true, false}
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

#define OT_FX_SCRATCH_MAGIC 0x0FA117EDu
void ot_wled_fx_reset(ot_wled_fx_state_t *state) {
    if (state) {
        /* Only own/free allocations carrying our initialized magic. */
        if(state->scratch_magic==OT_FX_SCRATCH_MAGIC)free(state->scratch);
        memset(state,0,sizeof(*state));
        state->rng_state=0x73A11C5Du;
        state->scratch_magic=OT_FX_SCRATCH_MAGIC;
    }
}
/* Each zone owns its source data. Resize only on effect replacement or
 * LED-count change; no allocation occurs on steady-state render frames. */
static uint8_t *fx_zone_scratch(ot_wled_fx_state_t *state,size_t required) {
    if(!required)return NULL;
    if(state->scratch_magic!=OT_FX_SCRATCH_MAGIC) {
        state->scratch=NULL;state->scratch_bytes=0u;
        state->scratch_magic=OT_FX_SCRATCH_MAGIC;
    }
    if(state->scratch && state->scratch_bytes>=required)return state->scratch;
    uint8_t *next=(uint8_t*)calloc(required,1u);
    if(!next)return NULL;
    if(state->scratch) {
        memcpy(next,state->scratch,state->scratch_bytes);
        free(state->scratch);
    }
    state->scratch=next;
    state->scratch_bytes=required;
    return next;
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
/* Direct source adaptation of WLED FX.cpp phased_base(0).
 * CORE's palette=0 foreground/background semantics match the original
 * SEGCOLOR(0)/SEGCOLOR(1) path. Preserve the source float phase in
 * SEGENV.step bits; memcpy avoids strict-aliasing undefined behavior.
 * Phased Noise is not routed here; it requires original 1D Perlin. */
static uint8_t fx_perlin8_1(uint16_t x);
static void phased_frame(const ot_wled_fx_config_t *cfg,
                         ot_wled_fx_state_t *state,
                         ot_wled_rgb_t *pixels,size_t count,
                         bool use_noise) {
    float phase;
    memcpy(&phase,&state->step,sizeof(phase));
    phase += (float)cfg->speed/32.0f;
    memcpy(&state->step,&phase,sizeof(phase));

    const unsigned cutoff=255u-(unsigned)cfg->intensity;
    const ot_wled_rgb_t bg=cfg->background;
    const ot_wled_rgb_t fg=cfg->foreground;
    for(size_t i=0;i<count;i++) {
        unsigned mod_val=5u;
        if(use_noise)mod_val=fx_perlin8_1((uint16_t)((unsigned)i*20u))/16u;
        if(!mod_val)mod_val=1u;
        unsigned val=((unsigned)i+1u)*16u;
        val+=(unsigned)(phase*(float)(((unsigned)i%mod_val)+1u)/2.0f);
        unsigned b=wled_cubicwave8((uint8_t)val);
        b=(b>cutoff)?b-cutoff:0u;
        pixels[i]=scaled(blend_colors(bg,fg,(uint8_t)b),cfg->brightness);
    }
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
    /* WLED hw_random16(0) returns zero; never divide by a zero bound. */
    if(count==0u)return 0u;
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

/* Pinned FX.cpp mode_twinkle(): original 16-bit LCG locations and
 * 224-rate fade, preserving the previous active zone frame. */
static void twinkle_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          ot_wled_rgb_t *pixels,size_t count) {
    fx_fade_previous(cfg,pixels,count,224u);
    const uint32_t cycle=20u+5u*(255u-(uint32_t)cfg->speed);
    const uint32_t it=now/cycle;
    if(it!=state->step) {
        const size_t max_on=1u+
            ((uint32_t)cfg->intensity*((uint32_t)count-1u))/255u;
        if(state->aux0>=max_on) {
            state->aux0=0u;
            state->aux1=(uint16_t)fx_prng(state);
        }
        state->aux0++;
        state->step=it;
    }
    uint16_t seed=state->aux1;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(unsigned j=0;j<state->aux0;j++) {
        seed=(uint16_t)((uint32_t)seed*2053u+13849u);
        const size_t index=((uint32_t)count*(uint32_t)seed)>>16;
        pixels[index]=fg;
    }
}
/* Pinned FX.cpp mode_chase_flash_random() with its SEGENV.call freeze
 * during the stepped flash delays, and retained adjacent flash pixels. */
static void chase_flash_random_frame(const ot_wled_fx_config_t *cfg,
                                      ot_wled_fx_state_t *state,uint32_t now,
                                      ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    const bool advance=now>=state->step;
    if(!advance && state->call)state->call--;
    const unsigned flash_step=state->call%9u;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    const ot_wled_rgb_t wheel=scaled(wheel_rgb((uint8_t)state->aux0),cfg->brightness);
    for(size_t i=0;i<state->aux1 && i<count;i++)pixels[i]=wheel;
    uint32_t delay=1u+(10u*(255u-(uint32_t)cfg->speed))/(uint32_t)count;
    if(flash_step<8u) {
        const size_t n=state->aux1%count;
        const size_t m=(n+1u)%count;
        if(!(flash_step&1u)) {
            pixels[n]=fg;pixels[m]=fg;delay=20u;
        } else {
            pixels[n]=wheel;pixels[m]=bg;delay=30u;
        }
    } else if(advance) {
        state->aux1=(uint16_t)((state->aux1+1u)%count);
        if(state->aux1==0u)state->aux0=fx_new_wheel(state,(uint8_t)state->aux0);
    }
    if(advance)state->step=now+delay;
}
/* Pinned FX.cpp mode_multi_comet(), eight per-zone fixed particles, and
 * source fade_out rate (intensity/2+128). No heap, no WLED bus. */
static void multi_comet_frame(const ot_wled_fx_config_t *cfg,
                              ot_wled_fx_state_t *state,uint32_t now,
                              ot_wled_rgb_t *pixels,size_t count) {
    const uint32_t cycle=10u+(255u-(uint32_t)cfg->speed);
    const uint32_t it=now/cycle;
    if(state->step==it)return;
    fx_fade_previous(cfg,pixels,count,(uint8_t)(cfg->intensity/2u+128u));
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t j=0;j<8u;j++) {
        if(state->comet_positions[j]<count) {
            const size_t index=state->comet_positions[j];
            pixels[index]=fg;
            state->comet_positions[j]++;
        } else if(fx_rand_index(state,count)==0u) {
            state->comet_positions[j]=0u;
        }
    }
    state->step=it;
}
/* Pinned FX.cpp mode_chunchun(): original integer sine positions for
 * 2+len/8 'birds', no matrix or output-specific indexing assumptions. */
static void chunchun_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                           ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u){pixels[0]=scaled(cfg->foreground,cfg->brightness);return;}
    fx_fade_previous(cfg,pixels,count,254u);
    uint32_t counter=now*(6u+((uint32_t)cfg->speed>>4));
    const uint32_t num_birds=2u+((uint32_t)count>>3);
    const uint32_t span=((uint32_t)cfg->intensity<<8)/num_birds;
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(uint32_t i=0;i<num_birds;i++) {
        counter-=span;
        const uint32_t wave=(uint32_t)((int32_t)wled_sin16((uint16_t)counter)+32768);
        uint32_t index=(wave*(uint32_t)count)>>16;
        if(index>=count)index=(uint32_t)count-1u;
        pixels[index]=fg;
    }
}

/* Pinned FX.cpp candle(false): source candle luminance state and
 * 23-ms update gate; no allocation for its single-segment variant. */
static void candle_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          ot_wled_rgb_t *pixels,size_t count) {
    if(now-state->data_counter<23u)return;
    state->data_counter=now;
    const unsigned range=cfg->intensity,rand_val=range>>1;
    const unsigned speed_factor=cfg->speed>252u?1u:
       cfg->speed>99u?2u:cfg->speed>49u?3u:4u;
    unsigned luminance=state->aux0;
    unsigned target=state->aux1;
    unsigned step=state->step;
    if(step==0u) {
        luminance=128u;
        target=130u+fx_rand8(state,4u);
        step=1u;
    }
    bool new_target=false;
    if(target>luminance) {
        luminance=luminance+step>255u?255u:luminance+step;
        if(luminance>=target)new_target=true;
    } else {
        luminance=luminance>step?luminance-step:0u;
        if(luminance<=target)new_target=true;
    }
    if(new_target) {
        target=fx_rand8(state,rand_val)+fx_rand8(state,rand_val);
        if(target<(rand_val>>1))
            target=(rand_val>>1)+fx_rand8(state,rand_val);
        target+=255u-range;
        const unsigned difference=target>luminance?
               target-luminance:luminance-target;
        step=difference>>speed_factor;
        if(step==0u)step=1u;
    }
    const ot_wled_rgb_t color=scaled(blend_colors(
            cfg->background,cfg->foreground,(uint8_t)luminance),cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=color;
    state->aux0=(uint16_t)luminance;
    state->aux1=(uint16_t)target;
    state->step=step;
}

/*
 * Pinned FX.cpp mode_dynamic() / mode_dynamic_smooth() with per-zone
 * SEGENV.data bytes and source random hue/update probabilities.
 */
static bool dynamic_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          uint32_t old_call,ot_wled_rgb_t *pixels,
                          size_t count,bool smooth) {
    uint8_t *hues=fx_zone_scratch(state,count);
    if(!hues)return false;
    if(old_call==0u) {
        for(size_t i=0;i<count;i++)hues[i]=fx_rand8(state,256u);
    }
    const uint32_t cycle=50u+15u*(255u-(uint32_t)cfg->speed);
    const uint32_t it=now/cycle;
    if(it!=state->step && cfg->speed!=0u) {
        for(size_t i=0;i<count;i++) {
            if(fx_rand8(state,256u)<=cfg->intensity)
                hues[i]=fx_rand8(state,256u);
        }
        state->step=it;
    }
    for(size_t i=0;i<count;i++) {
        const ot_wled_rgb_t c=scaled(wheel_rgb(hues[i]),cfg->brightness);
        if(smooth)pixels[i]=blend_colors(pixels[i],c,16u);
        else pixels[i]=c;
    }
    return true;
}
static uint32_t fx_pack(ot_wled_rgb_t c) {
    return ((uint32_t)c.r<<16)|((uint32_t)c.g<<8)|c.b;
}
static ot_wled_rgb_t fx_unpack(uint32_t c) {
    return (ot_wled_rgb_t){(uint8_t)(c>>16),
                           (uint8_t)(c>>8),(uint8_t)c};
}
/* Pinned FX.cpp dissolve() backing array and both wrappers.
 * Default WLED overlay/check1/check2 options are off; the original
 * two-way state machine still uses aux0 and step on every render. */
static bool dissolve_frame(const ot_wled_fx_config_t *cfg,
                           ot_wled_fx_state_t *state,uint32_t now,
                           uint32_t old_call,ot_wled_rgb_t *pixels,
                           size_t count,bool rainbow) {
    (void)now;
    uint32_t *raw=(uint32_t*)fx_zone_scratch(state,count*sizeof(uint32_t));
    if(!raw)return false;
    const uint32_t background=fx_pack(cfg->background);
    if(old_call==0u) {
        for(size_t i=0;i<count;i++)raw[i]=background;
        state->aux0=1u;
    }
    const uint32_t color=fx_pack(rainbow?
                              wheel_rgb(fx_rand8(state,256u)):
                              cfg->foreground);
    for(size_t j=0;j<=count/15u;j++) {
        if(fx_rand8(state,256u)<=cfg->intensity) {
            for(unsigned attempts=0;attempts<10u;attempts++) {
                const size_t i=fx_rand_index(state,count);
                if(state->aux0) {
                    if(raw[i]==background){raw[i]=color;break;}
                } else if(raw[i]!=background){raw[i]=background;break;}
            }
        }
    }
    for(size_t i=0;i<count;i++)
        pixels[i]=scaled(fx_unpack(raw[i]),cfg->brightness);
    if(state->step>255u-(uint32_t)cfg->speed+15u) {
        state->aux0=(uint16_t)!state->aux0;
        state->step=0u;
    } else state->step++;
    return true;
}


/* Pinned FX.cpp candle(true): one 32-bit last-call time plus a three-byte
 * brightness/target/step tuple per pixel beyond candle zero. */
static bool candle_multi_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               ot_wled_rgb_t *pixels,size_t count) {
    if(count>SIZE_MAX/3u-4u)return false;
    uint8_t *data=fx_zone_scratch(state,4u+(count>1u?(count-1u)*3u:3u));
    if(!data)return false;
    uint32_t lastcall;
    memcpy(&lastcall,data,sizeof(lastcall));
    if(now-lastcall<23u)return true;
    memcpy(data,&now,sizeof(now));
    const unsigned valrange=cfg->intensity;
    const unsigned rndval=valrange>>1;
    const unsigned factor=cfg->speed>252u?1u:cfg->speed>99u?2u:
                          cfg->speed>49u?3u:4u;
    for(size_t i=0;i<count;i++) {
        const size_t off=4u+(i-1u)*3u;
        unsigned brightness=i==0u?state->aux0:data[off];
        unsigned target=i==0u?state->aux1:data[off+1u];
        unsigned fade_step=i==0u?state->step:data[off+2u];
        if(fade_step==0u) {
            brightness=128u;
            target=130u+fx_rand8(state,4u);
            fade_step=1u;
        }
        bool new_target=false;
        if(target>brightness) {
            brightness=brightness+fade_step>255u?255u:brightness+fade_step;
            if(brightness>=target)new_target=true;
        } else {
            brightness=brightness>fade_step?brightness-fade_step:0u;
            if(brightness<=target)new_target=true;
        }
        if(new_target) {
            target=fx_rand8(state,rndval)+fx_rand8(state,rndval);
            if(target<(rndval>>1))
                target=(rndval>>1)+fx_rand8(state,rndval);
            target+=(255u-valrange);
            unsigned difference=target>brightness?
                target-brightness:brightness-target;
            fade_step=difference>>factor;
            if(!fade_step)fade_step=1u;
        }
        const ot_wled_rgb_t col=scaled(blend_colors(cfg->background,
                         cfg->foreground,(uint8_t)brightness),cfg->brightness);
        if(i==0u) {
            for(size_t j=0;j<count;j++)pixels[j]=col;
            state->aux0=(uint16_t)brightness;
            state->aux1=(uint16_t)target;
            state->step=fade_step;
        } else {
            pixels[i]=col;
            data[off]=(uint8_t)brightness;
            data[off+1u]=(uint8_t)target;
            data[off+2u]=(uint8_t)fade_step;
        }
    }
    return true;
}
/* Pinned FX.cpp mode_meteor() source path with check1/check3 false.
 * Palette 0 maps the source heat-trail brightness into foreground RGB.
 * Each universal zone retains its own WLED trail SEGENV.data bytes. */
static bool meteor_frame(const ot_wled_fx_config_t *cfg,
                         ot_wled_fx_state_t *state,uint32_t now,
                         ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);return true;
    }
    uint8_t *trail=fx_zone_scratch(state,count);
    if(!trail)return false;
    const size_t size=1u+count/20u;
    const uint32_t counter=now*((uint32_t)(cfg->speed>>2)+8u);
    const size_t start=(size_t)((counter*(uint32_t)count)>>16);
    for(size_t i=0;i<count;i++) {
        if(fx_rand8(state,256u)<=255u-(uint32_t)cfg->intensity) {
            const uint8_t scale=(uint8_t)(128u+fx_rand8(state,127u));
            trail[i]=(uint8_t)(((uint16_t)trail[i]*(1u+scale))>>8);
            const uint8_t luminance=trail[i];
            pixels[i]=scaled(scaled(cfg->foreground,luminance),cfg->brightness);
        }
    }
    const ot_wled_rgb_t full=scaled(cfg->foreground,cfg->brightness);
    for(size_t j=0;j<size;j++) {
        const size_t index=(start+j)%count;
        trail[index]=240u;
        pixels[index]=full;
    }
    state->step+=(uint32_t)cfg->speed+1u;
    return true;
}

/* Pinned FX.cpp mode_twinkleup() and prng.h: a fixed local seed 535 is
 * replayed per frame; all three random draws per LED must be consumed even
 * when the palette=0 source index is ignored by color_from_palette(). */
static void twinkleup_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                            ot_wled_rgb_t *pixels,size_t count) {
    uint16_t prng=535u;
    const uint32_t period=256u-(uint32_t)cfg->speed;
    const uint32_t phase=(16u*now)/period;
    for(size_t i=0;i<count;i++) {
        const uint8_t start=stream2_random8(&prng);
        uint8_t brightness=wled_sin8((uint8_t)((uint32_t)start+phase));
        if(stream2_random8(&prng)>cfg->intensity)brightness=0u;
        (void)stream2_random8(&prng); /* upstream palette-index draw */
        const ot_wled_rgb_t c=blend_colors(cfg->background,cfg->foreground,
                                            brightness);
        pixels[i]=scaled(c,cfg->brightness);
    }
}

/* Pinned FX.cpp mode_railway() state and ramp timing. The upstream
 * default 'Colors 1 & 2' palette is represented with two configurable
 * RGB colors; multi-entry palette interpolation remains outside this port. */
static void railway_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,
                          ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const uint32_t dur=(256u-(uint32_t)cfg->speed)*40u;
    const uint16_t ramp=(uint16_t)((dur*(uint32_t)cfg->intensity)>>8);
    if(state->step>dur) {
        state->step=0u;
        state->aux0=(uint16_t)!state->aux0;
    }
    unsigned pos=255u;
    if(ramp!=0u) {
        const unsigned p=(unsigned)(((uint64_t)state->step*255u)/ramp);
        if(p<255u)pos=p;
    }
    if(state->aux0)pos=255u-pos;
    const ot_wled_rgb_t first=scaled(
         blend_colors(cfg->background,cfg->foreground,(uint8_t)(255u-pos)),
         cfg->brightness);
    const ot_wled_rgb_t second=scaled(
         blend_colors(cfg->background,cfg->foreground,(uint8_t)pos),
         cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=(i&1u)?second:first;
    state->step+=(uint32_t)cfg->frame_time_ms;
}
/* Exact pinned FX.cpp tristate_square8() signed waveform. */
static int8_t fx_tristate8(uint8_t x,uint8_t pulse,uint8_t attack) {
    int8_t amplitude=127;
    if(x>127u) {
        amplitude=-127;
        x=(uint8_t)(x-127u);
    }
    if(x<attack)return (int8_t)(((int16_t)x*amplitude)/attack);
    if(x<(uint8_t)(pulse-attack))return amplitude;
    if(x<pulse)return (int8_t)(((int16_t)(pulse-x)*amplitude)/attack);
    return 0;
}
/* Pinned mode_washing_machine() sine and phase arithmetic; default
 * palette-index to RGB colors requires full palette mapper later. */
static void washing_frame(const ot_wled_fx_config_t *cfg,
                           ot_wled_fx_state_t *state,uint32_t now,
                           ot_wled_rgb_t *pixels,size_t count) {
    const int speed=fx_tristate8((uint8_t)(now>>7),90u,15u);
    const int delta=(speed*2048)/(512-(int)cfg->speed);
    state->step+=(uint32_t)delta;
    for(size_t i=0;i<count;i++) {
        const uint32_t angle=(((uint32_t)cfg->intensity/25u+1u)*255u*
                              (uint32_t)i)/(uint32_t)count+(state->step>>7);
        const uint8_t color_index=wled_sin8((uint8_t)angle);
        pixels[i]=scaled(blend_colors(cfg->background,cfg->foreground,
                                       color_index),cfg->brightness);
    }
}
/* Pinned mode_flow() 1D zigzag zones and rotating stripe indices.
 * The source palette mapping for mcol=255 still needs engine-wide parity. */
static void flow_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                       ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    uint32_t counter=0u;
    if(cfg->speed)counter=(now*((uint32_t)(cfg->speed>>2)+1u))>>8;
    const unsigned max_zones=(unsigned)count/6u;
    unsigned zones=((uint32_t)cfg->intensity*max_zones)>>8;
    if(zones&1u)zones++;
    if(zones<2u)zones=2u;
    const unsigned zone_len=(unsigned)count/zones;
    if(!zone_len) {
        for(size_t i=0;i<count;i++)pixels[i]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const unsigned required=((unsigned)count+zone_len-1u)/zone_len;
    zones=required+2u;
    const int32_t offset=((int32_t)count-(int32_t)zones*(int32_t)zone_len)/2;
    for(size_t i=0;i<count;i++)pixels[i]=scaled(cfg->background,cfg->brightness);
    for(unsigned z=0;z<zones;z++) {
        const int32_t pos=offset+(int32_t)z*(int32_t)zone_len;
        for(unsigned i=0;i<zone_len;i++) {
            const uint8_t index=(uint8_t)(((uint32_t)i*255u)/zone_len-counter);
            const unsigned led=(z&1u)?i:(zone_len-1u-i);
            const int32_t dst=pos+(int32_t)led;
            if(dst<0 || (size_t)dst>=count)continue;
            pixels[dst]=scaled(blend_colors(cfg->background,cfg->foreground,index),
                               cfg->brightness);
        }
    }
}
/* Pinned mode_sunrise() stage progress and center-out geometry. Upstream
 * Fire palette (ID 35) is not available yet; use the current two-color
 * interpolation only as a source-derived geometry port. */
static void sunrise_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,uint32_t now,
                          uint32_t old_call,ot_wled_rgb_t *pixels,
                          size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);return;
    }
    if(old_call==0u || cfg->speed!=state->aux0) {
        state->step=now;state->aux0=cfg->speed;
    }
    uint32_t stage=0xffffu;
    uint32_t elapsed=(now-state->step)/100u;
    if(cfg->speed>120u) {
        const uint16_t counter=(uint16_t)(
            (now>>1)*(((uint32_t)(cfg->speed-120u)>>1)+1u));
        stage=wave16(counter);
    } else if(cfg->speed) {
        const uint32_t minutes=cfg->speed>60u?(uint32_t)cfg->speed-60u:
                                (uint32_t)cfg->speed;
        const uint32_t target=minutes*600u;
        if(elapsed>target)elapsed=target;
        stage=(elapsed*65535u)/target;
        if(cfg->speed>60u)stage=65535u-stage;
    }
    for(size_t i=0;i<=count/2u;i++) {
        const uint32_t angle=(uint32_t)(((uint64_t)i*stage)/count);
        const uint32_t wave=wave16((uint16_t)angle);
        uint32_t luminance=(wave>>8)+((wave*(uint32_t)cfg->intensity)>>15);
        if(luminance>240u)luminance=240u;
        const ot_wled_rgb_t c=scaled(blend_colors(cfg->background,cfg->foreground,
                                        (uint8_t)luminance),cfg->brightness);
        pixels[i]=c;
        pixels[count-1u-i]=c;
    }
}

/* Pinned FX.cpp ripple_base() particle cells (4 bytes each), max 100 on
 * ESP32: independent per-zone heap backing, no fixed vehicle LED geometry. */
typedef struct {
    uint8_t ripple_state;
    uint8_t color;
    uint16_t pos;
} ot_fx_ripple_t;
static uint8_t fx_triwave8(uint8_t x) {
    if(x&0x80u)x=(uint8_t)(255u-x);
    return (uint8_t)(x<<1);
}
static void fx_ripple_blend(ot_wled_rgb_t *pixels,size_t count,
                            int32_t pos,ot_wled_rgb_t color,
                            uint8_t magnitude,uint8_t brightness) {
    if(pos<0 || (size_t)pos>=count)return;
    const ot_wled_rgb_t next=blend_colors(pixels[pos],color,magnitude);
    pixels[pos]=scaled(next,brightness);
}
/* Upstream mode_ripple() and mode_ripple_rainbow() with the original
 * 1D four-tap propagation, wave birth, amplitude and lifetime. WLED
 * nonzero palette selection is still outside the current RGB-only profile. */
static bool ripple_frame(const ot_wled_fx_config_t *cfg,
                         ot_wled_fx_state_t *state,uint32_t now,
                         uint32_t old_call,ot_wled_rgb_t *pixels,
                         size_t count,bool rainbow) {
    (void)now;
    if(count<=1u){
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return true;
    }
    const size_t max_ripples=(1u+count/4u)<100u?(1u+count/4u):100u;
    ot_fx_ripple_t *ripples=(ot_fx_ripple_t*)fx_zone_scratch(
                                    state,max_ripples*sizeof(ot_fx_ripple_t));
    if(!ripples)return false;
    ot_wled_rgb_t bg=cfg->background;
    if(rainbow) {
        if(old_call==0u) {
            state->aux0=fx_rand8(state,256u);
            state->aux1=fx_rand8(state,256u);
        }
        if(state->aux0==state->aux1)state->aux1=fx_rand8(state,256u);
        else if(state->aux1>state->aux0)state->aux0++;
        else state->aux0--;
        bg=blend_colors(wheel_rgb((uint8_t)state->aux0),
                        (ot_wled_rgb_t){0,0,0},235u);
    }
    const ot_wled_rgb_t base=scaled(bg,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=base;
    for(size_t i=0;i<max_ripples;i++) {
        uint32_t ripple_state=ripples[i].ripple_state;
        if(ripple_state) {
            const uint32_t decay=(uint32_t)(cfg->speed>>4)+1u;
            const uint32_t propagation=((ripple_state/decay-1u)*
                                     ((uint32_t)cfg->speed+1u));
            const int32_t distance=(int32_t)(propagation>>8);
            const uint32_t fraction=propagation&255u;
            const uint32_t amplitude=ripple_state<17u?
                      fx_triwave8((uint8_t)((ripple_state-1u)*8u)):
                      255u-(ripple_state-17u)*253u/238u;
            const ot_wled_rgb_t color=scaled(
                    wheel_rgb(ripples[i].color),cfg->brightness);
            const int32_t left=(int32_t)ripples[i].pos-distance-1;
            const int32_t right=(int32_t)ripples[i].pos+distance+2;
            for(int v=0;v<4;v++) {
                const uint8_t wave=wled_cubicwave8((uint8_t)((fraction>>2)+
                                                                (uint32_t)v*64u));
                const uint8_t magnitude=(uint8_t)(
                              ((uint16_t)wave*(1u+amplitude))>>8);
                fx_ripple_blend(pixels,count,left+v,color,magnitude,255u);
                fx_ripple_blend(pixels,count,right-v,color,magnitude,255u);
            }
            ripple_state+=decay;
            ripples[i].ripple_state=(uint8_t)(ripple_state>254u?0u:ripple_state);
        } else if(fx_rand_index(state,15100u)<=cfg->intensity) {
            ripples[i].ripple_state=1u;
            ripples[i].pos=(uint16_t)fx_rand_index(state,count);
            ripples[i].color=fx_rand8(state,256u);
        }
    }
    return true;
}

/* Pinned WLED FX_fcn.cpp blur(16) 1D RGB three-channel adaptation:
 * keep=239, seep=8, carryover to next pixel. */
static ot_wled_rgb_t fx_scale256(ot_wled_rgb_t c,uint8_t amount) {
    return (ot_wled_rgb_t){
        (uint8_t)(((uint16_t)c.r*amount)>>8),
        (uint8_t)(((uint16_t)c.g*amount)>>8),
        (uint8_t)(((uint16_t)c.b*amount)>>8)};
}
static ot_wled_rgb_t fx_add_saturate(ot_wled_rgb_t a,ot_wled_rgb_t b) {
    const unsigned r=(unsigned)a.r+b.r,g=(unsigned)a.g+b.g,
                   blue=(unsigned)a.b+b.b;
    return (ot_wled_rgb_t){(uint8_t)(r>255u?255u:r),
                          (uint8_t)(g>255u?255u:g),
                          (uint8_t)(blue>255u?255u:blue)};
}
static void fx_blur16(ot_wled_rgb_t *pixels,size_t count) {
    if(!count)return;
    ot_wled_rgb_t carry=fx_scale256(pixels[0],8u);
    pixels[0]=fx_scale256(pixels[0],239u);
    for(size_t i=1;i<count;i++) {
        const ot_wled_rgb_t cur=pixels[i];
        const ot_wled_rgb_t part=fx_scale256(cur,8u);
        pixels[i]=fx_add_saturate(fx_scale256(cur,239u),carry);
        pixels[i-1u]=fx_add_saturate(pixels[i-1u],part);
        carry=part;
    }
}
/* Source FX.cpp mode_fireworks() with retained 1D framebuffer and sparse
 * random sparks. Source default palette=11 requires separate palette port. */
static void fireworks_frame(const ot_wled_fx_config_t *cfg,
                            ot_wled_fx_state_t *state,uint32_t old_call,
                            ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    if(old_call==0u){
        state->aux0=UINT16_MAX;
        state->aux1=UINT16_MAX;
    }
    fx_fade_previous(cfg,pixels,count,128u);
    if(state->step==0u) {
        const bool valid_a=state->aux0<count;
        const bool valid_b=state->aux1<count;
        const ot_wled_rgb_t old_a=valid_a?pixels[state->aux0]:
                                 (ot_wled_rgb_t){0,0,0};
        const ot_wled_rgb_t old_b=valid_b?pixels[state->aux1]:
                                 (ot_wled_rgb_t){0,0,0};
        fx_blur16(pixels,count);
        if(valid_a)pixels[state->aux0]=old_a;
        if(valid_b)pixels[state->aux1]=old_b;
    }
    const size_t bursts=count/20u?count/20u:1u;
    const uint32_t chance=129u-(cfg->intensity>>1);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t j=0;j<bursts;j++) {
        if(fx_rand8(state,chance)==0u) {
            const size_t index=fx_rand_index(state,count);
            (void)fx_rand8(state,256u); /* palette hue draw, mcol=0 */
            pixels[index]=fg;
            state->aux1=state->aux0;
            state->aux0=(uint16_t)index;
        }
    }
}
/* Source FX.cpp mode_rain() reuses the Fireworks renderer but scrolls the
 * retained framebuffer before adding new sparks. */
static void rain_frame(const ot_wled_fx_config_t *cfg,
                        ot_wled_fx_state_t *state,uint32_t old_call,
                        ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    state->step+=(uint32_t)cfg->frame_time_ms;
    const uint32_t delay=5u+(50u*(255u-(uint32_t)cfg->speed))/(uint32_t)count;
    if(old_call!=0u && state->step>delay) {
        state->step=1u;
        const ot_wled_rgb_t first=pixels[0];
        for(size_t i=0;i+1u<count;i++)pixels[i]=pixels[i+1u];
        pixels[count-1u]=first;
        state->aux0++;
        state->aux1++;
        if(state->aux0==0u)state->aux0=UINT16_MAX;
        if(state->aux1==0u)state->aux0=UINT16_MAX;
        if(state->aux0>=count)state->aux0=0u;
        if(state->aux1>=count)state->aux1=0u;
    }
    fireworks_frame(cfg,state,old_call,pixels,count);
}
/* Pinned util.cpp beatsin88_t(), plus FastLED hsv2rgb_rainbow RGB
 * color OR used by mode_juggle. */
static uint16_t fx_beatsin88(uint16_t bpm88,uint16_t upper,uint32_t now) {
    const uint16_t phase=(uint16_t)(
        ((uint32_t)(now*(uint32_t)bpm88*280u))>>16);
    const uint16_t sine=(uint16_t)((int32_t)wled_sin16(phase)+32768);
    return (uint16_t)(((uint32_t)sine*(1u+(uint32_t)upper))>>16);
}
/* Pinned fastled_slim.cpp hsv2rgb_rainbow() saturation / value path
 * reuses the existing exact hue wheel; hue is provided at (hue8<<8).
 * gamma32inv is identity when gammaCorrectCol is disabled, as it is in
 * CORE's current gamma-off RGB path. */
static ot_wled_rgb_t fx_hsv_rgb(uint8_t hue,uint8_t sat,uint8_t value) {
    ot_wled_rgb_t c=wheel_rgb(hue);
    if(sat!=255u) {
        if(sat==0u) {
            c=(ot_wled_rgb_t){255u,255u,255u};
        } else {
            uint32_t desat=(uint32_t)(255u-sat);
            desat=desat*desat;
            const uint8_t floor=(uint8_t)(desat>>8);
            const uint32_t factor=0xffffu-desat;
            c.r=(uint8_t)(((c.r?((uint32_t)c.r*factor)>>16:0u)+floor));
            c.g=(uint8_t)(((c.g?((uint32_t)c.g*factor)>>16:0u)+floor));
            c.b=(uint8_t)(((c.b?((uint32_t)c.b*factor)>>16:0u)+floor));
        }
    }
    if(value!=255u) {
        if(value==0u) c=(ot_wled_rgb_t){0u,0u,0u};
        else {
            const uint32_t v=(uint32_t)value*value+512u;
            c.r=(uint8_t)(c.r?(((uint32_t)c.r*v)>>16)+1u:0u);
            c.g=(uint8_t)(c.g?(((uint32_t)c.g*v)>>16)+1u:0u);
            c.b=(uint8_t)(c.b?(((uint32_t)c.b*v)>>16)+1u:0u);
        }
    }
    return c;
}

/* WLED colors.cpp color_fade(color, amount, true), packed RGB.
 * The source's dominant-channel rounding is intentional: replacing this
 * with linear scaling visibly changes the default-color Colorwaves path. */
/* WLED FX.cpp mode_bouncing_balls(): 1D RGB/palette=0, no AA and
 * no tertiary color. Original 12-byte Ball state and 16-ball ceiling are
 * algorithm constants, never a vehicle/output-specific LED count.
 * Every zone owns its own balls via CORE's existing scratch allocator.
 * Source hw_random8(5,11) maps to CORE's zone-local seeded RNG. */
typedef struct {
    uint32_t last_bounce_time; /* upstream 32-bit ESP32 unsigned long */
    float impact_velocity;
    float height;
} ot_bouncing_ball_t;

static void bouncing_balls_frame(const ot_wled_fx_config_t *cfg,
                                  ot_wled_fx_state_t *state,uint32_t now,
                                  uint32_t old_call,ot_wled_rgb_t *pixels,
                                  size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    ot_bouncing_ball_t *balls=(ot_bouncing_ball_t*)fx_zone_scratch(
        state,16u*sizeof(ot_bouncing_ball_t));
    if(!balls) {
        /* Upstream FX_FALLBACK_STATIC on segment allocation failure. */
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    const unsigned num_balls=(unsigned)cfg->intensity*15u/255u+1u;
    const float gravity=-9.81f;
    if(old_call==0u)
        for(size_t i=0;i<16u;i++)balls[i].last_bounce_time=now;
    for(size_t i=0;i<(size_t)num_balls;i++) {
        /* Upstream uses integer time division BEFORE converting to float. */
        const uint32_t elapsed=(uint32_t)(now-balls[i].last_bounce_time);
        const float elapsed_ms=(float)(elapsed/
            ((255u-(unsigned)cfg->speed)/64u+1u));
        const float seconds=elapsed_ms/1000.0f;
        balls[i].height=(0.5f*gravity*seconds+balls[i].impact_velocity)*seconds;
        if(balls[i].height<=0.0f) {
            balls[i].height=0.0f;
            const float dampening=0.9f-
                (float)i/(float)(num_balls*num_balls);
            balls[i].impact_velocity=dampening*balls[i].impact_velocity;
            balls[i].last_bounce_time=now;
            if(balls[i].impact_velocity<0.015f) {
                const float impulse=sqrtf(-2.0f*gravity)*
                    (float)(5u+fx_rand8(state,6u))/10.0f;
                balls[i].impact_velocity=impulse;
            }
        } else if(balls[i].height>1.0f) {
            continue;
        }
        /* roundf() on nonnegative 1D normalized height; avoid a libm
         * call for rounding, preserving round-half-away-from-zero. */
        const size_t pos=(size_t)(balls[i].height*(float)(count-1u)+0.5f);
        if(pos<count)pixels[pos]=fg;
    }
}


/* Direct pinned FX.cpp mode_rolling_balls() 1D default options port.
 * 16 zone-local balls, identical source random draw order and bounded
 * collision-free motion. check1/collide, check2/overlay, check3/trails and
 * nonzero palette are not exposed by this CORE config: never claim parity
 * for those unsupported controls. No vehicle geometry or global state. */
typedef struct {
    uint32_t last_bounce_update; /* ESP32 upstream unsigned long is uint32_t */
    float mass;
    float velocity;
    float height;
} ot_rolling_ball_t;
_Static_assert(sizeof(ot_rolling_ball_t)==16u,"Pinned rolling ball state layout");

static void rolling_balls_frame(const ot_wled_fx_config_t *cfg,
                                ot_wled_fx_state_t *state,uint32_t now,
                                uint32_t old_call,ot_wled_rgb_t *pixels,
                                size_t count) {
    ot_rolling_ball_t *balls=(ot_rolling_ball_t*)fx_zone_scratch(
        state,16u*sizeof(ot_rolling_ball_t));
    if(!balls) {
        /* WLED FX_FALLBACK_STATIC on segment data allocation failure. */
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    if(old_call==0u) {
        for(unsigned i=0u;i<16u;i++) {
            balls[i].last_bounce_update=now;
            balls[i].velocity=20.0f*(float)(1000u+fx_prng(state)%9000u)/10000.0f;
            if(fx_rand8(state,256u)<128u)balls[i].velocity=-balls[i].velocity;
            balls[i].height=(float)(fx_prng(state)%10000u)/10000.0f;
            balls[i].mass=(float)(1000u+fx_prng(state)%9000u)/10000.0f;
        }
    }
    /* Pinned FastLED scale8(8, 255-speed) before source float multiplier. */
    const uint32_t scale=(8u*(256u-(uint32_t)cfg->speed))>>8;
    const float cfac=(float)(scale+1u)*20000.0f;
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    const unsigned num_balls=(unsigned)cfg->intensity/16u+1u;
    for(unsigned i=0u;i<num_balls;i++) {
        const float delta=(float)(now-balls[i].last_bounce_update)/cfac;
        float height=balls[i].height+balls[i].velocity*delta;
        if(height< -0.5f || height>1.5f) {
            height=balls[i].height=(float)(fx_prng(state)%10000u)/10000.0f;
            balls[i].last_bounce_update=now;
        }
        if((height<=0.0f && balls[i].velocity<0.0f) ||
           (height>=1.0f && balls[i].velocity>0.0f)) {
            balls[i].velocity=-balls[i].velocity;
            balls[i].last_bounce_update=now;
            balls[i].height=height;
        }
        if(height<0.0f)height=0.0f;
        if(height>1.0f)height=1.0f;
        const size_t pos=(size_t)(height*(float)(count-1u)+0.5f);
        if(pos<count)pixels[pos]=fg;
        balls[i].last_bounce_update=now;
        balls[i].height=height;
    }
}


/* Pinned FX.cpp Spark (20 bytes), shared by source Popcorn, Fireworks,
 * and Drip. Keep exact layout and source per-frame update ordering. */
typedef struct {
    float pos, pos_x;
    float vel, vel_x;
    uint16_t col;
    uint8_t col_index;
} ot_fx_spark_t;
_Static_assert(sizeof(ot_fx_spark_t)==20u,"Pinned WLED Spark size");

static void popcorn_frame(const ot_wled_fx_config_t *cfg,
                          ot_wled_fx_state_t *state,ot_wled_rgb_t *pixels,
                          size_t count) {
    if(count<=1u) {
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    /* ESP32 source uses 21 kernels / virtual strip, and FAIR_DATA_PER_SEG
     * does not limit 420 bytes on a one-dimensional single-zone strip. */
    const unsigned max_popcorn=21u;
    ot_fx_spark_t *popcorn=(ot_fx_spark_t*)fx_zone_scratch(
        state,max_popcorn*sizeof(ot_fx_spark_t));
    if(!popcorn) {
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    float gravity=-0.0001f-(float)cfg->speed/200000.0f;
    gravity*=(float)count;
    unsigned num_popcorn=(unsigned)cfg->intensity*max_popcorn/255u;
    if(num_popcorn==0u)num_popcorn=1u;
    for(unsigned i=0u;i<num_popcorn;i++) {
        if(popcorn[i].pos>=0.0f) {
            popcorn[i].pos+=popcorn[i].vel;
            popcorn[i].vel+=gravity;
        } else if(fx_rand8(state,256u)<2u) {
            popcorn[i].pos=0.01f;
            unsigned peak=128u+fx_rand8(state,128u);
            peak=(peak*(unsigned)(count-1u))>>8;
            popcorn[i].vel=sqrtf(-2.0f*gravity*(float)peak);
            /* Palette=0, no tertiary: pinned source consumes a random
             * color-slot draw and maps every slot to primary. */
            (void)fx_rand8(state,3u);
            popcorn[i].col_index=0u;
        }
        if(popcorn[i].pos>=0.0f) {
            const size_t index=(size_t)popcorn[i].pos;
            if(index<count)pixels[index]=fg;
        }
    }
}


/* Pinned FX.cpp mode_drip(): shared 20-byte Spark state, 1D default
 * overlay=false, no physical bus assumptions. Keep source update order. */
static void drip_frame(const ot_wled_fx_config_t *cfg,
                       ot_wled_fx_state_t *state,ot_wled_rgb_t *pixels,
                       size_t count) {
    if(count<=1u) {
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    ot_fx_spark_t *drops=(ot_fx_spark_t*)fx_zone_scratch(
        state,4u*sizeof(ot_fx_spark_t));
    if(!drops) {
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    const ot_wled_rgb_t black={0,0,0};
    const ot_wled_rgb_t fg=cfg->foreground;
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    const unsigned num_drops=1u+((unsigned)cfg->intensity>>6);
    float gravity=-0.0005f-(float)cfg->speed/50000.0f;
    gravity*=(float)(count-1u);
    const unsigned swell=1u+((unsigned)cfg->speed*5u/255u);
    const unsigned source_drop=12u;
    for(unsigned j=0u;j<num_drops;j++) {
        if(drops[j].col_index==0u) {
            drops[j].pos=(float)(count-1u);
            drops[j].vel=0.0f;
            drops[j].col=source_drop;
            drops[j].col_index=1u;
        }
        pixels[count-1u]=scaled(blend_colors(black,fg,(uint8_t)source_drop),
                                cfg->brightness);
        if(drops[j].col_index==1u) {
            if(drops[j].col>255u)drops[j].col=255u;
            const size_t source=(size_t)drops[j].pos;
            if(source<count)pixels[source]=scaled(
                blend_colors(black,fg,(uint8_t)drops[j].col),cfg->brightness);
            drops[j].col=(uint16_t)(drops[j].col+swell);
            if(fx_rand8(state,256u)<(unsigned)drops[j].col/10u) {
                drops[j].col_index=2u;
                drops[j].col=255u;
            }
        }
        if(drops[j].col_index>1u) {
            if(drops[j].pos>0.0f) {
                drops[j].pos+=drops[j].vel;
                if(drops[j].pos<0.0f)drops[j].pos=0.0f;
                drops[j].vel+=gravity;
                for(unsigned i=1u;i<7u-(unsigned)drops[j].col_index;i++) {
                    size_t index=(size_t)drops[j].pos+i;
                    if(index>=count)index=count-1u;
                    pixels[index]=scaled(blend_colors(black,fg,
                        (uint8_t)(drops[j].col/i)),cfg->brightness);
                }
                if(drops[j].col_index>2u) {
                    pixels[0]=scaled(blend_colors(fg,black,
                       (uint8_t)drops[j].col),cfg->brightness);
                }
            } else if(drops[j].col_index>2u) {
                drops[j].col_index=0u;
                drops[j].col=source_drop;
            } else {
                if(drops[j].col_index==2u) {
                    drops[j].vel=-drops[j].vel/4.0f;
                    drops[j].pos+=drops[j].vel;
                }
                drops[j].col=source_drop*2u;
                drops[j].col_index=5u;
            }
        }
    }
}


/* Direct default-options 1D source port of pinned FX.cpp mode_tv_simulator.
 * No global scene, renderer allocation, tertiary colors or vehicle assumptions.
 * C layout preserves source TvSim fields; all timing/RNG is zone-owned. */
typedef struct {
    uint32_t total_time, fade_time, start_time, elapsed, pixel_num;
    uint16_t slider_values;
    uint32_t scene_start, scene_duration;
    uint16_t scene_color_hue;
    uint8_t scene_color_sat, scene_color_bri;
    uint8_t actual_r, actual_g, actual_b;
    uint16_t previous_r, previous_g, previous_b;
} ot_tv_state_t;

static uint16_t tv_rand16_range(ot_wled_fx_state_t *s,uint32_t lo,uint32_t hi) {
    return (uint16_t)(hi>lo ? lo+fx_prng(s)%(hi-lo) : lo);
}
static uint8_t tv_rand8_range(ot_wled_fx_state_t *s,uint32_t lo,uint32_t hi) {
    return (uint8_t)(hi>lo ? lo+fx_prng(s)%(hi-lo) : lo);
}
static int32_t tv_map(int32_t x,int32_t lo,int32_t hi,
                      int32_t dest_lo,int32_t dest_hi) {
    return (x-lo)*(dest_hi-dest_lo)/(hi-lo)+dest_lo;
}
static void tv_simulator_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               ot_wled_rgb_t *pixels,size_t count) {
    ot_tv_state_t *tv=(ot_tv_state_t*)fx_zone_scratch(state,sizeof(ot_tv_state_t));
    if(!tv) {
        const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    const uint8_t color_speed=(uint8_t)tv_map(cfg->speed,0,255,1,20);
    const uint8_t color_intensity=(uint8_t)tv_map(cfg->intensity,0,255,10,30);
    const unsigned sliders=((unsigned)cfg->speed<<8)|cfg->intensity;
    if(sliders!=tv->slider_values) {
        tv->slider_values=(uint16_t)sliders;
        state->aux1=0u;
    }
    if(now-tv->scene_start>=tv->scene_duration || state->aux1==0u) {
        tv->scene_start=now;
        tv->scene_duration=tv_rand16_range(state,60u*250u*color_speed,
                                                  60u*750u*color_speed);
        tv->scene_color_hue=tv_rand16_range(state,0u,768u);
        tv->scene_color_sat=tv_rand8_range(state,100u,130u+color_intensity);
        tv->scene_color_bri=tv_rand8_range(state,200u,240u);
        state->aux1=1u;
        state->aux0=0u;
    }
    if(state->aux0==0u) {
        uint8_t j=fx_rand8(state,4u*color_intensity);
        int hue=fx_rand8(state,256u)<128u ?
            ((j<tv->scene_color_hue)?(int)tv->scene_color_hue-(int)j:
                                      767-(int)tv->scene_color_hue-(int)j):
            (((int)j+(int)tv->scene_color_hue)<767 ?
                 (int)tv->scene_color_hue+(int)j:
                 (int)tv->scene_color_hue+(int)j-767);
        j=fx_rand8(state,2u*color_intensity);
        const uint8_t sat=(uint8_t)(((int)tv->scene_color_sat-(int)j)<0 ?
                             0:(int)tv->scene_color_sat-(int)j);
        j=fx_rand8(state,100u);
        const uint8_t bri=(uint8_t)(((int)tv->scene_color_bri-(int)j)<0 ?
                             0:(int)tv->scene_color_bri-(int)j);
        const uint8_t n=(uint8_t)((hue>>8)%3);
        const uint8_t x=(uint8_t)(((((hue&255)*(int)sat)>>8)*(int)bri)>>8);
        const uint8_t v=(uint8_t)(((256-(int)sat)*(int)bri)>>8);
        uint8_t temp[5]={v,(uint8_t)(x+v),(uint8_t)(bri-x),v,(uint8_t)(x+v)};
        tv->actual_r=temp[n+2u];
        tv->actual_g=temp[n+1u];
        tv->actual_b=temp[n];
    }
    const int nr=(int)tv->actual_r*257,ng=(int)tv->actual_g*257,
              nb=(int)tv->actual_b*257;
    if(state->aux0==0u) {
        state->aux0=1u;
        tv->total_time=tv_rand16_range(state,250u,2500u);
        tv->fade_time=tv_rand16_range(state,0u,tv->total_time);
        if(fx_rand8(state,10u)<3u)tv->fade_time=0u;
        tv->start_time=now;
    }
    tv->elapsed=now-tv->start_time;
    int r=nr,g=ng,b=nb;
    if(tv->elapsed<tv->fade_time) {
        r=tv_map((int)tv->elapsed,0,(int)tv->fade_time,tv->previous_r,nr);
        g=tv_map((int)tv->elapsed,0,(int)tv->fade_time,tv->previous_g,ng);
        b=tv_map((int)tv->elapsed,0,(int)tv->fade_time,tv->previous_b,nb);
    }
    const ot_wled_rgb_t color=scaled((ot_wled_rgb_t){
        (uint8_t)(r>>8),(uint8_t)(g>>8),(uint8_t)(b>>8)
    },cfg->brightness);
    for(size_t i=0;i<count;i++)pixels[i]=color;
    if(tv->elapsed>=tv->total_time) {
        tv->previous_r=(uint16_t)nr;
        tv->previous_g=(uint16_t)ng;
        tv->previous_b=(uint16_t)nb;
        state->aux0=0u;
    }
}

/* FX.cpp mode_halloween_eyes(): source 1D eye positions, fade, blink,
 * and on/off transitions. The RGB profile uses palette=0 (primary eye
 * color), no matrix or overlay options. Eye state belongs to the zone's
 * existing scratch storage; random draws use CORE's seeded zone stream. */
typedef struct {
    uint8_t state;
    uint8_t color;
    uint16_t start_pos;
    uint16_t duration;
    uint32_t start_time;
    uint32_t blink_end_time;
} ot_halloween_eyes_data_t;
_Static_assert(sizeof(ot_halloween_eyes_data_t)==16u,
               "Pinned WLED EyeData must remain 16 bytes");

static void halloween_eyes_frame(const ot_wled_fx_config_t *cfg,
                                  ot_wled_fx_state_t *state,uint32_t now,
                                  ot_wled_rgb_t *pixels,size_t count) {
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
    const ot_wled_rgb_t bg=scaled(cfg->background,cfg->brightness);
    unsigned eye_space=(unsigned)(count>>5);
    if(eye_space<2u)eye_space=2u;
    const unsigned eye_width=eye_space/2u;
    const unsigned eye_length=2u*eye_width+eye_space;
    if(count<=1u || eye_length>=count) {
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    ot_halloween_eyes_data_t *data=
       (ot_halloween_eyes_data_t*)fx_zone_scratch(state,sizeof(*data));
    if(!data) {
        for(size_t i=0;i<count;i++)pixels[i]=fg;
        return;
    }
    for(size_t i=0;i<count;i++)pixels[i]=bg;
    data->state=(uint8_t)(data->state%5u);
    unsigned duration=data->duration?data->duration:1u;
    const uint32_t elapsed=(uint32_t)(now-data->start_time);
    switch(data->state) {
      case 0u: /* initializeOn, then immediately render on */
        data->start_pos=(uint16_t)fx_rand_index(
                            state,count-(size_t)eye_length-1u);
        data->color=fx_rand8(state,256u);
        duration=128u+fx_rand_index(state,(size_t)cfg->intensity*64u);
        data->duration=(uint16_t)duration;
        data->state=1u;
        /* fall through */
      case 1u: { /* on */
        const unsigned second=(unsigned)data->start_pos+eye_width+eye_space;
        const unsigned limit=128u+(unsigned)cfg->intensity*64u;
        if(duration>limit)duration=limit;
        const uint32_t fade=((uint32_t)(elapsed*2048u))/duration;
        ot_wled_rgb_t c=fg;
        if(fade<256u)c=scaled(blend_colors(cfg->background,cfg->foreground,
                                          (uint8_t)fade),cfg->brightness);
        else if(elapsed>1024u) {
            const uint32_t remaining=elapsed>=duration?0u:duration-elapsed;
            if(remaining>1024u && fx_rand8(state,256u)<4u) {
                c=bg;data->state=2u;
                data->blink_end_time=now+8u+fx_rand8(state,120u);
            }
        }
        if(c.r!=bg.r||c.g!=bg.g||c.b!=bg.b) {
            for(unsigned i=0;i<eye_width;i++) {
                pixels[data->start_pos+i]=c;
                pixels[second+i]=c;
            }
        }
        break;
      }
      case 2u: /* blink */
        if(now>=data->blink_end_time)data->state=1u;
        break;
      case 3u: { /* initializeOff, then fall through */
        const unsigned base=(unsigned)cfg->speed*128u;
        duration=base+fx_rand_index(state,base);
        data->duration=(uint16_t)duration;
        data->state=4u;
      }
      __attribute__((fallthrough)); /* Intentional source-equivalent transition to off. */
      case 4u: { /* off */
        const unsigned limit=(unsigned)cfg->speed*256u;
        if(duration>limit)duration=limit;
        break;
      }
      default:
        data->state=0u;
        break;
    }
    if(elapsed>duration) {
        data->state=(data->state==0u||data->state==1u||data->state==2u)
                    ?3u:0u;
        data->start_time=now;
    }
}

static ot_wled_rgb_t fx_video_fade(ot_wled_rgb_t input,uint8_t amount);

/* WLED FX.cpp Fairy uses a four-byte flasher per active sparkle. Reuse
 * CORE's existing per-zone scratch allocation and source shimmer-zone
 * grouping. The upstream hw_random8 draws use the existing seeded CORE
 * per-zone RNG until hardware RNG injection is implemented; randomized
 * event sequences are therefore not yet certified as 1:1. */
typedef struct {
    uint16_t state_start;
    uint8_t state_dur;
    uint8_t state_on;
} ot_fairy_flasher_t;

static void fairy_frame(const ot_wled_fx_config_t *cfg,
                        ot_wled_fx_state_t *state,uint32_t now,
                        ot_wled_rgb_t *pixels,size_t count) {
    uint16_t seed=5100u; /* WLED getCurrSegmentId()==0 reference */
    const ot_wled_rgb_t primary=scaled(cfg->foreground,cfg->brightness);
    for(size_t i=0;i<count;i++) {
        seed=(uint16_t)((uint32_t)seed*2053u+1384u);
        pixels[i]=primary; /* WLED palette=0, mcol=0 */
    }
    if(cfg->intensity==0u)return;
    const size_t distance=((255u-(unsigned)cfg->intensity)/28u)+1u;
    const size_t flashers_count=count/distance+1u;
    if(flashers_count>SIZE_MAX/sizeof(ot_fairy_flasher_t))return;
    ot_fairy_flasher_t *flashers=(ot_fairy_flasher_t*)fx_zone_scratch(
         state,flashers_count*sizeof(ot_fairy_flasher_t));
    if(!flashers)return; /* WLED leaves the initialized background */
    const uint16_t now16=(uint16_t)now;

    /* WLED FLASHERS_PER_ZONE=6, MAX_SHIMMER=92. With 6+ flashers
     * the last zone may contain up to 11 positions. */
    size_t zones=flashers_count/6u;
    if(!zones)zones=1u;
    size_t in_zone=flashers_count/zones;
    for(size_t z=0;z<zones;z++) {
        const size_t first=z*in_zone;
        const size_t n=(z==zones-1u)?flashers_count-first:in_zone;
        unsigned brightness[11]={0u};
        unsigned sum=0u;
        if(n>11u)return; /* avoid an unsafe buffer on future changes */
        for(size_t j=0;j<n;j++) {
            ot_fairy_flasher_t *fl=&flashers[first+j];
            unsigned age=(uint16_t)(now16-fl->state_start);
            if(age>(unsigned)fl->state_dur*10u) {
                fl->state_on=(uint8_t)!fl->state_on;
                if(fl->state_on)
                    fl->state_dur=(uint8_t)(12u+fx_rand8(state,
                            12u+((255u-(unsigned)cfg->speed)>>2)));
                else
                    fl->state_dur=(uint8_t)(20u+fx_rand8(state,
                            6u+((255u-(unsigned)cfg->speed)>>2)));
                fl->state_start=now16;
                if(age<255u) {
                    fl->state_start=(uint16_t)(fl->state_start-
                                             (uint16_t)(255u-age));
                    fl->state_dur=(uint8_t)((unsigned)fl->state_dur+
                                          26u-age/10u);
                    age=255u-age;
                } else age=0u;
            }
            if(age>255u)age=255u;
            brightness[j]=fl->state_on?age:255u-age;
            sum+=brightness[j];
        }
        const unsigned avg=sum/(unsigned)n;
        const unsigned peak=255u-((avg*92u)>>8);
        const ot_wled_rgb_t dim=fx_video_fade(cfg->foreground,(uint8_t)peak);
        for(size_t j=0;j<n;j++) {
            const unsigned bri=(brightness[j]*peak)/255u;
            seed=(uint16_t)((uint32_t)seed*2053u+1384u);
            const size_t pos=(first+j)*distance;
            if(pos<count) {
                pixels[pos]=scaled(blend_colors(cfg->background,
                                      cfg->foreground,(uint8_t)bri),
                                      cfg->brightness);
            }
            for(size_t i=pos+1u;i<pos+distance&&i<count;i++) {
                seed=(uint16_t)((uint32_t)seed*2053u+1384u);
                pixels[i]=scaled(dim,cfg->brightness);
            }
        }
    }
}

/* WLED FX.cpp mode_fairytwinkle() shares the four-byte Fairy flasher
 * state and per-zone allocator. The original mode explicitly applies
 * gamma8(), independent of the bus gamma switch. This pinned WLED 2.2
 * lookup curve preserves the default effect envelope; user-configurable
 * gamma remains an explicit later engine-parity requirement. */
static const uint8_t fx_wled_gamma_22[256]={
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
      3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,
      6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,  12,
     12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
     20,  20,  21,  22,  22,  23,  23,  24,  25,  25,  26,  26,  27,  28,  28,  29,
     30,  30,  31,  32,  33,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,
     42,  43,  43,  44,  45,  46,  47,  48,  49,  49,  50,  51,  52,  53,  54,  55,
     56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,
     73,  74,  75,  76,  77,  78,  79,  81,  82,  83,  84,  85,  87,  88,  89,  90,
     91,  93,  94,  95,  97,  98,  99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
    113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
    137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
    163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
    192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
    223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255
};

static void fairytwinkle_frame(const ot_wled_fx_config_t *cfg,
                             ot_wled_fx_state_t *state,uint32_t now,
                             ot_wled_rgb_t *pixels,size_t count) {
    if(count>SIZE_MAX/sizeof(ot_fairy_flasher_t))return;
    ot_fairy_flasher_t *flashers=(ot_fairy_flasher_t*)fx_zone_scratch(
                                state,count*sizeof(ot_fairy_flasher_t));
    if(!flashers) {
        for(size_t i=0;i<count;i++)
            pixels[i]=scaled(cfg->foreground,cfg->brightness);
        return;
    }
    const uint16_t now16=(uint16_t)now;
    uint16_t seed=5100u; /* pinned segment-0 PRNG16 starting value */
    const unsigned rise_fall=400u+(255u-(unsigned)cfg->speed)*3u;
    const unsigned max_duration=rise_fall/100u+
             ((255u-(unsigned)cfg->intensity)>>2)+13u+
             ((255u-(unsigned)cfg->intensity)>>1);
    for(size_t i=0;i<count;i++) {
        ot_fairy_flasher_t *fl=&flashers[i];
        unsigned age=(uint16_t)(now16-fl->state_start);
        if(age>(unsigned)fl->state_dur*100u) {
            fl->state_on=(uint8_t)!fl->state_on;
            const bool init=fl->state_dur==0u;
            if(fl->state_on)
                fl->state_dur=(uint8_t)(rise_fall/100u+
                    ((255u-(unsigned)cfg->intensity)>>2)+
                    fx_rand8(state,12u+((255u-(unsigned)cfg->intensity)>>1))+1u);
            else
                fl->state_dur=(uint8_t)(rise_fall/100u+
                    fx_rand8(state,3u+((255u-(unsigned)cfg->speed)>>6))+1u);
            fl->state_start=now16;
            age=0u;
            if(init) {
                fl->state_start=(uint16_t)(fl->state_start-(uint16_t)rise_fall);
                fl->state_dur=(uint8_t)(rise_fall/100u+
                    fx_rand8(state,12u+((255u-(unsigned)cfg->intensity)>>1))+5u);
                age=rise_fall;
            }
        }
        if(fl->state_on && (unsigned)fl->state_dur>max_duration)
            fl->state_dur=(uint8_t)max_duration;
        if(age>rise_fall)age=rise_fall;
        const unsigned fade_prog=255u-(age*255u)/rise_fall;
        const uint8_t gamma=fx_wled_gamma_22[fade_prog];
        const uint8_t brightness=fl->state_on?(uint8_t)(255u-gamma):gamma;
        const uint16_t last_seed=seed;
        unsigned diff=0u;
        do {
            seed=(uint16_t)((uint32_t)seed*2053u+1384u);
            diff=(seed>last_seed)?(unsigned)(seed-last_seed):
                                  (unsigned)(last_seed-seed);
        } while(diff<0x4000u);
        pixels[i]=scaled(blend_colors(cfg->background,cfg->foreground,
                                      brightness),cfg->brightness);
    }
}

static ot_wled_rgb_t fx_video_fade(ot_wled_rgb_t input,uint8_t amount) {
    const uint32_t c=((uint32_t)input.r<<16)|((uint32_t)input.g<<8)|input.b;
    if(c==0u || amount==0u)return (ot_wled_rgb_t){0u,0u,0u};
    if(amount==255u)return input;
    const uint32_t mask=0x00ff00ffu;
    const uint32_t rb=c&mask,wg=(c>>8)&mask;
    uint8_t maxc=input.r>input.g?input.r:input.g;
    if(input.b>maxc)maxc=input.b;
    uint32_t threshold=(uint32_t)((maxc>>1)+1u);
    threshold|=threshold<<16;
    const uint32_t dominant_rb=(((rb|0x80008000u)-threshold)&0x80008000u)>>15;
    const uint32_t dominant_wg=(((wg|0x80008000u)-threshold)&0x80008000u)>>7;
    const uint32_t scaled_rb=((((uint32_t)rb*amount)>>8)&mask)+dominant_rb;
    const uint32_t scaled_wg=(((uint32_t)wg*amount)&~mask)+dominant_wg;
    const uint32_t out=scaled_rb|scaled_wg;
    return (ot_wled_rgb_t){(uint8_t)(out>>16),(uint8_t)(out>>8),(uint8_t)out};
}

/* Pinned FX.cpp mode_colorwaves_pride_base(true/false), with per-zone
 * pseudotime/hue state. Palette=0 Colorwaves uses source video-fade FG.
 * This routine is shared rather than duplicating two almost identical
 * 1D animation engines. */
static void colorwaves_pride_frame(const ot_wled_fx_config_t *cfg,
                            ot_wled_fx_state_t *state,uint32_t now,
                            ot_wled_rgb_t *pixels,size_t count,bool pride) {
    const uint32_t duration=10u+(uint32_t)cfg->speed;
    const uint8_t sat=pride?(uint8_t)(220u+fx_beatsin88(87u,30u,now)):255u;
    const unsigned depth=96u+fx_beatsin88(341u,128u,now);
    const unsigned theta_inc=25u*256u+fx_beatsin88(203u,15u*256u,now);
    const unsigned multiplier=23u+fx_beatsin88(147u,37u,now);
    unsigned hue=state->aux0;
    const unsigned hue_inc=pride?(1u+fx_beatsin88(113u,2999u,now)):
       ((60u+(unsigned)fx_beatsin88(113u,240u,now))*
        (unsigned)cfg->intensity*10u/255u);
    uint32_t pseudo=state->step+duration*multiplier;
    state->aux0=(uint16_t)((uint32_t)state->aux0+
                           duration*(5u+fx_beatsin88(400u,4u,now)));
    unsigned theta=pseudo;
    for(size_t i=0;i<count;i++) {
        hue+=hue_inc;
        uint8_t hue8;
        if(pride) hue8=(uint8_t)(hue>>8);
        else {
            const unsigned h128=hue>>7;
            hue8=(uint8_t)((h128&0x100u)?(255u-(h128>>1)):(h128>>1));
        }
        theta+=theta_inc;
        const uint32_t sine=(uint32_t)((int32_t)wled_sin16((uint16_t)theta)+32768);
        const uint32_t bright16=(sine*sine)>>16;
        uint8_t bri=(uint8_t)((bright16*(uint32_t)depth)>>16);
        bri=(uint8_t)(bri+255u-depth);
        const ot_wled_rgb_t next=pride?fx_hsv_rgb(hue8,sat,bri):
                                          fx_video_fade(cfg->foreground,bri);
        pixels[i]=scaled(blend_colors(pixels[i],next,pride?64u:128u),cfg->brightness);
    }
    state->step=pseudo;
}

static ot_wled_rgb_t fx_hsv220(uint8_t hue) {
    ot_wled_rgb_t c=wheel_rgb(hue);
    const uint32_t desat=(255u-220u)*(255u-220u);
    const uint32_t floor=desat>>8;
    const uint32_t factor=65535u-desat;
    c.r=(uint8_t)((c.r?((uint32_t)c.r*factor)>>16:0u)+floor);
    c.g=(uint8_t)((c.g?((uint32_t)c.g*factor)>>16:0u)+floor);
    c.b=(uint8_t)((c.b?((uint32_t)c.b*factor)>>16:0u)+floor);
    return c;
}
static void juggle_frame(const ot_wled_fx_config_t *cfg,uint32_t now,
                         ot_wled_rgb_t *pixels,size_t count) {
    if(count<=1u) {
        pixels[0]=scaled(cfg->foreground,cfg->brightness);return;
    }
    const unsigned fade=192u-(3u*(unsigned)cfg->intensity/4u);
    for(size_t i=0;i<count;i++)
        pixels[i]=fx_scale256(pixels[i],(uint8_t)(255u-fade));
    uint8_t hue=0u;
    for(unsigned i=0u;i<8u;i++) {
        const uint16_t bpm=(uint16_t)((16u+(uint32_t)cfg->speed)*(i+7u));
        const size_t index=fx_beatsin88(bpm,(uint16_t)(count-1u),now);
        const ot_wled_rgb_t c=fx_hsv220(hue);
        const ot_wled_rgb_t dot=scaled(c,cfg->brightness);
        const unsigned r=pixels[index].r|dot.r;
        const unsigned g=pixels[index].g|dot.g;
        const unsigned b=pixels[index].b|dot.b;
        /* The bitwise OR of already scaled dots can exceed the output cap.
         * Bound only this compositing step; preserve the source OR pattern. */
        pixels[index]=(ot_wled_rgb_t){
            (uint8_t)(r>cfg->brightness?cfg->brightness:r),
            (uint8_t)(g>cfg->brightness?cfg->brightness:g),
            (uint8_t)(b>cfg->brightness?cfg->brightness:b)};
        hue=(uint8_t)(hue+32u);
    }
}


/*
 * Verbatim integer Perlin gradient/smoothing algorithms from pinned WLED
 * util.cpp, renamed to avoid C++ overloads. Shared by five FX.cpp modes.
 * Source EUPL-1.2-or-later (see upstream copyright notice).
 * Core palette=0 still uses primary FG; colored gradients come later.
 */
#define PERLIN_SHIFT 1
static inline int32_t hashToGradient(uint32_t h) {
  // using more steps yields more "detailed" perlin noise but looks less like the original fastled version (adjust PERLIN_SHIFT to compensate, also changes range and needs proper adustment)
  // return (h & 0xFF) - 128; // use PERLIN_SHIFT 7
  // return (h & 0x0F) - 8; // use PERLIN_SHIFT 3
  // return (h & 0x07) - 4; // use PERLIN_SHIFT 2
  return (h & 0x03) - 2; // use PERLIN_SHIFT 1 -> closest to original fastled version
}



static inline int32_t gradient2D(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy) {
  uint32_t h = (x0 * 0x27D4EB2D) ^ (y0 * 0xB5297A4D);
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  return (hashToGradient(h) * dx + hashToGradient(h>>PERLIN_SHIFT) * dy) >> (1 + PERLIN_SHIFT);
}

static inline int32_t gradient3D(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy, uint32_t z0, int32_t dz) {
  // fast and good entropy hash from corner coordinates
  uint32_t h = (x0 * 0x27D4EB2D) ^ (y0 * 0xB5297A4D) ^ (z0 * 0x1B56C4E9);
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  return ((hashToGradient(h) * dx + hashToGradient(h>>(1+PERLIN_SHIFT)) * dy + hashToGradient(h>>(1 + 2*PERLIN_SHIFT)) * dz) * 85) >> (8 + PERLIN_SHIFT); // scale to 16bit, x*85 >> 8 = x/3
}

static uint32_t smoothstep(const uint32_t t) {
  uint32_t t_squared = (t * t) >> 16;
  uint32_t factor = (3 << 16) - ((t << 1));
  return (t_squared * factor) >> 18; // scale to avoid overflows and give best resolution
}

static inline int32_t lerpPerlin(int32_t a, int32_t b, int32_t t) {
    return a + (((b - a) * t) >> 14); // match scaling with smoothstep to yield 16.16bit values
}



/* Pinned WLED util.cpp perlin1D_raw() and perlin8(uint16_t).
 * Reuse the existing hash, smoothstep and interpolation functions.
 * Phased Noise is a true 1D field; the 2D substitute is incorrect. */
static inline int32_t fx_gradient1D(uint32_t x0,int32_t dx) {
    uint32_t h=x0*0x27D4EB2Du;
    h^=h>>15;h*=0x92C3412Bu;h^=h>>13;h^=h>>7;
    return (hashToGradient(h)*dx)>>PERLIN_SHIFT;
}
static int32_t fx_perlin1D_raw(uint32_t x,bool is16bit) {
    int32_t x0=(int32_t)(x>>16);
    int32_t x1=x0+1;
    if(is16bit)x1&=0xff;
    int32_t dx0=(int32_t)(x&0xffffu),dx1=dx0-0x10000;
    const int32_t g0=fx_gradient1D((uint32_t)x0,dx0);
    const int32_t g1=fx_gradient1D((uint32_t)x1,dx1);
    return lerpPerlin(g0,g1,(int32_t)smoothstep((uint32_t)dx0));
}
static uint8_t fx_perlin8_1(uint16_t x) {
    return (uint8_t)((((fx_perlin1D_raw((uint32_t)x<<8,true)*1353)>>10)
                       +32769)>>8);
}

static int32_t fx_perlin2D_raw(uint32_t x, uint32_t y, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;

  if(is16bit) {
    x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF
    y1 = y1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;

  int32_t g00 = gradient2D(x0, dx0, y0, dy0);
  int32_t g10 = gradient2D(x1, dx1, y0, dy0);
  int32_t g01 = gradient2D(x0, dx0, y1, dy1);
  int32_t g11 = gradient2D(x1, dx1, y1, dy1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);

  int32_t nx0 = lerpPerlin(g00, g10, tx);
  int32_t nx1 = lerpPerlin(g01, g11, tx);

  int32_t noise = lerpPerlin(nx0, nx1, ty);
  return noise;
}

static int32_t fx_perlin3D_raw(uint32_t x, uint32_t y, uint32_t z, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t z0 = z >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;
  int32_t z1 = z0 + 1;

  if(is16bit) {
    x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF
    y1 = y1 & 0xFF;
    z1 = z1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dz0 = z & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;
  int32_t dz1 = dz0 - 0x10000;

  int32_t g000 = gradient3D(x0, dx0, y0, dy0, z0, dz0);
  int32_t g001 = gradient3D(x0, dx0, y0, dy0, z1, dz1);
  int32_t g010 = gradient3D(x0, dx0, y1, dy1, z0, dz0);
  int32_t g011 = gradient3D(x0, dx0, y1, dy1, z1, dz1);
  int32_t g100 = gradient3D(x1, dx1, y0, dy0, z0, dz0);
  int32_t g101 = gradient3D(x1, dx1, y0, dy0, z1, dz1);
  int32_t g110 = gradient3D(x1, dx1, y1, dy1, z0, dz0);
  int32_t g111 = gradient3D(x1, dx1, y1, dy1, z1, dz1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);
  uint32_t tz = smoothstep(dz0);

  int32_t nx0 = lerpPerlin(g000, g100, tx);
  int32_t nx1 = lerpPerlin(g010, g110, tx);
  int32_t nx2 = lerpPerlin(g001, g101, tx);
  int32_t nx3 = lerpPerlin(g011, g111, tx);
  int32_t ny0 = lerpPerlin(nx0, nx1, ty);
  int32_t ny1 = lerpPerlin(nx2, nx3, ty);

  int32_t noise = lerpPerlin(ny0, ny1, tz);
  return noise;
}



static uint16_t fx_perlin16_2(uint32_t x, uint32_t y) {
 return ((fx_perlin2D_raw(x, y, false) * 1537) >> 10) + 32725; //scale to 16bit and offset (fastled range: about 1748 to 63697)
}

static uint16_t fx_perlin16_3(uint32_t x, uint32_t y, uint32_t z) {
  return ((fx_perlin3D_raw(x, y, z, false) * 1731) >> 10) + 33147; //scale to 16bit and offset (fastled range: about 4766 to 60840)
}



static uint8_t fx_perlin8_2(uint16_t x, uint16_t y) {
  return (((fx_perlin2D_raw((uint32_t)x << 8, (uint32_t)y << 8, true) * 1620) >> 10) + 32771) >> 8; //scale to 16 bit, offset, then scale to 8bit
}
#undef PERLIN_SHIFT


/* Pinned mode_ColorClouds() for 1D RGB, palette=0, default custom
 * c1=48,c2=64,c3=12, Cozy=false. Reuses the original integer Perlin2D
 * helper, FastLED HSV conversion and per-zone seeded scene offsets.
 * Additional custom settings require a separately versioned CORE config. */
static void color_clouds_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               uint32_t old_call,ot_wled_rgb_t *pixels,
                               size_t count) {
    if(old_call==0u) {
        state->aux0=(uint16_t)fx_prng(state);
        state->aux1=(uint16_t)fx_prng(state);
    }
    const uint32_t vol_x0=state->aux0, hue_x0=state->aux1;
    const uint8_t hue_offset0=(uint8_t)(vol_x0+hue_x0);
    const uint32_t vol_speed=1u+(uint32_t)cfg->speed;
    const uint32_t hue_speed=1u+(uint32_t)cfg->intensity;
    const uint32_t vol_squeeze=8u+48u, hue_squeeze=64u;
    const int32_t vol_cutoff=12500+12*900;
    const int32_t vol_saturate=52000;
    const uint32_t vol_t=now*vol_speed/8u;
    const uint32_t hue_t=now*hue_speed/8u;
    const uint8_t hue_offset=(uint8_t)(
        (uint16_t)((now*64u*280u)>>16)>>8);
    for(size_t i=0;i<count;i++) {
        const uint32_t vol_x=(uint32_t)i*vol_squeeze*64u;
        int32_t vol=(int32_t)fx_perlin16_2(vol_x0+vol_x,vol_t);
        vol=(vol-vol_cutoff)*255/(vol_saturate-vol_cutoff);
        if(vol<0)vol=0;
        if(vol>255)vol=255;
        const uint32_t hue_x=(uint32_t)i*hue_squeeze*8u;
        uint8_t hue=(uint8_t)(fx_perlin16_2(hue_x0+hue_x,hue_t)>>7);
        hue=(uint8_t)(hue+hue_offset0+hue_offset);
        ot_wled_rgb_t rgb=fx_hsv_rgb(hue,255u,(uint8_t)vol);
        if((int)rgb.r+(int)rgb.g+(int)rgb.b<=2)
            rgb=(ot_wled_rgb_t){0,0,0};
        pixels[i]=scaled(rgb,cfg->brightness);
    }
}

/* Upstream FX.cpp Fill Noise, Noise 1–4 1D source animations. No WLED
 * runtime, Arduino global state or external LED bus is linked. */
static void perlin_noise_frame(const ot_wled_fx_config_t *cfg,
                               ot_wled_fx_state_t *state,uint32_t now,
                               uint32_t old_call,ot_wled_rgb_t *pixels,
                               size_t count) {
    if(cfg->effect_id==OT_WLED_FILL_NOISE) {
        if(old_call==0u)state->step=fx_prng(state);
        for(size_t i=0;i<count;i++) {
            const uint8_t index=fx_perlin8_2((uint16_t)((uint32_t)i*count),
                                   (uint16_t)(state->step+(uint32_t)i*count));
            (void)index;
            pixels[i]=scaled(cfg->foreground,cfg->brightness);
        }
        state->step+=fx_beatsin8(cfg->speed,1u,6u,now);
        return;
    }
    if(cfg->effect_id==OT_WLED_NOISE_1) {
        state->step+=(uint32_t)(1u+cfg->speed/16u);
        const uint32_t shift_x=fx_beatsin8(11u,0u,255u,now);
        for(size_t i=0;i<count;i++) {
            const uint32_t sx=((uint32_t)i+shift_x)*320u;
            const uint32_t sy=((uint32_t)i+state->step/42u)*320u;
            const uint8_t noise=(uint8_t)(fx_perlin16_3(sx,sy,state->step)>>8);
            const uint8_t index=wled_sin8((uint8_t)(noise*3u));
            (void)index;
            pixels[i]=scaled(cfg->foreground,cfg->brightness);
        }
        return;
    }
    if(cfg->effect_id==OT_WLED_NOISE_2) {
        state->step+=1u+((uint32_t)cfg->speed>>1);
        for(size_t i=0;i<count;i++) {
            const uint32_t sx=((uint32_t)i+(state->step>>6))*1000u;
            const uint8_t noise=(uint8_t)(fx_perlin16_3(sx,0u,4223u)>>8);
            const uint8_t index=wled_sin8((uint8_t)(noise*3u));
            (void)index;
            pixels[i]=scaled(scaled(cfg->foreground,noise),cfg->brightness);
        }
        return;
    }
    if(cfg->effect_id==OT_WLED_NOISE_3) {
        state->step+=1u+(uint32_t)cfg->speed;
        for(size_t i=0;i<count;i++) {
            const uint32_t sx=((uint32_t)i+4223u)*800u;
            const uint32_t sy=((uint32_t)i+1234u)*800u;
            const uint8_t noise=(uint8_t)(fx_perlin16_3(sx,sy,state->step*8u)>>8);
            const uint8_t index=wled_sin8((uint8_t)(noise*3u));
            (void)index;
            pixels[i]=scaled(scaled(cfg->foreground,noise),cfg->brightness);
        }
        return;
    }
    const uint32_t phase=(now*(uint32_t)cfg->speed)>>7;
    for(size_t i=0;i<count;i++) {
        const uint16_t index=fx_perlin16_2((uint32_t)i<<12,phase);
        (void)index;
        pixels[i]=scaled(cfg->foreground,cfg->brightness);
    }
}

bool ot_wled_fx_render(const ot_wled_fx_config_t *cfg, ot_wled_fx_state_t *state,
                       uint32_t time_ms, ot_wled_rgb_t *pixels, size_t count) {
    if (!cfg || !state || !pixels || count==0 || cfg->frame_time_ms==0 || cfg->palette_id!=0)
        return false;
    ot_wled_fx_metadata_t meta;
    if (!ot_wled_fx_metadata(cfg->effect_id, &meta)) return false;

    if (count>65535u) return false; /* 16-bit WLED segment indexing boundary */
    /* Absolute-off is a CORE controller invariant, not an upstream FX
     * animation output. Retained trails must never leak light at zero
     * brightness; no source state or prior frame can bypass this. */
    if(cfg->brightness==0u) {
        for(size_t i=0;i<count;i++)pixels[i]=(ot_wled_rgb_t){0,0,0};
        state->call++;
        return true;
    }
    /* Per-zone WLED SEGENV.call advances once per rendered frame; capture
     * the old count because rainbow blink/strobe read it BEFORE advancing. */
    const uint32_t call=state->call++;
    if(cfg->effect_id==OT_WLED_COLOR_CLOUDS) {
        color_clouds_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_FILL_NOISE ||
       cfg->effect_id==OT_WLED_NOISE_1 ||
       cfg->effect_id==OT_WLED_NOISE_2 ||
       cfg->effect_id==OT_WLED_NOISE_3 ||
       cfg->effect_id==OT_WLED_NOISE_4) {
        perlin_noise_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_FIREWORKS) {
        fireworks_frame(cfg,state,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_RAIN) {
        rain_frame(cfg,state,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_JUGGLE) {
        juggle_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_RIPPLE ||
       cfg->effect_id==OT_WLED_RIPPLE_RAINBOW)
        return ripple_frame(cfg,state,time_ms,call,pixels,count,
                            cfg->effect_id==OT_WLED_RIPPLE_RAINBOW);
    if(cfg->effect_id==OT_WLED_RAILWAY) {
        railway_frame(cfg,state,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_WASHING_MACHINE) {
        washing_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_FLOW) {
        flow_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_SUNRISE) {
        sunrise_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_TWINKLEUP) {
        twinkleup_frame(cfg,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_CANDLE_MULTI)
        return candle_multi_frame(cfg,state,time_ms,pixels,count);
    if(cfg->effect_id==OT_WLED_METEOR)
        return meteor_frame(cfg,state,time_ms,pixels,count);
    if(cfg->effect_id==OT_WLED_DYNAMIC ||
       cfg->effect_id==OT_WLED_DYNAMIC_SMOOTH)
        return dynamic_frame(cfg,state,time_ms,call,pixels,count,
                             cfg->effect_id==OT_WLED_DYNAMIC_SMOOTH);
    if(cfg->effect_id==OT_WLED_DISSOLVE ||
       cfg->effect_id==OT_WLED_DISSOLVE_RANDOM)
        return dissolve_frame(cfg,state,time_ms,call,pixels,count,
                              cfg->effect_id==OT_WLED_DISSOLVE_RANDOM);
    if(cfg->effect_id==OT_WLED_CANDLE) {
        candle_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_TWINKLE) {
        twinkle_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_CHASE_FLASH_RANDOM) {
        chase_flash_random_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_MULTI_COMET) {
        multi_comet_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_CHUNCHUN) {
        chunchun_frame(cfg,time_ms,pixels,count);
        return true;
    }
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
    if(cfg->effect_id==OT_WLED_HALLOWEEN_EYES) {
        halloween_eyes_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_TV_SIMULATOR) {
        tv_simulator_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_DRIP) {
        drip_frame(cfg,state,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_POPCORN) {
        popcorn_frame(cfg,state,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_ROLLING_BALLS) {
        rolling_balls_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_BOUNCING_BALLS) {
        bouncing_balls_frame(cfg,state,time_ms,call,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_FAIRYTWINKLE) {
        fairytwinkle_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_FAIRY) {
        fairy_frame(cfg,state,time_ms,pixels,count);
        return true;
    }
    if(cfg->effect_id==OT_WLED_PRIDE_2015 ||
       cfg->effect_id==OT_WLED_COLORWAVES) {
        colorwaves_pride_frame(cfg,state,time_ms,pixels,count,
                              cfg->effect_id==OT_WLED_PRIDE_2015);
        return true;
    }
    if(cfg->effect_id==OT_WLED_PHASED ||
       cfg->effect_id==OT_WLED_PHASED_NOISE) {
        phased_frame(cfg,state,pixels,count,
                     cfg->effect_id==OT_WLED_PHASED_NOISE);
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
