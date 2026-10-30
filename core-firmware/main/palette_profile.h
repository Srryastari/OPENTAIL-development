#pragma once
/* CORE custom palette protocol v3. Independent of BLE output/trigger ABI.
 * User entries 0..127 map one-to-one to palette IDs 200..73. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wled_fx_compat.h"
#define OT_PALETTE_SLOTS 128u
#define OT_PALETTE_STOPS_MAX 18u
#define OT_PALETTE_BYTES_MAX (OT_PALETTE_STOPS_MAX*4u)
#define OT_PALETTE_RECORD_MAGIC 0x4f54504cu
#define OT_PALETTE_RECORD_VERSION 1u
#define OT_PALETTE_WIRE_VERSION 3u
#define OT_PALETTE_WIRE_MAX 20u
#define OT_PALETTE_CHUNK_MAX 15u
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t index;
    uint8_t count;
    uint16_t byte_length;
    uint32_t revision;
    uint8_t stops[OT_PALETTE_BYTES_MAX];
    uint16_t crc;
} ot_palette_record_t;
typedef struct {
    ot_wled_palette16_t palette;
    uint32_t revision;
    uint16_t stops_crc;
    uint8_t count;
    bool present;
} ot_palette_entry_t;
uint16_t ot_palette_crc16(const uint8_t *p,size_t n);
bool ot_palette_record_build(ot_palette_record_t *r,uint8_t index,uint8_t count,
                              uint32_t revision,const uint8_t *stops);
bool ot_palette_record_tombstone(ot_palette_record_t *r,uint8_t index,uint32_t revision);
bool ot_palette_record_is_tombstone(const ot_palette_record_t *r);
bool ot_palette_record_valid(const ot_palette_record_t *r,
                              ot_wled_palette16_t *palette);
bool ot_palette_entry_from_record(ot_palette_entry_t *entry,
                                  const ot_palette_record_t *record);
bool ot_palette_id_to_index(uint8_t palette_id,uint8_t *out_index);
