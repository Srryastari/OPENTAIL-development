#pragma once
/* OpenTail original startup/shutdown animations. Pure frame renderer:
 * no GPIO, BLE session, WLED firmware or physical output ownership.
 * One-shot animation phase is derived from elapsed time per configured zone.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wled_fx_compat.h"
enum {
    OT_FX_MATERIALIZE_SPARKS = 32769u,
    OT_FX_RANDOM_MATERIALIZE = 32770u,
    OT_FX_SMOOTH_FADE = 32771u,
    OT_FX_PLASMA_SWEEP = 32772u,
    OT_FX_SMOOTH_FADE_OUT = 32773u
};
bool ot_auto_fx_id(uint16_t id);
size_t ot_auto_fx_ids(uint16_t *dst, size_t capacity);
bool ot_auto_fx_metadata(uint16_t id, ot_wled_fx_metadata_t *meta);
uint32_t ot_auto_fx_duration_ms(uint8_t speed);
/* Defaults: 120-ms spark fade, color/background authored in CORE.
 * All writes are bounded to count pixels and cfg->brightness.
 * Smooth Fade Out holds exact configured background after completion.
 * Effect state is owned by OpenTail's per-zone state bank.
 */
bool ot_auto_fx_render(const ot_wled_fx_config_t *cfg, uint32_t seed,
                       uint32_t elapsed_ms, ot_wled_rgb_t *pixels, size_t count);
