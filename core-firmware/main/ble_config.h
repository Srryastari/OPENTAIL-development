#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "output_manager.h"
/* Explicit, ABI-independent, little-endian BLE output mapping wire format.
 * 261 bytes per output: 1 ver, 2 pixels, 1 order, 1 zones, 25 name,
 * 3 RGB gain, 3 white gain, 1 gamma, 8*(25 name, 2 pixels, 1 role).
 */
#define OT_CFG_WIRE_VERSION 1u
#define OT_CFG_WIRE_BYTES 261u
#define OT_CFG_CHUNK_BYTES 15u /* write payload; read replies carry up to 14 */
#define OT_CFG_PACKET_BYTES 20u
uint16_t ot_cfg_crc16(const uint8_t *p,size_t size);
void ot_cfg_encode(const ot_om_output_t *src,uint8_t dest[OT_CFG_WIRE_BYTES]);
bool ot_cfg_decode(const uint8_t bytes[OT_CFG_WIRE_BYTES],ot_om_output_t *dest);
