/*
 * Isolated adaptation of WLED wled00/FX.cpp:
 * mode_static(), blink(), mode_blink(), mode_strobe(),
 * color_wipe(), mode_color_wipe(), mode_color_sweep() and mode_fade().
 *
 * WLED: copyright (c) 2016 Harm Aldick and individual WLED contributors.
 * Licensed under EUPL v. 1.2 or later; see third_party/wled/LICENSE.
 * Original: https://github.com/wled/WLED/blob/20e7a717664295eefc6ad77972cee1888d2da072/wled00/FX.cpp
 * This port omits WLED palette handling and uses direct foreground/background
 * for palette_id=0 only. No pixel I/O, allocations, Arduino or ESP-IDF dependencies.
 */
#include "wled_fx_compat.h"

static const ot_wled_fx_metadata_t supported[] = {
    {OT_WLED_SOLID,  "Solid",  "Solid",                         true, false, false, false, false},
    {OT_WLED_BLINK,  "Blink",  "Blink@!,Duty cycle;!,!;!;01",   true, true, true, true, false},
    {OT_WLED_WIPE,   "Wipe",   "Wipe@!,!;!,!;!",                true, true, true, true, false},
    {OT_WLED_SWEEP,  "Sweep",  "Sweep@!,!;!,!;!",              true, true, true, true, false},
    {OT_WLED_FADE,   "Fade",   "Fade@!;!,!;!;01",              true, true, true, false, false},
    {OT_WLED_STROBE, "Strobe", "Strobe@!;!,!;!;01",            true, true, true, false, false}
};

bool ot_wled_fx_metadata(uint16_t id, ot_wled_fx_metadata_t *out) {
    if (!out) return false;
    for (size_t i=0; i<sizeof(supported)/sizeof(supported[0]); ++i) {
        if (supported[i].id==id) { *out=supported[i]; return true; }
    }
    return false;
}

void ot_wled_fx_reset(ot_wled_fx_state_t *state) {
    if (state) state->step=0;
}

static ot_wled_rgb_t scaled(ot_wled_rgb_t c, uint8_t brightness) {
    return (ot_wled_rgb_t){
        (uint8_t)((((uint32_t)c.r*brightness)+127u)/255u),
        (uint8_t)((((uint32_t)c.g*brightness)+127u)/255u),
        (uint8_t)((((uint32_t)c.b*brightness)+127u)/255u)
    };
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

/* WLED triwave16 from FastLED-style triangle timing: 0..65534. */
static uint16_t wave16(uint16_t x) {
    if (x&0x8000u) x=(uint16_t)(65535u-(uint32_t)x);
    return (uint16_t)((uint32_t)x<<1);
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

bool ot_wled_fx_render(const ot_wled_fx_config_t *cfg, ot_wled_fx_state_t *state,
                       uint32_t time_ms, ot_wled_rgb_t *pixels, size_t count) {
    if (!cfg || !state || !pixels || count==0 || cfg->frame_time_ms==0 || cfg->palette_id!=0)
        return false;
    ot_wled_fx_metadata_t meta;
    if (!ot_wled_fx_metadata(cfg->effect_id, &meta)) return false;

    if (count>65535u) return false; /* 16-bit WLED segment indexing boundary */
    if (cfg->effect_id==OT_WLED_WIPE || cfg->effect_id==OT_WLED_SWEEP) {
        wipe_frame(cfg,time_ms,pixels,count,cfg->effect_id==OT_WLED_SWEEP);
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
    if (cfg->effect_id==OT_WLED_BLINK || cfg->effect_id==OT_WLED_STROBE) {
        /* Mirror WLED's 32-bit integer arithmetic in blink():
         * cycleTime=(255-speed)*20;
         * onTime=FRAMETIME + (strobe?0:(cycleTime*intensity)>>8);
         * cycleTime+=FRAMETIME*2;
         * on=(cycleIndex!=SEGENV.step || phase<=onTime).
         * Updating step ONCE per frame preserves WLED's sparse-frame catch-up.
         */
        uint32_t cycle_time=(255u-(uint32_t)cfg->speed)*20u;
        uint32_t on_time=(uint32_t)cfg->frame_time_ms;
        if (cfg->effect_id==OT_WLED_BLINK)
            on_time += (cycle_time * (uint32_t)cfg->intensity)>>8;
        cycle_time += (uint32_t)cfg->frame_time_ms*2u;
        uint32_t cycle_index=time_ms/cycle_time;
        uint32_t phase=time_ms%cycle_time;
        on=(cycle_index!=state->step || phase<=on_time);
        state->step=cycle_index;
    }

    const ot_wled_rgb_t color=scaled(on?cfg->foreground:cfg->background,cfg->brightness);
    for (size_t i=0;i<count;++i) pixels[i]=color;
    return true;
}
