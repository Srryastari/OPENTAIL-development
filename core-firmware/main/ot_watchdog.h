#pragma once
#include "ot_recovery_policy.h"
#include <stddef.h>
typedef enum {OT_WATCH_STARTUP, OT_WATCH_INPUTS, OT_WATCH_CONFIG,
              OT_WATCH_RENDER, OT_WATCH_COUNT} ot_watch_id_t;
/* Call before NVS/GPIO initialization. Failure to arm is fatal, never hidden. */
void ot_watchdog_init(void);
void ot_watchdog_arm(ot_watch_id_t id);
void ot_watchdog_disarm(ot_watch_id_t id);
void ot_watchdog_progress(ot_watch_id_t id);
void ot_watchdog_phase(ot_recovery_phase_t phase);
void ot_watchdog_frame(bool full_frame, bool boot_confirmed);
bool ot_watchdog_quarantined(void);
void ot_watchdog_format_json(char *out, size_t size);
