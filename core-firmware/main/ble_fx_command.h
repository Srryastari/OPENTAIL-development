#pragma once
/* Optional BLE FX command (bench-only). Parsed separately from legacy v1 SHOW.
 * Version 1 opcode 5, fits default ATT-MTU=23.
 * WLED-compatible effect ID is user-visible only in the one global effect library.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wled_fx_compat.h"
#define OT_BLE_FX_OPCODE 5u
/* Explicit volatile continuous playback; final wire byte reserved and zero. */
#define OT_BLE_FX_CONTINUOUS_OPCODE 10u
#define OT_BLE_FX_WIRE_BYTES 17u
/* V1 wire remains 17 bytes. New opcode 11 is an independently advertised
 * 20-byte volatile continuous FX write; 20 bytes fits ATT MTU 23.
 * Bytes 0..15 unchanged; byte 16 must be 1 (tertiary explicitly supplied),
 * bytes 17..19 are third RGB. Explicit 0,0,0 is real black.
 */
#define OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE 11u
#define OT_BLE_FX_TERTIARY_WIRE_BYTES 20u
#define OT_BLE_FX_TERTIARY_CAPABILITY "fx-tertiary-v1"
typedef struct {
  uint8_t r,g,b;
  bool set;
} ot_ble_fx_tertiary_t;

typedef struct {
  uint16_t effect_id;
  uint8_t chain_mask,zone_mask;
  uint8_t r,g,b,bg_r,bg_g,bg_b;
  uint8_t brightness,speed,intensity,palette_id,ttl_sec;
} ot_ble_fx_packet_t;
typedef enum {OT_BLE_FX_OK=0,OT_BLE_FX_BAD_WIRE,OT_BLE_FX_UNAUTHORIZED,
  OT_BLE_FX_UNSUPPORTED,OT_BLE_FX_RANGE} ot_ble_fx_result_t;
/* No side effects, no LED/vehicle logic. Authentication is managed by existing BLE GATT. */
ot_ble_fx_result_t ot_ble_fx_parse(const uint8_t *p,size_t n,bool authorized,
                                   ot_ble_fx_packet_t *out);

/* Reject effects without a source-proven third RGB consumer. Do not touch
 * existing on-flash ot_ble_fx_packet_t / preset / scene layouts. */
ot_ble_fx_result_t ot_ble_fx_parse_tertiary(const uint8_t *p,size_t n,
    bool authorized,ot_ble_fx_packet_t *out,ot_ble_fx_tertiary_t *third);
