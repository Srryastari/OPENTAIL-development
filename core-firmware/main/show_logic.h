#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core_logic.h"

/* Independent, VOLATILE, bench-only native effect model. No stored profiles. */
typedef enum {OT_SHOW_OFF=0, OT_SHOW_SOLID, OT_SHOW_CHASE, OT_SHOW_BREATHE, OT_SHOW_WIPE, OT_SHOW_RAINBOW} ot_show_mode_t;
typedef struct {
    ot_show_mode_t mode;
    ot_rgb_t color;
    uint8_t brightness;  /* capped by the caller to OT_MAX_BRIGHTNESS */
    uint8_t speed;       /* 0..100 */
    uint8_t chain_mask;  /* bit N selects physical output N; legacy field name */
    uint16_t section_mask; /* up to 8 user-defined zones; no vehicle semantics */
} ot_show_t;

bool ot_show_valid(const ot_show_t *show,const ot_layout_t *layout);
const char *ot_show_name(ot_show_mode_t mode);
ot_rgb_t ot_show_pixel(const ot_show_t *show,const ot_layout_t *layout,int chain,
                       unsigned pixel,uint32_t elapsed_ms);
