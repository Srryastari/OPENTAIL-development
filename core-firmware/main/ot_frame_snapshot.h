#pragma once
/* Optional, one-shot post-render logical RGB888 diagnostic snapshot.
 * NO persistent capture, no controller-side preview rendering, no pixel writes.
 * The render task alone fills the buffer. BLE reads a frozen frame by sequence.
 * A captured RGB frame is not a guarantee of measured optical output timing. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OT_FRAME_SNAPSHOT_VERSION 1u
#define OT_FRAME_SNAPSHOT_CHUNK_PIXELS 56u
typedef struct {
    uint8_t output;
    uint16_t count;
    uint32_t sequence;
    uint16_t crc;
    bool ready;
    bool pending;
} ot_frame_snapshot_info_t;
/* Returns false when a capture is in flight or the output is invalid. */
bool ot_frame_snapshot_request(uint8_t output);
/* Called by the sole render task only for the chosen output. */
bool ot_frame_snapshot_begin(uint8_t output,uint16_t count,uint32_t sequence);
void ot_frame_snapshot_pixel(uint16_t pixel,uint8_t r,uint8_t g,uint8_t b);
void ot_frame_snapshot_complete(bool submitted);
/* Fail a still-pending request if its physical output cannot render. */
void ot_frame_snapshot_unavailable(uint8_t output);
/* Status and sequence-checked immutable slices; count in PIXELS. */
bool ot_frame_snapshot_info(ot_frame_snapshot_info_t *out);
bool ot_frame_snapshot_chunk(uint8_t output,uint32_t sequence,uint16_t first,
                              uint8_t count,uint8_t *rgb);
/* Clear on BLE session disconnect. */
void ot_frame_snapshot_reset(void);
