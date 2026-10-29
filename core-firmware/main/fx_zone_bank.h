#pragma once
/* Standalone CORE 1D FX assignments. One slot per physical output/zone.
 * All arbitration/LED bus writes remain with the existing render task.
 * No effects catalogue duplication, Wi-Fi requirements, or auto-start on boot.
 */
#include <stdbool.h>
#include <stdint.h>
#include "ble_fx_command.h"
#include "output_manager.h"
#include "fx_overlay_engine.h"

/* Temporary per-zone effect accents; not part of saved NVS v1/v2 payload. */
typedef struct {
    uint8_t style,peak;
    ot_wled_rgb_t color;
} ot_fx_spark_style_t;
typedef struct {
    bool configured;
    bool standalone; /* explicitly played stored preset; survives BLE disconnection */
    ot_ble_fx_packet_t request;
    ot_fx_spark_style_t spark;
    ot_fx_overlay_config_t overlay; /* RAM-only; reset on new FX generation */
    uint32_t started_ms;
    uint32_t generation; /* changes only on this zone's replacement */
} ot_fx_zone_slot_t;

typedef struct {
    ot_fx_zone_slot_t zones[OT_OM_OUTPUTS][OT_OM_ZONES];
    uint32_t next_generation;
} ot_fx_zone_bank_t;

void ot_fx_bank_reset(ot_fx_zone_bank_t *bank);
bool ot_fx_bank_apply(ot_fx_zone_bank_t *bank,const ot_ble_fx_packet_t *request,
                      uint32_t now_ms,bool standalone);
/* Apply across configured Output Manager zones only; rejects a request whose
 * selected mask resolves to no usable LEDs. Retains all untouched zone states. */
bool ot_fx_bank_apply_configured(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 uint32_t now_ms,bool standalone);
/* Adjust only an already-playing zone's overlay. Do not change FX packet,
 * start time, lifetime, generation or another zone. Atomic on validation
 * failure; caller owns render state_lock and session authorization. */
bool ot_fx_bank_set_overlay(ot_fx_zone_bank_t *bank,
 const ot_om_output_t outputs[OT_OM_OUTPUTS], uint8_t chain_mask,
 uint8_t zone_mask,uint32_t now_ms,const ot_fx_overlay_config_t *overlay);
bool ot_fx_slot_active(const ot_fx_zone_slot_t *slot,uint32_t now_ms);
bool ot_fx_bank_any_active(const ot_fx_zone_bank_t *bank,uint32_t now_ms);
void ot_fx_bank_stop_temporary(ot_fx_zone_bank_t *bank);
void ot_fx_bank_stop_all(ot_fx_zone_bank_t *bank);
