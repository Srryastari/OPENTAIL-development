#pragma once
#include <stdbool.h>
#include <stdint.h>

#define OT_IN_COUNT 8
#define OT_CHAIN_COUNT 2

typedef enum { OT_TOP=0, OT_TURN=1, OT_REVERSE=2, OT_BOTTOM=3, OT_TRUNK=4, OT_SECTION_COUNT=8 } ot_section_t;
typedef struct { uint8_t r,g,b; } ot_rgb_t;
typedef struct {
    uint16_t sections[OT_SECTION_COUNT];
    uint16_t chain_pixels;
    uint8_t brightness_cap;
} ot_layout_t;
typedef struct {
    bool settled[OT_IN_COUNT];
    bool pending[OT_IN_COUNT];
    uint8_t ticks[OT_IN_COUNT];
} ot_inputs_t;
typedef enum { OT_DEMO_OFF=0, OT_DEMO_SOLID=1, OT_DEMO_CHASE=2 } ot_demo_mode_t;
typedef struct {ot_demo_mode_t mode; ot_rgb_t color; uint8_t brightness; } ot_demo_t;

#define OT_SIG_PARK    (1u<<0)
#define OT_SIG_BRAKE   (1u<<1)
#define OT_SIG_LEFT    (1u<<2)
#define OT_SIG_RIGHT   (1u<<3)
#define OT_SIG_REVERSE (1u<<4)
#define OT_USED_INPUTS (OT_SIG_PARK|OT_SIG_BRAKE|OT_SIG_LEFT|OT_SIG_RIGHT|OT_SIG_REVERSE)

bool ot_layout_valid(const ot_layout_t *cfg);
int ot_section_for_pixel(const ot_layout_t *cfg, unsigned pixel);
uint8_t ot_debounce(ot_inputs_t *in, const bool levels[OT_IN_COUNT], unsigned stable_ticks);
ot_rgb_t ot_pixel_color(const ot_layout_t *cfg, int chain, unsigned pixel, uint8_t signal_mask,
                        const ot_demo_t *demo, uint32_t tick);
