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
    OT_WLED_PACIFICA = 101,    /* WLED FX_MODE_PACIFICA */
    OT_WLED_ROCKTAVES = 185,  /* WLED FX_MODE_ROCKTAVES */
    OT_WLED_NOISE_PAL = 107,   /* WLED FX_MODE_NOISEPAL */
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
    OT_WLED_JUGGLE = 64,
    OT_WLED_FILL_NOISE = 69,
    OT_WLED_NOISE_1 = 70,
    OT_WLED_NOISE_2 = 71,
    OT_WLED_NOISE_3 = 72,
    OT_WLED_NOISE_4 = 73,
    OT_WLED_PHASED = 105, /* WLED FX_MODE_PHASED */
    OT_WLED_PHASED_NOISE = 109, /* WLED FX_MODE_PHASEDNOISE */
    OT_WLED_PRIDE_2015 = 63, /* WLED FX_MODE_PRIDE_2015 */
    OT_WLED_COLORWAVES = 67, /* WLED FX_MODE_COLORWAVES */
    OT_WLED_FAIRY = 49,      /* WLED FX_MODE_FAIRY */
    OT_WLED_FAIRYTWINKLE = 51, /* WLED FX_MODE_FAIRYTWINKLE */
    OT_WLED_FIRE_2012 = 66, /* WLED FX_MODE_FIRE_2012 */
    OT_WLED_DANCING_SHADOWS = 112, /* WLED FX_MODE_DANCING_SHADOWS */
    OT_WLED_SLOW_TRANSITION = 219, /* WLED FX_MODE_SLOW_TRANSITION */
    OT_WLED_COLORTWINKLE = 74, /* WLED FX_MODE_COLORTWINKLE */
    OT_WLED_AURORA = 38, /* WLED FX_MODE_AURORA */
    OT_WLED_TWINKLEFOX = 80, /* WLED FX_MODE_TWINKLEFOX */
    OT_WLED_TWINKLECAT = 81, /* WLED FX_MODE_TWINKLECAT */
    OT_WLED_PALETTE = 65, /* WLED FX_MODE_PALETTE */
    OT_WLED_BLENDS = 115, /* WLED FX_MODE_BLENDS */
    OT_WLED_GLITTER = 87, /* WLED FX_MODE_GLITTER */
    OT_WLED_FLOW_STRIPE = 179, /* WLED FX_MODE_FLOWSTRIPE */
    OT_WLED_TETRIX = 44, /* WLED FX_MODE_TETRIX */
    OT_WLED_EXPLODING_FIREWORKS = 90, /* WLED FX_MODE_EXPLODING_FIREWORKS */
    OT_WLED_STARBURST = 89, /* WLED FX_MODE_STARBURST */
    OT_WLED_COLOR_CLOUDS = 218, /* WLED FX_MODE_COLORCLOUDS */
    OT_WLED_TV_SIMULATOR = 116, /* WLED FX_MODE_TV_SIMULATOR */
    OT_WLED_DRIP = 96, /* WLED FX_MODE_DRIP */
    OT_WLED_POPCORN = 95, /* WLED FX_MODE_POPCORN */
    OT_WLED_ROLLING_BALLS = 48, /* WLED FX_MODE_ROLLINGBALLS */
    OT_WLED_BOUNCING_BALLS = 91, /* WLED FX_MODE_BOUNCINGBALLS */
    OT_WLED_HALLOWEEN_EYES = 82 /* WLED FX_MODE_HALLOWEEN_EYES */
};

typedef struct { uint8_t r, g, b; } ot_wled_rgb_t;
/* Source-accurate 16-entry RGB palette sampler. No global state, allocations,
 * UI styling or WLED runtime. Frame-scoped dynamic/custom palettes are accepted
 * only through explicit caller-owned resolved snapshots.
 * ID 0 is the WLED source default Party palette for palette-only callers;
 * it does not change existing direct-color render paths. */
typedef struct { ot_wled_rgb_t entries[16]; } ot_wled_palette16_t;
typedef enum {
    OT_WLED_PALETTE_NOBLEND=0,
    OT_WLED_PALETTE_LINEAR=1,
    OT_WLED_PALETTE_NOWRAP=2
} ot_wled_palette_blend_t;
bool ot_wled_palette_build(uint8_t id, ot_wled_rgb_t primary,
                           ot_wled_rgb_t secondary, ot_wled_rgb_t tertiary,
                           ot_wled_palette16_t *out);
/* Source WLED colors.cpp generateRandomPalette() CHSV->RGB construction.
 * Accept four HSV triplets in upstream generation order (h,s,v), so RNG
 * timing and state stay caller-owned. Does not enable dynamic palette ID 1
 * or change current fixed-palette selection/transport semantics. */
bool ot_wled_palette_from_hsv4(const uint8_t hsv[12],
                               ot_wled_palette16_t *out);
/* Source fastled_slim.h loadDynamicGradientPalette(): 2–18 sorted
 * [index,R,G,B] stops, last index=255. No JSON, NVS, BLE or ID allocation
 * is performed here; external profile/transport owns those contracts.
 * Invalid data fails atomically without changing *out. */
bool ot_wled_palette_from_gradient_stops(const uint8_t *stops,
                                          size_t stop_count,
                                          ot_wled_palette16_t *out);
/* Explicit source palette-ID lookup; arrays are caller-owned and ordered as
 * WLED custom palette 200,199,... and usermod 255,254,... respectively.
 * Built-ins 0,2-72 reuse verified fixed palettes; 1 reads the live shared
 * random palette. Unprovided dynamic/custom IDs fail closed rather than
 * fabricating fallback colors. No persistent or wire-format changes. */
bool ot_wled_palette_resolve(uint8_t id,
                              ot_wled_rgb_t primary,ot_wled_rgb_t secondary,
                              ot_wled_rgb_t tertiary,
                              const ot_wled_palette16_t *live_random,
                              const ot_wled_palette16_t *custom,
                              size_t custom_count,
                              const ot_wled_palette16_t *usermod,
                              size_t usermod_count,
                              ot_wled_palette16_t *out);
ot_wled_rgb_t ot_wled_palette_sample(const ot_wled_palette16_t *palette,
                                    uint8_t index, uint8_t brightness,
                                    ot_wled_palette_blend_t blend);

/* WLED fastled_slim.cpp nblendPaletteTowardPalette(), RGB-only.
 * These 16 RGB entries occupy 48 contiguous bytes in WLED's source order.
 * Like the source, a decrement can move a channel two units per pass.
 * The transition is shared by segments; callers own its lifecycle. */
void ot_wled_palette_blend_toward(ot_wled_palette16_t *current,
                                  const ot_wled_palette16_t *target,
                                  uint8_t max_changes);

/* Source FX_fcn.cpp handleRandomPalette() time/transition state. The WLED
 * palette is GLOBAL across segments. Call this once per global FX frame, not
 * independently per output or zone. The caller supplies a pre-generated
 * source palette ONLY when a change is due (from the existing verified HSV4
 * constructor or harmonic generator); this function never invents random
 * values. For a requested change with no generated palette, it rejects
 * atomically and leaves state untouched. */
typedef struct {
    ot_wled_palette16_t current;
    ot_wled_palette16_t target;
    uint16_t last_change_s;
    uint16_t next_blend_ms;
} ot_wled_palette_random_t;

bool ot_wled_palette_random_tick(ot_wled_palette_random_t *state,
                                 uint32_t now_ms, uint16_t change_interval_s,
                                 uint16_t frame_time_ms,
                                 uint16_t transition_time_ms,
                                 const ot_wled_palette16_t *generated_if_due);


/* Optional measured audio peak provided by the external audio subsystem.
 * No synthetic FFT, microphone acquisition or BLE extension is embedded in
 * the rendering engine. This data is read only for FX 185 Rocktaves. */
typedef struct {
    float major_peak_hz;
    float magnitude;
} ot_wled_audio_peak_t;

typedef struct {
    uint16_t effect_id;     /* Preserve WLED's numeric effect ID */
    uint8_t speed;          /* WLED 0..255 */
    uint8_t intensity;      /* WLED 0..255 (Blink duty-cycle) */
    uint8_t brightness;     /* 0..255 post-effect linear cap; WLED bus brightness is separate */
    uint8_t palette_id;     /* 0=default; 2-72 built-in; 1/73-255 require resolved_palette */
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
    const ot_wled_audio_peak_t *audio_peak; /* nullable, source FX 185 only */
    uint8_t active_segments;   /* FX 90 WLED spark budget, 0=isolated 1D zone */
    uint8_t max_segments;      /* FX 90 WLED spark budget, 0=64 (ESP32-S3) */
    /* Optional frame-scoped, already resolved global/user palette (ID 1 or
     * 73-255). Owned by the caller; never retained or modified by CORE.
     * No wire, persistence, or generation/scheduling behavior is implied. */
    const ot_wled_palette16_t *resolved_palette;
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
 * Fixed RGB palettes 2-12 are supported only where explicitly marked.
 * Dynamic/custom palette generation and delivery, gamma and bus brightness
 * remain separate; an injected palette never bypasses effect support gating.
 * Returns false for unknown effects, invalid config or zero-sized zones.
 */
bool ot_wled_fx_render(const ot_wled_fx_config_t *config, ot_wled_fx_state_t *state,
                       uint32_t time_ms, ot_wled_rgb_t *pixels, size_t count);
