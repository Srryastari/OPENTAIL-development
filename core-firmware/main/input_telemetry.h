#pragma once
#include <stdint.h>

#define OT_INPUT_DIAG_INPUTS 5u
#define OT_INPUT_DIAG_WIRE_VERSION 1u
#define OT_INPUT_DIAG_WIRE_BYTES 82u

typedef struct {
    uint32_t raw_edges[OT_INPUT_DIAG_INPUTS];
    uint32_t asserted_edges[OT_INPUT_DIAG_INPUTS];
    uint32_t deasserted_edges[OT_INPUT_DIAG_INPUTS];
    uint32_t edge_epoch;
    uint32_t turn_queue_overflow_count[2];
    uint8_t raw_asserted_mask;
    uint8_t debounced_asserted_mask;
    uint8_t turn_queue_depth[2];
    uint8_t turn_queue_high_water[2];
    uint8_t turn_queue_overflow_flag[2];
} ot_input_diag_snapshot_t;

/* CORE owns capture and atomics; BLE only reads a bounded immutable snapshot. */
void ot_input_diag_snapshot_get(ot_input_diag_snapshot_t *out);
