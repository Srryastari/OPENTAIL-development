#pragma once
/* OpenTail dual-MCU failover SOFTWARE CONTRACT, not a physical mux driver.
 *
 * No electrical interlock, output-enable GPIO, backup rail, backup NVS writer,
 * heartbeat wire or automatic takeover is implemented in this module.
 * Pure deterministic policy/render functions may be shared by a future
 * separately built backup application only after hardware review.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "output_manager.h"
#include "trigger_manager.h"

#define OT_FO_PROFILE_VERSION 1u
#define OT_FO_REQUIRED_SIGNALS OT_USED_INPUTS /* Park, Brake, Left, Right, Reverse */

typedef struct {
    uint16_t version;
    uint32_t generation; /* nonzero independently committed transaction ID */
    uint16_t output_revision[OT_OM_OUTPUTS];
    uint16_t trigger_revision;
    uint8_t required_signals;
    ot_tm_profile_t trigger;
    ot_om_output_t output[OT_OM_OUTPUTS];
    uint32_t digest; /* canonical integrity check, NOT authentication */
} ot_fo_profile_t;

typedef enum {OT_FO_PRIMARY=1, OT_FO_BACKUP=2} ot_fo_role_t;
typedef enum {
    OT_FO_DENY_INVALID=0,
    OT_FO_DENY_NO_INTERLOCK,
    OT_FO_DENY_CONTENTION,
    OT_FO_DENY_NO_GRANT,
    OT_FO_DENY_UNHEALTHY,
    OT_FO_DENY_HANDOFF_INCOMPLETE,
    OT_FO_GRANT_QUALIFIED
} ot_fo_decision_t;

/* All fields below must be sourced from independently qualified HARDWARE
 * and frame-path measurements, never inferred from a BLE heartbeat or an
 * optimistic timer. No approved GPIO mapping or electrical grant exists yet.
 */
typedef struct {
    bool interlock_physically_verified;
    bool primary_grant;
    bool backup_grant;
    bool local_rail_ok;
    bool independent_input_sampler_ok;
    bool completed_frame_progress_ok;
    bool local_watchdog_ok;
    bool same_committed_profile_on_both_mcus;
    bool break_before_make_verified;
    bool fresh_full_frame_after_handoff;
} ot_fo_qualification_t;

/* No defaults or vehicle-specific zone geometry: callers supply a complete
 * currently ACTIVE, installed and validated snapshot only. Never stage a
 * pending profile as active. This function performs no NVS I/O.
 */
bool ot_fo_profile_seal(ot_fo_profile_t *dst,
                        const ot_om_output_t outputs[OT_OM_OUTPUTS],
                        const ot_tm_profile_t *trigger,
                        const uint16_t revisions[OT_OM_OUTPUTS],
                        uint16_t trigger_revision, uint32_t generation,
                        bool committed_active_snapshot);
bool ot_fo_profile_valid(const ot_fo_profile_t *profile);
bool ot_fo_profiles_match(const ot_fo_profile_t *primary,
                          const ot_fo_profile_t *backup);
/* Fail closed: no new profile may activate unless both independent stores
 * read back the same complete revision and physical inputs are idle.
 * This is a PRECONDITION, not a durable two-phase/NVS transport.
 */
bool ot_fo_can_activate_profile(const ot_fo_profile_t *primary_new,
                                const ot_fo_profile_t *backup_new,
                                uint8_t raw_inputs, uint8_t debounced_inputs);

/* Pure decision: never toggles output GPIO or enables an RMT lane. The actual
 * hardware arbiter MUST enforce exclusive grants even if firmware malfunctions.
 */
ot_fo_decision_t ot_fo_assess_grant(ot_fo_role_t role,
                                    const ot_fo_profile_t *profile,
                                    const ot_fo_qualification_t *health);

/* Essential-only logical RGB frame. Uses the *backup's own* isolated physical
 * input mask, maps user-configured zones, never plays effects/AUX/overlays or
 * touches output hardware. Validated grant AND independent electrical mux
 * must be enforced by an eventual backup runtime.
 */
bool ot_fo_render_essential(const ot_fo_profile_t *profile, unsigned output,
                             uint8_t independent_physical_inputs,
                             uint8_t brightness, ot_rgb_t *logical_rgb,
                             size_t pixel_capacity);
