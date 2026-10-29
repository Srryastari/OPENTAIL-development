#pragma once
/*
 * OpenTail's isolated WLED 1D compatibility layer.
 * The implementation of Blink / Strobe timing is adapted from WLED FX.cpp,
 * copyright (c) 2016 Harm Aldick and WLED contributors, EUPL-1.2-or-later.
 * Source pin: wled/WLED commit 20e7a717664295eefc6ad77972cee1888d2da072.
 *
 * BENCH ONLY: CORE can now dispatch supported IDs over BLE to the local LED
 * render task. The pure C module itself remains independent of GPIO/network.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    OT_WLED_SOLID = 0,      /* WLED FX_MODE_STATIC */
    OT_WLED_BLINK = 1,      /* WLED FX_MODE_BLINK */
    OT_WLED_WIPE = 3,       /* WLED FX_MODE_COLOR_WIPE */
    OT_WLED_SWEEP = 6,      /* WLED FX_MODE_COLOR_SWEEP */
    OT_WLED_SCAN = 10,      /* WLED FX_MODE_SCAN */
    OT_WLED_DUAL_SCAN = 11, /* WLED FX_MODE_DUAL_SCAN */
    OT_WLED_FADE = 12,      /* WLED FX_MODE_FADE */
    OT_WLED_THEATER = 13,   /* WLED FX_MODE_THEATER_CHASE */
    OT_WLED_STROBE = 23,    /* WLED FX_MODE_STROBE */
    OT_WLED_CHASE_FLASH = 31,  /* WLED FX_MODE_CHASE_FLASH */
    OT_WLED_RUNNING_COLOR = 37, /* WLED FX_MODE_RUNNING_COLOR */
    OT_WLED_SOLID_PATTERN = 83, /* WLED FX_MODE_STATIC_PATTERN */
    OT_WLED_SPOTS = 85,        /* WLED FX_MODE_SPOTS */
    OT_WLED_SPOTS_FADE = 86,   /* WLED FX_MODE_SPOTS_FADE */
    OT_WLED_PERCENT = 98,      /* WLED FX_MODE_PERCENT */
    OT_WLED_HEARTBEAT = 100,    /* WLED FX_MODE_HEARTBEAT */
    OT_WLED_COLORLOOP = 8,     /* WLED FX_MODE_RAINBOW */
    OT_WLED_RAINBOW = 9,       /* WLED FX_MODE_RAINBOW_CYCLE */
    OT_WLED_THEATER_RAINBOW = 14,
    OT_WLED_STROBE_RAINBOW = 24,
    OT_WLED_BLINK_RAINBOW = 26,
    OT_WLED_MULTI_STROBE = 25,
    OT_WLED_CHASE_COLOR = 28,
    OT_WLED_CHASE_RAINBOW = 30,
    OT_WLED_CHASE_RAINBOW_WHITE = 33,
    OT_WLED_TWO_DOTS = 50,
    OT_WLED_TRICOLOR_CHASE = 54,
    OT_WLED_TRICOLOR_WIPE = 55,
    OT_WLED_TRICOLOR_FADE = 56,
    OT_WLED_BREATHE = 2,
    OT_WLED_RUNNING_LIGHTS = 15,
    OT_WLED_SAW = 16,
    OT_WLED_RUNNING_DUAL = 52,
    OT_WLED_SOLID_PATTERN_TRI = 84,
    OT_WLED_COLORFUL = 34,
    OT_WLED_GRADIENT = 46,
    OT_WLED_LOADING = 47,
    OT_WLED_SINEWAVE = 108, /* WLED FX_MODE_SINEWAVE */
    OT_WLED_ANDROID = 27,  /* WLED FX_MODE_ANDROID */
    OT_WLED_WIPE_RANDOM = 4,
    OT_WLED_RANDOM_COLORS = 5,
    OT_WLED_SPARKLE = 20,
    OT_WLED_SPARKLE_DARK = 21,
    OT_WLED_SPARKLE_PLUS = 22,
    OT_WLED_TRAFFIC_LIGHT = 35,
    OT_WLED_SWEEP_RANDOM = 36,
    OT_WLED_SOLID_GLITTER = 103,
    OT_WLED_CHASE_RANDOM = 29,
    OT_WLED_STREAM = 39,
    OT_WLED_STREAM2 = 61,
    OT_WLED_LIGHTNING = 57,
    OT_WLED_ICU = 58,
    OT_WLED_FIRE_FLICKER = 45,
    OT_WLED_WAVESINS = 184,
    OT_WLED_BPM = 68,
    OT_WLED_LAKE = 75,
    OT_WLED_PLASMA = 97,
    OT_WLED_OSCILLATE = 62,
    OT_WLED_SCANNER = 40,
    OT_WLED_LIGHTHOUSE = 41,
    OT_WLED_SCANNER_DUAL = 60,
    OT_WLED_SINELON = 92,
    OT_WLED_SINELON_DUAL = 93,
    OT_WLED_SINELON_RAINBOW = 94,
    OT_WLED_TWINKLE = 17,
    OT_WLED_CHASE_FLASH_RANDOM = 32,
    OT_WLED_MULTI_COMET = 59,
    OT_WLED_CHUNCHUN = 111,
    OT_WLED_CANDLE = 88,
    OT_WLED_DYNAMIC = 7,
    OT_WLED_DYNAMIC_SMOOTH = 117,
    OT_WLED_DISSOLVE = 18,
    OT_WLED_DISSOLVE_RANDOM = 19,
    OT_WLED_CANDLE_MULTI = 102,
    OT_WLED_METEOR = 76,
    OT_WLED_TWINKLEUP = 106,
    OT_WLED_RAILWAY = 78,
    OT_WLED_WASHING_MACHINE = 113,
    OT_WLED_FLOW = 110,
    OT_WLED_SUNRISE = 104,
    OT_WLED_RIPPLE = 79,
    OT_WLED_RIPPLE_RAINBOW = 99,
    OT_WLED_FIREWORKS = 42,
    OT_WLED_RAIN = 43,
    OT_WLED_JUGGLE = 64
};

typedef struct { uint8_t r, g, b; } ot_wled_rgb_t;
typedef struct {
    uint16_t effect_id;     /* Preserve WLED's numeric effect ID */
    uint8_t speed;          /* WLED 0..255 */
    uint8_t intensity;      /* WLED 0..255 (Blink duty-cycle) */
    uint8_t brightness;     /* 0..255 post-effect linear cap; WLED bus brightness is separate */
    uint8_t palette_id;     /* Only 0 (direct solid colors) is implemented */
    uint16_t frame_time_ms; /* WLED runtime FRAMETIME, supplied by caller */
    ot_wled_rgb_t foreground;
    ot_wled_rgb_t background;
    /* Optional OpenTail one-shot spark customization. Zeroed configuration
     * retains the legacy defaults; the BLE wire format is unchanged.
     * spark_style: 0=original, 1=bright shades, 2=rainbow+white, 3=custom.
     * spark_peak: 0=legacy automatic cap; otherwise 1..32 bench-only.
     * These fields have no effect on strobes, trigger overlays or WLED modes. */
    uint8_t spark_style;
    uint8_t spark_peak;
    ot_wled_rgb_t spark_color;
} ot_wled_fx_config_t;
typedef struct {
    uint16_t pos;
    uint8_t size;
    int8_t dir;
    uint8_t speed;
} ot_wled_oscillator_t;
typedef struct {
    uint32_t step;          /* Upstream per-segment SEGENV.step */
    uint16_t aux0;          /* Upstream per-segment SEGENV.aux0 */
    uint16_t aux1;          /* Upstream per-segment SEGENV.aux1 (Heartbeat/Percent) */
    uint32_t call;          /* Upstream SEGENV.call, once per rendered frame */
    uint32_t data_counter;  /* Per-zone mode_android() allocated uint32_t counter */
    uint32_t rng_state;     /* Software RNG state for source-driven random modes */
    uint32_t held_rng_seed; /* Reproduce Fire Flicker frame between WLED updates */
    ot_wled_oscillator_t oscillators[3]; /* 3 source-owned bars, per zone */
    uint16_t comet_positions[8];  /* WLED MAX_COMETS 1D source-owned */
    uint8_t *scratch;        /* optional source data allocated per zone, not per frame */
    size_t scratch_bytes;
    uint32_t scratch_magic; /* guards reset/release for newly initialized structs */
} ot_wled_fx_state_t;

typedef struct {
    uint16_t id;
    const char *name;
    const char *wled_metadata; /* Upstream effect metadata; not a promise of palette support */
    bool supports_foreground, supports_background, supports_speed, supports_intensity;
    bool supports_palette;    /* false until palette behavior is ported and tested */
} ot_wled_fx_metadata_t;

bool ot_wled_fx_metadata(uint16_t id, ot_wled_fx_metadata_t *out);
/* Enumerates exactly the locally implemented FX IDs for BLE capabilities. */
size_t ot_wled_fx_supported_ids(uint16_t *dest,size_t max_ids);
void ot_wled_fx_reset(ot_wled_fx_state_t *state);

/* Render ONE frame for a contiguous 1D zone. Call exactly once per frame,
 * never once per pixel, so that SEGENV.step advances at the right cadence.
 * Supports source-pinned 1D timing/position adaptations for Solid, Blink, Wipe,
 * Sweep, Scan, Dual Scan, Fade, Theater, Strobe, Colorloop, Rainbow,
 * Theater Rainbow, Blink Rainbow, Strobe Rainbow, Multi Strobe,
 * Chase, Chase Rainbow, Rainbow Runner, Two Dots, Chase 3, Tri Wipe,
 * Tri Fade, Breathe, Running, Saw, Running Dual, Solid Pattern Tri,
 * Colorful, Gradient, Loading, Sine, Solid Pattern and
 * Heartbeat, Spots, Spots Fade, Percent, Chase Flash and Running Color,
 * with default
 * direct colors and Overlay disabled. Full upstream parity remains unverified.
 * WLED palette modes, gamma and global brightness handling are NOT ported.
 * Returns false for unknown effects, invalid config or zero-sized zones.
 */
bool ot_wled_fx_render(const ot_wled_fx_config_t *config, ot_wled_fx_state_t *state,
                       uint32_t time_ms, ot_wled_rgb_t *pixels, size_t count);
