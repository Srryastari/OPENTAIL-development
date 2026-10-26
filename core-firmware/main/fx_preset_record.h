#pragma once
/* Pure host-testable NVS records for saved CORE FX snapshots and zone scenes.
 * Separate NVS namespace/version from Output Manager and Trigger Manager.
 * Stored presets do not autostart after a power cycle.
 */
#include <stdbool.h>
#include <stdint.h>
#include "ble_fx_command.h"
#include "fx_zone_bank.h"
#define OT_FX_PRESET_MAGIC 0x4f544658u
#define OT_FX_PRESET_VERSION 1u
#define OT_FX_PRESET_SLOTS 8u
#define OT_FX_SCENE_VERSION 2u
typedef struct {
 uint32_t magic;
 uint16_t version;
 uint16_t crc;
 ot_ble_fx_packet_t fx;
} ot_fx_preset_record_t;
bool ot_fx_preset_pack(ot_fx_preset_record_t *rec,const ot_ble_fx_packet_t *fx);
bool ot_fx_preset_valid(const ot_fx_preset_record_t *rec);

/* V2 scene captures the entire active two-output x eight-zone effect set.
 * Preserve v1 readers so existing saved flash slots remain loadable.
 * No automatic playback on boot; no changes to Trigger/Output NVS.
 */
typedef struct {
 uint32_t magic;
 uint16_t version;
 uint16_t crc;
 uint16_t active_mask; /* bit output*8+zone: exact stored assignment */
 ot_ble_fx_packet_t effect[OT_OM_OUTPUTS][OT_OM_ZONES];
} ot_fx_scene_record_t;
bool ot_fx_scene_pack(ot_fx_scene_record_t *record,
                      const ot_fx_zone_bank_t *bank,uint32_t now_ms);
bool ot_fx_scene_valid(const ot_fx_scene_record_t *record);
