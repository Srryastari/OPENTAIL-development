#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "core_logic.h"

/* Two present physical RMT outputs. Twelve outputs are NOT implemented. */
#define OT_OM_OUTPUTS 2
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
    uint16_t active_pixels; /* 1..522 for this bench hardware */
    uint8_t color_order;
    uint8_t zone_count; /* 1..8 user-managed zones, migrated from v1 5-zone model */
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
bool ot_om_layout(const ot_om_output_t *o,ot_layout_t *layout);
ot_rgb_t ot_om_trigger_color(uint8_t role,uint8_t mask,uint8_t brightness);
ot_rgb_t ot_om_swizzle(ot_rgb_t rgb,uint8_t requested_order);
ot_rgb_t ot_om_calibrate(ot_rgb_t rgb,const ot_om_output_t *cfg);
