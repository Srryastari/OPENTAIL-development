#pragma once
/* WLED effect identifiers/names used solely for catalog interoperability.
 * WLED FX.h/FX.cpp: https://github.com/wled/WLED (EUPL 1.2-or-later).
 * Renderer in this module is an original 1D compatibility implementation;
 * it is NOT claimed to be pixel-identical to WLED's complete engine.
 */
#include <stddef.h>
#include <stdint.h>
#include "core_logic.h"
typedef struct { uint16_t id; const char *key; const char *name; uint8_t family; } ot_wled_1d_meta_t;
size_t ot_wled_1d_count(void);
const ot_wled_1d_meta_t *ot_wled_1d_at(size_t i);
const ot_wled_1d_meta_t *ot_wled_1d_by_key(const char *key);
const ot_wled_1d_meta_t *ot_wled_1d_by_id(unsigned id);
ot_rgb_t ot_wled_1d_pixel(unsigned id, unsigned pixel, unsigned count, uint32_t elapsed_ms,
                          uint8_t speed, uint8_t intensity, ot_rgb_t color, ot_rgb_t bg, ot_rgb_t accent);
