#pragma once
/* NVS v3 transactional mapping/geometry journal.
 * A pending record always wins over an active record at boot: it contains
 * the previous safe live image and the staged candidate in ONE CRC-checked
 * blob. The legacy v1/v2 keys are never erased or reinterpreted.
 * Pure C: NVS operations and all GPIO admission belong to CORE. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "trigger_manager.h"
#include "output_manager.h"
#define OT_CFG_JOURNAL_MAGIC 0x4f544a33u
#define OT_CFG_JOURNAL_VERSION 1u
#define OT_CFG_JOURNAL_KEY_ACTIVE "active_v3"
#define OT_CFG_JOURNAL_KEY_STAGED "stage_v3"
typedef struct {
    uint32_t magic,version,crc;
    ot_tm_profile_t previous,candidate;
} ot_tm_journal_t;
typedef struct {
    uint32_t magic,version,crc;
    ot_om_output_t previous[OT_OM_OUTPUTS],candidate[OT_OM_OUTPUTS];
} ot_om_journal_t;
uint32_t ot_cfg_crc32(const void *data,size_t bytes);
bool ot_tm_journal_build(ot_tm_journal_t *out,const ot_tm_profile_t *previous,
                         const ot_tm_profile_t *candidate);
bool ot_tm_journal_valid(const ot_tm_journal_t *record);
bool ot_om_journal_build(ot_om_journal_t *out,
                         const ot_om_output_t previous[OT_OM_OUTPUTS],
                         const ot_om_output_t candidate[OT_OM_OUTPUTS]);
bool ot_om_journal_valid(const ot_om_journal_t *record);
bool ot_tm_journal_recover(const ot_tm_journal_t *active,
                           const ot_tm_journal_t *staged,
                           ot_tm_profile_t *live,ot_tm_pending_t *pending);
bool ot_om_journal_recover(const ot_om_journal_t *active,
                           const ot_om_journal_t *staged,
                           ot_om_output_t live[OT_OM_OUTPUTS],
                           ot_om_pending_t *pending);
