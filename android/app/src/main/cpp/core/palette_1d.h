#pragma once
/* Subset of canonical WLED palette IDs, usable by the ORIGINAL Embers FX only.
 * IDs preserved; this is not full WLED palette implementation/parity.
 */
#include <stdbool.h>
#include <stdint.h>
#include "wled_fx_compat.h"
bool ot_palette_1d_supported(uint16_t effect_id,uint8_t palette_id);
ot_wled_rgb_t ot_palette_1d_sample(uint8_t palette_id,
  ot_wled_rgb_t primary,ot_wled_rgb_t secondary,uint8_t position);
