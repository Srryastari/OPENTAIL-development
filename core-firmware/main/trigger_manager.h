#pragma once
#include <stdbool.h>
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
uint8_t ot_tm_resolve(const ot_tm_profile_t *p,uint8_t physical_bits);
