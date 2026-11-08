#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core_logic.h"

#define OT_PZ_DIAG_PROBE_OPCODE 0xaau
#define OT_PZ_DIAG_PROPOSED_FILL_OPCODE 0xabu /* documented proposal only; not accepted over BLE */
#define OT_PZ_DIAG_PACKET_BYTES 5u
#define OT_PZ_DIAG_LEASE_MS 2000u

typedef enum {OT_PZ_DIAG_NONE=0,OT_PZ_DIAG_PROBE=1,OT_PZ_DIAG_FILL=2} ot_pz_diag_mode_t;
typedef struct {
    uint8_t mode;
    uint8_t lane; /* zero-based physical output lane */
    uint8_t color; /* 0=off, 1=white, 2=cyan */
    uint16_t endpoint; /* probe index or inclusive cumulative fill endpoint */
    uint32_t expires_ms;
} ot_pz_diag_state_t;

void ot_pz_diag_clear(ot_pz_diag_state_t *state);
/* Authenticated, capability-negotiated exact five-byte Pixel Probe. */
bool ot_pz_diag_probe(ot_pz_diag_state_t *state,const uint8_t *packet,size_t size,
                      bool authenticated,bool capability_seen,
                      const uint16_t output_lengths[2],uint32_t now_ms);
/* CORE cumulative fill primitive. The BLE opcode remains unassigned pending owner review. */
bool ot_pz_diag_fill(ot_pz_diag_state_t *state,unsigned lane,uint16_t inclusive_endpoint,
                     uint8_t color,uint16_t output_length,uint32_t now_ms);
uint16_t ot_pz_diag_fill_count(const ot_pz_diag_state_t *state);
bool ot_pz_diag_expire(ot_pz_diag_state_t *state,uint32_t now_ms);
/* For fill, owns the entire selected lane so pixels after endpoint are forced dark. */
bool ot_pz_diag_render(const ot_pz_diag_state_t *state,unsigned lane,uint16_t pixel,
                       uint32_t now_ms,bool *owned,ot_rgb_t *color);
