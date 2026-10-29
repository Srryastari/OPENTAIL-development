#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "core_logic.h"

/* Two present physical RMT outputs. Twelve outputs are NOT implemented. */
#define OT_OM_OUTPUTS 2
/* Current prototype: two RMT outputs with up to 522 allocated pixels each.
 * These are tested-development hardware limits, not vehicle-specific zone
 * boundaries; the output manager never assumes a particular lighting shape.
 */
/* Inert one-pixel placeholder until an installer explicitly configures output.
 * This is not a vehicle layout nor the capacity of the hardware. */
#define OT_OM_UNCONFIGURED_PIXELS 1u
#define OT_OM_ZONES 8
#define OT_OM_NAME_LEN 25
#define OT_OM_VERSION 2

typedef enum {
    OT_OM_OFF=0,OT_OM_PARK_BRAKE,OT_OM_PARK,OT_OM_BRAKE,
    OT_OM_LEFT,OT_OM_RIGHT,OT_OM_REVERSE,OT_OM_AUX1,OT_OM_AUX2,OT_OM_AUX3,
    OT_OM_DECORATIVE
} ot_om_role_t;
typedef enum {OT_OM_RGB=0,OT_OM_RBG,OT_OM_GRB,OT_OM_GBR,OT_OM_BRG,OT_OM_BGR} ot_om_order_t;
typedef struct {
    char name[OT_OM_NAME_LEN];
    uint16_t pixels;   /* contiguous; sums to active_pixels */
    uint8_t role;
} ot_om_zone_t;
typedef struct {
    char name[OT_OM_NAME_LEN];
    uint16_t active_pixels; /* 1..522 allocated per physical output in this prototype */
    uint8_t color_order;
    uint8_t zone_count; /* 1..8 configured zones; newly initialized device has one neutral zone */
    uint8_t rgb_gain[3]; /* per-output decorative RGB calibration, 0..255 */
    uint8_t white_gain[3]; /* additional tint correction for neutral whites */
    bool gamma_enabled; /* optional soft gamma; OFF by default to preserve colors */
    ot_om_zone_t zones[OT_OM_ZONES];
} ot_om_output_t;

const char *ot_om_role_name(uint8_t role);
const char *ot_om_order_name(uint8_t order);
int ot_om_role_from_name(const char *role);
int ot_om_order_from_name(const char *order);
void ot_om_defaults(ot_om_output_t outputs[OT_OM_OUTPUTS]);
bool ot_om_valid(const ot_om_output_t *o);
int ot_om_section(const ot_om_output_t *o,unsigned pixel);
/* O(1) amortized sequential-zone lookup for the pixel output loop.
 * Only use after ot_om_layout() validates an output and only with
 * ascending pixel indices. Zero-length zones are safely skipped.
 * Standalone/random-access callers must keep using ot_om_section(). */
typedef struct {
    uint8_t next_zone;
    uint16_t end;
} ot_om_zone_cursor_t;
static inline int ot_om_section_next(const ot_om_output_t *o,
                                      ot_om_zone_cursor_t *cursor,unsigned pixel){
    if(!o||!cursor||pixel>=o->active_pixels)return -1;
    while(cursor->next_zone<o->zone_count && pixel>=cursor->end){
        cursor->end=(uint16_t)(cursor->end+o->zones[cursor->next_zone].pixels);
        cursor->next_zone++;
    }
    return cursor->next_zone?(int)cursor->next_zone-1:-1;
}
bool ot_om_layout(const ot_om_output_t *o,ot_layout_t *layout);
ot_rgb_t ot_om_trigger_color(uint8_t role,uint8_t mask,uint8_t brightness);
/* Inputs overlay only assigned zone roles. Parking is the baseline and yields
 * when a user-selected effect (or demo) occupies this zone. */
bool ot_om_trigger_overlay(uint8_t role,uint8_t mask,bool show_here,
                           uint8_t brightness,ot_rgb_t *color);
ot_rgb_t ot_om_swizzle(ot_rgb_t rgb,uint8_t requested_order);
ot_rgb_t ot_om_calibrate(ot_rgb_t rgb,const ot_om_output_t *cfg);
