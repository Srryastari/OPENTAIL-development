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
    OT_WLED_STROBE = 23     /* WLED FX_MODE_STROBE */
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
} ot_wled_fx_config_t;
typedef struct {
    uint32_t step;          /* Upstream per-segment SEGENV.step */
    uint16_t aux0;          /* Upstream per-segment SEGENV.aux0 for Theater phase */
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
 * Sweep, Scan, Dual Scan, Fade, Theater and Strobe, with default
 * direct colors and Overlay disabled. Full upstream parity remains unverified.
 * WLED palette modes, gamma and global brightness handling are NOT ported.
 * Returns false for unknown effects, invalid config or zero-sized zones.
 */
bool ot_wled_fx_render(const ot_wled_fx_config_t *config, ot_wled_fx_state_t *state,
                       uint32_t time_ms, ot_wled_rgb_t *pixels, size_t count);
