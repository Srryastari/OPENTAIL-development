#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Stable v1 wire format. Not a production vehicle-lighting transport. */
#define OT_BLE_WIRE_V1 1u
#define OT_BLE_AUTH 1u
#define OT_BLE_SHOW 2u
#define OT_BLE_STOP 3u
/* Graceful release of session-owned temporary FX before BLE link teardown. */
#define OT_BLE_RELEASE_TEMPORARY 9u
#define OT_BLE_FX_SAVE_PRESET 6u
#define OT_BLE_FX_PLAY_PRESET 7u
#define OT_BLE_FX_DELETE_PRESET 8u
#define OT_BLE_TOKEN_BYTES 16u
#define OT_BLE_SHOW_BYTES 11u
/* Modes match native show_logic.h: 1=solid,2=chase,3=breathe,4=wipe,5=rainbow. */
typedef struct {uint8_t effect,chain_mask,zone_mask,r,g,b,brightness,speed,ttl_sec;} ot_ble_show_packet_t;
typedef enum {OT_BLE_OK_AUTH,OT_BLE_OK_SHOW,OT_BLE_OK_STOP,OT_BLE_OK_FX,OT_BLE_OK_SAVED,OT_BLE_OK_LOADED,OT_BLE_OK_DELETED,
  OT_BLE_ERR_WIRE,OT_BLE_ERR_AUTH,OT_BLE_ERR_RANGE,OT_BLE_ERR_BUSY} ot_ble_result_t;
/* Only auth packets can mark this session authorized; caller clears on disconnect. */
ot_ble_result_t ot_ble_parse(const uint8_t *packet,size_t length,
  const uint8_t secret[OT_BLE_TOKEN_BYTES],bool *authenticated,
  ot_ble_show_packet_t *show);
const char *ot_ble_result_string(ot_ble_result_t result);
