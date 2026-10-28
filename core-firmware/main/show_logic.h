#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core_logic.h"

/* Independent, VOLATILE, bench-only native effect model. No stored profiles. */
typedef enum {OT_SHOW_OFF=0, OT_SHOW_SOLID, OT_SHOW_CHASE, OT_SHOW_BREATHE, OT_SHOW_WIPE, OT_SHOW_RAINBOW,
    OT_SHOW_STROBE, OT_SHOW_SCAN, OT_SHOW_PULSE, OT_SHOW_FADE, OT_SHOW_BLINK,
    OT_SHOW_PLASMA_SWEEP, OT_SHOW_SPARKLE, OT_SHOW_METEOR, OT_SHOW_COLOR_WAVES,
    OT_SHOW_TWINKLE, OT_SHOW_WLED_COMPAT, OT_SHOW_LAST=OT_SHOW_WLED_COMPAT} ot_show_mode_t;
typedef struct {
    ot_show_mode_t mode;
    ot_rgb_t color;
    ot_rgb_t background;
    ot_rgb_t accent;
    uint16_t compat_id; /* stable WLED effect ID when mode==OT_SHOW_WLED_COMPAT */
    uint8_t intensity; /* 0..255, default 128 */
    uint8_t brightness;  /* capped by the caller to OT_MAX_BRIGHTNESS */
    uint8_t speed;       /* 0..100 */
    uint8_t chain_mask;  /* bit 0=left, bit 1=right */
    uint16_t section_mask; /* up to 8 user-defined zones; no vehicle semantics */
} ot_show_t;

bool ot_show_valid(const ot_show_t *show,const ot_layout_t *layout);
const char *ot_show_name(ot_show_mode_t mode);
ot_rgb_t ot_show_pixel(const ot_show_t *show,const ot_layout_t *layout,int chain,
                       unsigned pixel,uint32_t elapsed_ms);
