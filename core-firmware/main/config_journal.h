#pragma once
/* Fail-closed NVS profile journal. Every durable candidate retains its
 * pre-edit active profile and revisions so a held-input reboot cannot
 * silently activate a mapping whose publication was interrupted.
 * The protocol's 16-bit BLE revision field never wraps or resets. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "trigger_manager.h"
#include "output_manager.h"
#define OT_CFG_JOURNAL_MAGIC 0x4f544a33u
#define OT_CFG_JOURNAL_VERSION 2u
#define OT_CFG_JOURNAL_KEY_ACTIVE "active_v3"
#define OT_CFG_JOURNAL_KEY_STAGED "stage_v3"

/* V1 is read-only migration input (original #46 draft). */
typedef struct {
    uint32_t magic,version,crc;
    ot_tm_profile_t previous,candidate;
} ot_tm_journal_v1_t;
typedef struct {
    uint32_t magic,version,crc;
    ot_om_output_t previous[OT_OM_OUTPUTS],candidate[OT_OM_OUTPUTS];
} ot_om_journal_v1_t;

typedef struct {
    uint32_t magic,version,crc;
    ot_tm_profile_t previous,candidate;
    uint16_t previous_revision,candidate_revision;
} ot_tm_journal_t;
typedef struct {
    uint32_t magic,version,crc;
    ot_om_output_t previous[OT_OM_OUTPUTS],candidate[OT_OM_OUTPUTS];
    uint16_t previous_revision[OT_OM_OUTPUTS];
    uint16_t candidate_revision[OT_OM_OUTPUTS];
} ot_om_journal_t;
uint32_t ot_cfg_crc32(const void *data,size_t bytes);
bool ot_tm_journal_build(ot_tm_journal_t *out,const ot_tm_profile_t *previous,
                         const ot_tm_profile_t *candidate);
bool ot_om_journal_build(ot_om_journal_t *out,
                         const ot_om_output_t previous[OT_OM_OUTPUTS],
                         const ot_om_output_t candidate[OT_OM_OUTPUTS]);
bool ot_tm_journal_build_revisions(ot_tm_journal_t *out,
                         const ot_tm_profile_t *previous,
                         const ot_tm_profile_t *candidate,
                         uint16_t previous_revision,uint16_t candidate_revision);
bool ot_om_journal_build_revisions(ot_om_journal_t *out,
                         const ot_om_output_t previous[OT_OM_OUTPUTS],
                         const ot_om_output_t candidate[OT_OM_OUTPUTS],
                         const uint16_t previous_revision[OT_OM_OUTPUTS],
                         const uint16_t candidate_revision[OT_OM_OUTPUTS]);
bool ot_tm_journal_valid(const ot_tm_journal_t *record);
bool ot_om_journal_valid(const ot_om_journal_t *record);
bool ot_tm_journal_upgrade_v1(const ot_tm_journal_v1_t *v1,ot_tm_journal_t *v2);
bool ot_om_journal_upgrade_v1(const ot_om_journal_v1_t *v1,ot_om_journal_t *v2);
bool ot_tm_journal_recover(const ot_tm_journal_t *active,
                           const ot_tm_journal_t *staged,
                           ot_tm_profile_t *live,ot_tm_pending_t *pending);
bool ot_om_journal_recover(const ot_om_journal_t *active,
                           const ot_om_journal_t *staged,
                           ot_om_output_t live[OT_OM_OUTPUTS],
                           ot_om_pending_t *pending);
