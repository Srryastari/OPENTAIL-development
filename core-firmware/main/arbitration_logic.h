#pragma once
#include "core_logic.h"
/* BENCH-ONLY priority model, not an automotive fail-safe. */
typedef enum {OT_ARB_IDLE=0, OT_ARB_VEHICLE, OT_ARB_DEMO, OT_ARB_SHOW} ot_arb_mode_t;
typedef enum {OT_ZONE_OFF=0, OT_ZONE_PARK, OT_ZONE_BRAKE, OT_ZONE_LEFT_TURN,
              OT_ZONE_RIGHT_TURN, OT_ZONE_REVERSE, OT_ZONE_DEMO, OT_ZONE_SHOW} ot_zone_mode_t;
typedef struct {
    ot_arb_mode_t mode;
    uint8_t signals; /* all eight physical/simulated inputs; AUX zones may be assigned */
    bool hazards;
    bool brake_over_parking;
    bool show_requested;
    bool show_suppressed;
    bool demo_requested;
    bool demo_suppressed;
} ot_arbitration_t;
ot_arbitration_t ot_arbitrate(uint8_t effective_mask,bool demo_requested,bool show_requested);
ot_zone_mode_t ot_arb_zone(const ot_arbitration_t *state, int chain, int section,
                           uint8_t show_chain_mask,uint16_t show_section_mask);
const char *ot_arb_mode_name(ot_arb_mode_t mode);
const char *ot_zone_mode_name(ot_zone_mode_t mode);
