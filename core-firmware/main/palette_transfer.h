#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "palette_profile.h"
#define OT_PALETTE_TRANSFER_TIMEOUT_MS 5000u
typedef struct {
    uint8_t bytes[OT_PALETTE_BYTES_MAX];
    uint8_t index,count;
    uint16_t expected_crc;
    uint32_t expected_revision,started_ms;
    size_t received;
    bool active;
} ot_palette_transfer_t;
void ot_palette_transfer_reset(ot_palette_transfer_t *u);
bool ot_palette_transfer_begin(ot_palette_transfer_t *u,uint8_t index,
    uint8_t count,uint32_t expected_revision,uint16_t expected_crc,uint32_t now_ms);
bool ot_palette_transfer_chunk(ot_palette_transfer_t *u,uint16_t offset,
    const uint8_t *data,size_t count,uint32_t now_ms);
bool ot_palette_transfer_finish(ot_palette_transfer_t *u,uint8_t index,
    uint32_t now_ms,ot_palette_record_t *out);
