/*
 * Isolated adaptation of WLED wled00/FX.cpp:
 * mode_static(), blink(), mode_blink(), mode_strobe(),
 * color_wipe(), mode_color_wipe(), mode_color_sweep(), mode_fade(),
 * scan(), mode_scan(), mode_dual_scan(), running(), mode_theater_chase(),
 * mode_static_pattern(), mode_heartbeat(), spots_base(),
 * mode_spots(), mode_spots_fade(), mode_percent(),
 * mode_chase_flash(), and mode_running_color().
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
     true, true, true, true, false}
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
    if (state) { state->step=0; state->aux0=0; state->aux1=0; }
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
                          uint32_t now, ot_wled_rgb_t *pixels, size_t count) {
    const uint32_t width=3u+((uint32_t)cfg->intensity>>4);
    const uint32_t iteration=now/(50u+255u-(uint32_t)cfg->speed);
    const ot_wled_rgb_t fg=scaled(cfg->foreground,cfg->brightness);
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
    if (cfg->effect_id==OT_WLED_THEATER) {
        theater_frame(cfg,state,time_ms,pixels,count);
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
