#pragma once
/* Pure host-testable NVS record for a single saved CORE FX snapshot.
 * Separate NVS namespace/version from Output Manager and Trigger Manager.
 * Stored presets do not autostart after a power cycle.
 */
#include <stdbool.h>
#include <stdint.h>
#include "ble_fx_command.h"
#define OT_FX_PRESET_MAGIC 0x4f544658u
#define OT_FX_PRESET_VERSION 1u
#define OT_FX_PRESET_SLOTS 8u
typedef struct {
 uint32_t magic;
 uint16_t version;
 uint16_t crc;
 ot_ble_fx_packet_t fx;
} ot_fx_preset_record_t;
bool ot_fx_preset_pack(ot_fx_preset_record_t *rec,const ot_ble_fx_packet_t *fx);
bool ot_fx_preset_valid(const ot_fx_preset_record_t *rec);
