#pragma once
#include <stdbool.h>
#include <stdint.h>

#define OT_RECOVERY_BOOT_LIMIT 3u
#define OT_RECOVERY_STABLE_MS 60000u
#define OT_RECOVERY_OTA_MS 30000u
/* Stable diagnostic IDs, independent of ESP-IDF reset-reason numbering. */
typedef enum {
    OT_RESET_POWER, OT_RESET_SOFTWARE, OT_RESET_TASK_WDT, OT_RESET_INTERRUPT_WDT,
    OT_RESET_OTHER_WDT, OT_RESET_PANIC, OT_RESET_BROWNOUT, OT_RESET_OTHER,
    OT_RESET_OTA_FAILURE
} ot_reset_class_t;
typedef enum {
    OT_PHASE_BOOT, OT_PHASE_RENDER, OT_PHASE_RESIZE,
    OT_PHASE_SUBMIT_O1, OT_PHASE_SUBMIT_O2,
    OT_PHASE_COMPLETE_O1, OT_PHASE_COMPLETE_O2, OT_PHASE_FRAME_DONE,
    OT_PHASE_OTA_TIMEOUT
} ot_recovery_phase_t;
/* RTC only: no per-boot or per-frame flash writes. Never stores configuration. */
typedef struct {
    uint32_t magic, failures, seal;
} ot_recovery_record_t;
bool ot_recovery_record_valid(const ot_recovery_record_t *r);
void ot_recovery_boot(ot_recovery_record_t *r, ot_reset_class_t reason);
void ot_recovery_stable(ot_recovery_record_t *r);
bool ot_recovery_quarantined(const ot_recovery_record_t *r);
bool ot_recovery_stable_due(uint32_t now_ms, uint32_t healthy_since_ms);
const char *ot_recovery_reset_name(ot_reset_class_t reason);
