#pragma once
/* Authenticated caller-only v3 transport; GATT session ownership remains in
 * ble_status.c. This module never changes the old BLE/FX/config packets. */
#include <stddef.h>
#include <stdint.h>
#define OT_PALETTE_REPLY_BYTES 12u
void ot_palette_ble_session_reset(void);
void ot_palette_ble_process(const uint8_t *p,size_t n,uint32_t now_ms);
void ot_palette_ble_reply(uint8_t out[OT_PALETTE_REPLY_BYTES]);
