#pragma once
/* Standalone CORE 1D FX assignments. One slot per physical output/zone.
 * All arbitration/LED bus writes remain with the existing render task.
 * No effects catalogue duplication, Wi-Fi requirements, or auto-start on boot.
 */
#include <stdbool.h>
#include <stdint.h>
#include "ble_fx_command.h"
#include "output_manager.h"

typedef struct {
    bool configured;
    bool standalone; /* explicitly played stored preset; survives BLE disconnection */
    ot_ble_fx_packet_t request;
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
bool ot_fx_slot_active(const ot_fx_zone_slot_t *slot,uint32_t now_ms);
bool ot_fx_bank_any_active(const ot_fx_zone_bank_t *bank,uint32_t now_ms);
void ot_fx_bank_stop_temporary(ot_fx_zone_bank_t *bank);
void ot_fx_bank_stop_all(ot_fx_zone_bank_t *bank);
