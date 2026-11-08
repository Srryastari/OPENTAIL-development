#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "panel_zone.h"

/* Authenticated NimBLE host callbacks call these CORE state APIs. None owns
 * or writes the LED driver; led_task remains the sole RMT/DMA owner. */
bool ot_ble_panel_get(uint8_t lane,ot_pz_output_t *out,uint16_t *active_revision,
                      uint16_t *staged_revision,bool *pending,bool *persistent);
int ot_ble_panel_save(uint8_t lane,const ot_pz_output_t *next,uint16_t base_revision);
bool ot_ble_pixel_probe(const uint8_t *packet,size_t size,
                        bool authenticated,bool capability_seen);
void ot_ble_diag_disconnect(void);
