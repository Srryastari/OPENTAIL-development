#pragma once
/* Firmware-owned durable palette bank. No changes to output/trigger NVS ABI. */
#include <stdbool.h>
#include <stdint.h>
#include "palette_profile.h"
void ot_palette_runtime_load(void);
bool ot_palette_runtime_get(uint8_t index,ot_palette_entry_t *out);
/* Authenticated readback of the exact persisted stop bytes; never from a
 * stale renderer frame or a substituted built-in palette. */
bool ot_palette_runtime_read_record(uint8_t index,ot_palette_record_t *out);
bool ot_palette_runtime_put(const ot_palette_record_t *record,uint32_t expected_revision);
bool ot_palette_runtime_delete(uint8_t index,uint32_t expected_revision);
/* All requested indices copied under ONE short critical section. Out data is
 * caller-owned and remains immutable for the entire render frame. */
void ot_palette_runtime_snapshot(const uint8_t *indices,size_t count,
                                 ot_palette_entry_t *out);
