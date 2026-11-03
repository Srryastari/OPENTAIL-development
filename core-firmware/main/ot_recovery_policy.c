#include "ot_recovery_policy.h"
#define OT_RECOVERY_MAGIC 0x4f545731u
static void seal(ot_recovery_record_t *r) {
    r->magic=OT_RECOVERY_MAGIC;
    r->seal=~(r->magic ^ r->failures);
}
bool ot_recovery_record_valid(const ot_recovery_record_t *r) {
    return r && r->magic==OT_RECOVERY_MAGIC && r->failures<=OT_RECOVERY_BOOT_LIMIT &&
           r->seal==~(r->magic ^ r->failures);
}
void ot_recovery_boot(ot_recovery_record_t *r, ot_reset_class_t reason) {
    if(!ot_recovery_record_valid(r) || reason==OT_RESET_POWER)r->failures=0u;
    /* Count abnormal resets even if RTC contents were lost (e.g. brownout).
     * Software OTA/manual restarts preserve, but never increment, the streak. */
    if(reason!=OT_RESET_POWER && reason!=OT_RESET_SOFTWARE &&
       r->failures<OT_RECOVERY_BOOT_LIMIT)r->failures++;
    seal(r);
}
void ot_recovery_stable(ot_recovery_record_t *r) {r->failures=0u;seal(r);}
bool ot_recovery_quarantined(const ot_recovery_record_t *r) {
    return ot_recovery_record_valid(r) && r->failures>=OT_RECOVERY_BOOT_LIMIT;
}
bool ot_recovery_stable_due(uint32_t now, uint32_t since) {
    return (uint32_t)(now-since)>=OT_RECOVERY_STABLE_MS;
}
const char *ot_recovery_reset_name(ot_reset_class_t reason) {
    switch(reason) {
    case OT_RESET_POWER:return "power_on";
    case OT_RESET_SOFTWARE:return "software";
    case OT_RESET_TASK_WDT:return "task_watchdog";
    case OT_RESET_INTERRUPT_WDT:return "interrupt_watchdog";
    case OT_RESET_OTHER_WDT:return "other_watchdog";
    case OT_RESET_PANIC:return "panic";
    case OT_RESET_BROWNOUT:return "brownout";
    case OT_RESET_OTA_FAILURE:return "ota_confirmation_failure";
    default:return "other";
    }
}
