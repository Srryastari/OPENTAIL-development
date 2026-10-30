#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "output_manager.h"
#include "trigger_manager.h"

/* Persistent recovery snapshot. Each NVS blob commits two independently valid
 * profiles atomically: the last active one and the requested new one. A boot
 * ALWAYS restores the previous profile first, leaving requested activation
 * to the physical GPIO sampler after a stable, fully released interval.
 *
 * The recovery copy is intentionally conservative: after a reboot with an
 * asserted trigger it can be older than the last live configuration, but a
 * saved, not-yet-activated mapping can never take over the held input.
 * No flash writes occur in the latency-sensitive GPIO sampler.
 */
#define OT_CFG_JOURNAL_VERSION 1u
#define OT_CFG_TM_MAGIC 0x4f544a54u
#define OT_CFG_OM_MAGIC 0x4f544a4fu

typedef struct {
    uint32_t magic;
    uint32_t version;
    ot_tm_profile_t previous;
    ot_tm_profile_t requested;
    uint32_t crc;
} ot_cfg_tm_journal_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    ot_om_output_t previous[OT_OM_OUTPUTS];
    ot_om_output_t requested[OT_OM_OUTPUTS];
    uint32_t crc;
} ot_cfg_om_journal_t;

bool ot_cfg_tm_pack(const ot_tm_profile_t *previous,
                    const ot_tm_profile_t *requested,ot_cfg_tm_journal_t *out);
bool ot_cfg_tm_unpack(const ot_cfg_tm_journal_t *in,
                      ot_tm_profile_t *previous,ot_tm_profile_t *requested);
bool ot_cfg_om_pack(const ot_om_output_t previous[OT_OM_OUTPUTS],
                    const ot_om_output_t requested[OT_OM_OUTPUTS],
                    ot_cfg_om_journal_t *out);
bool ot_cfg_om_unpack(const ot_cfg_om_journal_t *in,
                      ot_om_output_t previous[OT_OM_OUTPUTS],
                      ot_om_output_t requested[OT_OM_OUTPUTS]);
