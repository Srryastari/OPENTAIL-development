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
#define OT_BLE_FX_WIRE_BYTES 17u
typedef struct {
  uint16_t effect_id;
  uint8_t chain_mask,zone_mask;
  uint8_t r,g,b,bg_r,bg_g,bg_b;
  uint8_t brightness,speed,intensity,palette_id,ttl_sec;
  /* Internal bench-only HTTP customization; NOT transmitted as extra BLE
   * bytes and NOT part of the v1 17-byte wire ABI. */
  uint8_t spark_style,spark_peak,spark_r,spark_g,spark_b;
} ot_ble_fx_packet_t;
typedef enum {OT_BLE_FX_OK=0,OT_BLE_FX_BAD_WIRE,OT_BLE_FX_UNAUTHORIZED,
  OT_BLE_FX_UNSUPPORTED,OT_BLE_FX_RANGE} ot_ble_fx_result_t;
/* No side effects, no LED/vehicle logic. Authentication is managed by existing BLE GATT. */
ot_ble_fx_result_t ot_ble_fx_parse(const uint8_t *p,size_t n,bool authorized,
                                   ot_ble_fx_packet_t *out);
