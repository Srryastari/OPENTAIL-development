#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "output_manager.h"

/* Explicit, backward-compatible hierarchical schema. All pixel coordinates are
 * zero-based and global within the physical output lane; no geometry inference. */
#define OT_PZ_OUTPUTS OT_OM_OUTPUTS
#define OT_PZ_SCHEMA_VERSION 3u
#define OT_PZ_WIRE_VERSION 3u
#define OT_PZ_MAX_PANELS_PER_OUTPUT 4u
#define OT_PZ_MAX_ZONES_PER_OUTPUT OT_OM_ZONES
#define OT_PZ_MAX_RANGES_PER_ZONE 8u
#define OT_PZ_MAX_PIXELS OT_OM_MAX_PIXELS_PER_OUTPUT
#define OT_PZ_NAME_LEN OT_OM_NAME_LEN
#define OT_PZ_MAX_WIRE_BYTES 667u
#define OT_PZ_ROLE_UNASSIGNED 0xffu
#define OT_PZ_ZONE_FLAG_LEGACY_EMPTY 0x01u
#define OT_PZ_ZONE_FLAGS_MASK OT_PZ_ZONE_FLAG_LEGACY_EMPTY

typedef struct {
    uint16_t start; /* inclusive, zero-based lane coordinate */
    uint16_t count; /* positive; inclusive end = start + count - 1 */
} ot_pz_range_t;

typedef struct {
    uint16_t id;
    uint8_t lane; /* 0=physical Output 1, 1=physical Output 2 */
    char name[OT_PZ_NAME_LEN];
    uint8_t zone_count;
} ot_pz_panel_t;

typedef struct {
    uint16_t id;
    uint16_t panel_id;
    char name[OT_PZ_NAME_LEN];
    uint8_t role; /* OT_OM_* role or OT_PZ_ROLE_UNASSIGNED; OFF is distinct */
    uint8_t flags; /* only the read-only legacy-empty migration marker is valid */
    uint8_t range_count;
    ot_pz_range_t ranges[OT_PZ_MAX_RANGES_PER_ZONE];
} ot_pz_zone_t;

typedef struct {
    char name[OT_PZ_NAME_LEN];
    uint16_t active_pixels; /* 0 means unknown/unconfigured, NOT buffer capacity */
    uint8_t color_order;
    uint8_t rgb_gain[3];
    uint8_t white_gain[3];
    bool gamma_enabled;
    uint8_t panel_count;
    uint8_t zone_count; /* total zones across all panels; preserves existing limit of 8 */
    ot_pz_panel_t panels[OT_PZ_MAX_PANELS_PER_OUTPUT];
    ot_pz_zone_t zones[OT_PZ_MAX_ZONES_PER_OUTPUT];
} ot_pz_output_t;

typedef struct {
    ot_pz_output_t outputs[OT_PZ_OUTPUTS];
} ot_pz_controller_t;

/* Single-owner volatile arrangement preview; slider updates never write NVS. */
#define OT_PZ_PREVIEW_LEASE_MS 2000u
typedef struct {
    ot_pz_controller_t candidate;
    uint32_t expires_ms;
    uint16_t base_revision;
    uint8_t lane;
    bool active;
} ot_pz_preview_t;

typedef struct {
    ot_pz_controller_t candidate;
    bool pending;
} ot_pz_pending_t;

void ot_pz_defaults(ot_pz_controller_t *config);
bool ot_pz_output_valid(const ot_pz_output_t *output,unsigned lane);
bool ot_pz_valid(const ot_pz_controller_t *config);
bool ot_pz_activation_inputs_quiet(uint8_t instantaneous,uint8_t debounced,
                                   uint8_t quiet_samples,uint8_t required_samples,
                                   uint32_t current_epoch,uint32_t expected_epoch);
bool ot_pz_preview_begin(ot_pz_preview_t *state,
                         const ot_pz_controller_t *committed,unsigned lane,
                         uint16_t active_revision,uint16_t base_revision,
                         bool pending,uint32_t now_ms);
bool ot_pz_preview_update(ot_pz_preview_t *state,unsigned lane,
                          const ot_pz_output_t *next,uint16_t base_revision,
                          uint16_t active_revision,bool pending,uint32_t now_ms);
bool ot_pz_preview_expire(ot_pz_preview_t *state,uint32_t now_ms);
void ot_pz_preview_cancel(ot_pz_preview_t *state);
uint16_t ot_pz_zone_pixel_count(const ot_pz_zone_t *zone);
bool ot_pz_migrate_legacy(const ot_om_output_t legacy[OT_OM_OUTPUTS],
                          bool legacy_persisted,ot_pz_controller_t *out);
/* Replace one lane from the unchanged legacy BLE v1 codec without erasing
 * the other lane's panel IDs or hierarchical layout. */
bool ot_pz_from_legacy_output(const ot_om_output_t *legacy,unsigned lane,
                              const ot_pz_controller_t *existing,
                              ot_pz_output_t *out);
bool ot_pz_to_legacy(const ot_pz_output_t *src,unsigned lane,
                     ot_om_output_t *dst);
bool ot_pz_to_fx_view(const ot_pz_output_t *src,unsigned lane,
                      ot_om_output_t *dst);
void ot_pz_build_pixel_map(const ot_pz_output_t *output,
                           uint8_t zone_index[OT_PZ_MAX_PIXELS],
                           uint16_t logical_index[OT_PZ_MAX_PIXELS]);

/* Raw commissioning inputs use the fixed IN1..IN8 wiring contract, independent
 * of any saved trigger-role profile. Brake > Reverse > directional > Parking. */
ot_rgb_t ot_pz_fallback_color(unsigned lane,uint8_t raw_input_mask,
                              uint8_t brightness);
ot_rgb_t ot_pz_priority_color(bool system_inhibit,bool physical_owned,
                              ot_rgb_t physical_color,bool diagnostic_owned,
                              ot_rgb_t diagnostic_color,ot_rgb_t effect_color);

/* Fixed-size, deterministic v3 output payload. BLE transport chunks this
 * payload; it never changes the existing 261-byte v1 codec. */
bool ot_pz_wire_encode(const ot_pz_output_t *output,unsigned lane,
                       uint8_t wire[OT_PZ_MAX_WIRE_BYTES]);
bool ot_pz_wire_decode(const uint8_t wire[OT_PZ_MAX_WIRE_BYTES],
                       unsigned expected_lane,ot_pz_output_t *output);
