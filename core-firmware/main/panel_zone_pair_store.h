#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config_journal.h"

typedef enum {
    OT_PZ_PAIR_NS_OUTPUT_CFG=0,
    OT_PZ_PAIR_NS_ZONE_MAP=1
} ot_pz_pair_namespace_t;

typedef enum {
    OT_PZ_PAIR_IO_OK=0,
    OT_PZ_PAIR_IO_NOT_FOUND=1,
    OT_PZ_PAIR_IO_ERROR=2
} ot_pz_pair_io_result_t;

/* The CORE adapter must make write_commit/erase_commit durable before return.
 * This keeps the same runtime transaction algorithm testable with host NVS. */
#define OT_PZ_PAIR_STORE_MARKER_KEY "pair_v4_init"
#define OT_PZ_PAIR_STORE_MARKER_MAGIC 0x4f54564du
#define OT_PZ_PAIR_STORE_MARKER_VERSION 1u
typedef struct {
    uint32_t magic,version,crc,transaction_id;
} ot_pz_pair_store_marker_t;

typedef struct {
    void *context;
    ot_pz_pair_io_result_t (*read)(void *context,ot_pz_pair_namespace_t name_space,
                                   const char *key,void *data,size_t *size);
    bool (*write_commit)(void *context,ot_pz_pair_namespace_t name_space,
                         const char *key,const void *data,size_t size);
    bool (*erase_commit)(void *context,ot_pz_pair_namespace_t name_space,
                         const char *key);
} ot_pz_pair_store_io_t;

typedef bool (*ot_pz_pair_phase_hook_t)(void *context,uint8_t phase,
                                        uint32_t transaction_id);

typedef struct {
    bool has_lane_active,has_lane_stage;
    bool has_map_active,has_map_stage,has_intent,has_marker;
    ot_pz_lane_journal_t lane_active,lane_stage;
    ot_pz_map_journal_t map_active,map_stage;
    ot_pz_pair_intent_t intent;
    ot_pz_pair_store_marker_t marker;
} ot_pz_pair_store_snapshot_t;

bool ot_pz_pair_store_snapshot(const ot_pz_pair_store_io_t *io,
                               ot_pz_pair_store_snapshot_t *snapshot);
bool ot_pz_pair_store_recover(const ot_pz_pair_store_io_t *io,
                              const ot_pz_controller_t *legacy_fallback,
                              ot_pz_controller_t *live,ot_pz_pending_t *pending,
                              uint16_t active_revision[OT_PZ_OUTPUTS],
                              uint16_t staged_revision[OT_PZ_OUTPUTS],
                              bool *intent_pending,uint32_t *transaction_id,
                              uint8_t *reset_mask);

/* Writes lane stage, map stage, then pair intent: durable phases 1..3.
 * allow_empty_active is reserved for first-time legacy migration only. */
bool ot_pz_pair_store_begin(const ot_pz_pair_store_io_t *io,
                            const ot_pz_controller_t *previous,
                            const ot_pz_controller_t *candidate,
                            const uint16_t previous_revision[OT_PZ_OUTPUTS],
                            const uint16_t candidate_revision[OT_PZ_OUTPUTS],
                            uint32_t transaction_id,uint8_t reset_mask,
                            bool allow_empty_active,
                            ot_pz_pair_phase_hook_t phase_hook,void *phase_context);

/* Completes phases 4..9 idempotently. It can resume from any durable cut
 * while the pair intent remains present. */
bool ot_pz_pair_store_finish(const ot_pz_pair_store_io_t *io,
                             uint32_t transaction_id,
                             ot_pz_pair_phase_hook_t phase_hook,void *phase_context);

/* Records a reset of an already-empty lane without writing a combined profile. */
bool ot_pz_pair_store_mark_reset(const ot_pz_pair_store_io_t *io,uint8_t lane);
