#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core_logic.h"

/* BENCH-ONLY: physical input pin indexes are fixed, their logical functions are configurable.
 * Logical function bits preserve the v0.1.17 OT_SIG_* / AUX output-role contract.
 * The optocoupler polarity is still controlled by board_config.h.
 */
#define OT_TM_VERSION 1
#define OT_TM_MAGIC 0x4f54544du

typedef enum {
    OT_TM_DISABLED=0, OT_TM_PARK, OT_TM_BRAKE, OT_TM_LEFT, OT_TM_RIGHT,
    OT_TM_REVERSE, OT_TM_AUX1, OT_TM_AUX2, OT_TM_AUX3
} ot_tm_function_t;

typedef struct {
    uint8_t function[OT_IN_COUNT]; /* IN1..IN8 order */
} ot_tm_profile_t;

void ot_tm_defaults(ot_tm_profile_t *p);
bool ot_tm_valid(const ot_tm_profile_t *p);
const char *ot_tm_name(uint8_t function);
int ot_tm_from_name(const char *name);
/* Canonical function-name to logical signal mask for bench simulation and tests.
 * Disabled/unknown names map to zero; never interpret simulation names as GPIO pins. */
uint8_t ot_tm_signal_mask_of(const char *name);
uint8_t ot_tm_resolve(const ot_tm_profile_t *p,uint8_t physical_bits);

/* Authenticated BLE trigger-profile v1: one 11-byte write fits default ATT MTU.
 * [version, expected_revision LE16, IN1..IN8 logical function IDs].
 * No GPIO numbers, LED geometry, or vehicle-specific function assumptions. */
#define OT_TM_BLE_VERSION 1u
#define OT_TM_BLE_WRITE_BYTES (3u+OT_IN_COUNT)
#define OT_TM_BLE_READ_BYTES (7u+OT_IN_COUNT)
bool ot_tm_ble_decode(const uint8_t *wire,size_t size,
                      uint16_t *expected_revision,ot_tm_profile_t *out);

/* Pure encoder for the 15-byte owner-authenticated trigger GATT readback.
 * [version,status,revision LE16,raw input mask,effective function mask,
 *  persisted flag,IN1..IN8 function IDs]. Returns false for invalid profile. */
bool ot_tm_ble_encode(const ot_tm_profile_t *profile,uint16_t revision,
                      uint8_t raw,uint8_t effective,bool persisted,uint8_t status,
                      uint8_t wire[OT_TM_BLE_READ_BYTES]);
