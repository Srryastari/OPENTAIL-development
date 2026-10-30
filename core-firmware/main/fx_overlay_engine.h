#pragma once
/* OpenTail private effect-compositing engine, not a public effect ID.
 * Stateless and deterministic across frame rates; no heap, LED/GPIO, BLE,
 * WLED branding or trigger ownership. The caller overlays this on a rendered
 * zone BEFORE physical trigger arbitration and output calibration.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wled_fx_compat.h"

enum {
    OT_FX_OVERLAY_OFF=0,
    OT_FX_OVERLAY_SPARKS=1,
    OT_FX_OVERLAY_FLARES=2,
    OT_FX_OVERLAY_SPARKS_FLARES=3
};
typedef struct {
    uint8_t style;         /* off / sparks / flares / both */
    uint8_t peak;          /* 0 => default 32, otherwise 1..32 bench cap */
    uint8_t density;       /* 0..255; spawn period 600..70 ms */
    uint8_t radius;        /* 0 => default radius 4, otherwise 1..12 px */
    uint16_t fade_ms;      /* 0 => default 120, otherwise 40..1000 ms */
    ot_wled_rgb_t color;   /* 0,0,0 => default white */
} ot_fx_overlay_config_t;

bool ot_fx_overlay_valid(const ot_fx_overlay_config_t *cfg);
/* Returns false for invalid input. Pure max-composite (no blackening), bounded
 * to max 32/255 channel intensity for the overlay. A zero-brightness effect
 * never emits an overlay. Count and seed are arbitrary, no car geometry.
 * Does not alter cfg or any pixel outside the caller-provided zone.
 */
bool ot_fx_overlay_apply(const ot_fx_overlay_config_t *cfg,
                         uint32_t seed,uint32_t elapsed_ms,
                         ot_wled_rgb_t *pixels,size_t count,
                         uint8_t effect_brightness);
