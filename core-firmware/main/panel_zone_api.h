#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "panel_zone.h"

#define OT_PZ_RESET_CHALLENGE_TTL_MS 5000u

/* Authenticated NimBLE host callbacks call these CORE state APIs. None owns
 * or writes the LED driver; led_task remains the sole RMT/DMA owner. */
bool ot_ble_panel_get(uint8_t lane,ot_pz_output_t *out,uint16_t *active_revision,
                      uint16_t *staged_revision,bool *pending,bool *persistent);
int ot_ble_panel_save(uint8_t lane,const ot_pz_output_t *next,uint16_t base_revision);
bool ot_ble_pixel_probe(const uint8_t *packet,size_t size,
                        bool authenticated,bool capability_seen);
void ot_ble_diag_disconnect(void);

/* Volatile authenticated live-arrangement preview; commit uses the existing journal. */
int ot_ble_panel_preview_begin(uint8_t lane,uint16_t base_revision,
                               uint16_t pixel_capacity);
int ot_ble_panel_preview_update_ranges(uint8_t lane,uint16_t base_revision,
                               const ot_pz_range_update_t *updates,size_t count);
int ot_ble_panel_preview_heartbeat(uint8_t lane,uint16_t base_revision);
bool ot_ble_panel_preview_crc32(uint8_t lane,uint16_t base_revision,uint32_t *crc32);
int ot_ble_panel_preview_commit(uint8_t lane,uint16_t base_revision,
                                uint32_t expected_crc32);
uint16_t ot_ble_panel_render_revision(uint8_t lane);
void ot_ble_panel_preview_cancel(void);

/* Authenticated two-step destructive map reset. Return 0 only after durable readback. */
int ot_ble_panel_reset_challenge(uint8_t lane,uint16_t expected_revision,
                                 uint32_t *challenge);
int ot_ble_panel_reset_confirm(uint8_t lane,uint16_t expected_revision,
                               uint32_t challenge);
