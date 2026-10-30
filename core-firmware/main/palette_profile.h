#pragma once
/* Custom palette v1: independent from 261-byte output configuration and
 * legacy effect / scene wire formats. No NVS or BLE dependency. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wled_fx_compat.h"
#define OT_PAL_CUSTOM_SLOTS 128u
#define OT_PAL_STOP_MAX 18u
#define OT_PAL_STOP_BYTES 72u
#define OT_PAL_RECORD_BYTES 83u
#define OT_PAL_UPLOAD_TIMEOUT_MS 15000u
#define OT_PAL_MAGIC 0x4f54504cu
#define OT_PAL_VERSION 1u
typedef struct {
    uint8_t index;           /* palette ID = 200 - index; 201..255 reserved */
    uint16_t revision;       /* zero means absent and is never persisted */
    uint8_t stop_count;      /* 2..18, indexed RGB stops */
    uint8_t stops[OT_PAL_STOP_BYTES];
} ot_pal_profile_t;
typedef struct {
    ot_wled_palette16_t colors[OT_PAL_CUSTOM_SLOTS];
    uint16_t revision[OT_PAL_CUSTOM_SLOTS];
} ot_pal_bank_t;
typedef struct {
    ot_pal_profile_t profile;
    uint16_t expected_revision,expected_crc;
    uint8_t received;
    uint32_t last_ms;
    bool active;
} ot_pal_transfer_t;
uint16_t ot_pal_crc16(const uint8_t *bytes,size_t length);
bool ot_pal_validate(const ot_pal_profile_t *record);
bool ot_pal_encode(const ot_pal_profile_t *record,uint8_t bytes[OT_PAL_RECORD_BYTES]);
bool ot_pal_decode(const uint8_t *bytes,size_t length,ot_pal_profile_t *record);
bool ot_pal_bank_get(const ot_pal_bank_t *bank,uint8_t palette_id,
                     ot_wled_palette16_t *out,uint16_t *revision);
bool ot_pal_bank_put(ot_pal_bank_t *bank,const ot_pal_profile_t *record);
bool ot_pal_bank_remove(ot_pal_bank_t *bank,uint8_t index);
void ot_pal_transfer_reset(ot_pal_transfer_t *transfer);
bool ot_pal_transfer_begin(ot_pal_transfer_t *transfer,uint8_t index,
                            uint16_t expected_revision,uint8_t stop_count,
                            uint16_t expected_crc,uint32_t now_ms);
bool ot_pal_transfer_append(ot_pal_transfer_t *transfer,uint8_t index,
                             uint8_t offset,const uint8_t *data,size_t length,
                             uint32_t now_ms);
bool ot_pal_transfer_complete(const ot_pal_transfer_t *transfer,
                               uint32_t now_ms,ot_pal_profile_t *record);
