#include "ot_single_watchdog.h"
#include <stddef.h>

uint32_t ot_sw_rtc_check(const ot_sw_rtc_record_t *record){
    if(!record)return 0u;
    return 0x6C59C137u ^ record->magic ^
           (record->crash_streak*0x45D9F3Bu) ^
           (record->stable_boot*0x9E3779B9u);
}
bool ot_sw_rtc_valid(const ot_sw_rtc_record_t *record){
    return record && record->magic==OT_SW_RTC_MAGIC &&
           record->stable_boot<=1u &&
           record->crash_streak<=OT_SW_BOOTLOOP_LIMIT &&
           record->check==ot_sw_rtc_check(record);
}
bool ot_sw_is_crash_reset(ot_sw_reset_class_t reason){
    return reason==OT_SW_RESET_PANIC ||
           reason==OT_SW_RESET_TASK_WDT ||
           reason==OT_SW_RESET_INTERRUPT_WDT ||
           reason==OT_SW_RESET_OTHER_WDT ||
           reason==OT_SW_RESET_CPU_LOCKUP;
}
ot_sw_boot_t ot_sw_boot_record(ot_sw_rtc_record_t *record,
                              ot_sw_reset_class_t reason){
    ot_sw_boot_t boot={.reset_class=reason};
    if(!record)return boot;
    const bool prior_valid=ot_sw_rtc_valid(record);
    const uint32_t prior_streak=prior_valid?record->crash_streak:0u;
    const bool prior_stable=prior_valid && record->stable_boot!=0u;
    if(ot_sw_is_crash_reset(reason)){
        /* A healthy completed boot starts a NEW streak. Bound the counter,
         * including corrupt/uninitialised RTC and repeated boot-loop panics. */
        boot.crash_streak_at_boot=prior_stable?1u:
            (prior_streak<OT_SW_BOOTLOOP_LIMIT?prior_streak+1u:OT_SW_BOOTLOOP_LIMIT);
    }
    /* Software OTA restart, power-on and brownout are distinct events and
     * do not falsely count as consecutive application watchdog resets. */
    boot.boot_loop_seen=boot.crash_streak_at_boot>=OT_SW_BOOTLOOP_LIMIT;
    record->magic=OT_SW_RTC_MAGIC;
    record->crash_streak=boot.crash_streak_at_boot;
    record->stable_boot=0u;
    record->check=ot_sw_rtc_check(record);
    return boot;
}
bool ot_sw_can_mark_stable(uint32_t elapsed_ms, uint32_t sampled_inputs,
                           uint32_t full_frames, bool output_fault,
                           bool config_fault){
    return elapsed_ms>=OT_SW_STABLE_MS &&
           sampled_inputs>=OT_SW_MIN_INPUT_SAMPLES &&
           full_frames>=OT_SW_MIN_FULL_FRAMES &&
           !output_fault&&!config_fault;
}
bool ot_sw_mark_stable(ot_sw_rtc_record_t *record, ot_sw_boot_t *boot,
                        uint32_t elapsed_ms, uint32_t sampled_inputs,
                        uint32_t full_frames, bool output_fault,
                        bool config_fault){
    if(!record||!boot||boot->stable ||
       !ot_sw_can_mark_stable(elapsed_ms,sampled_inputs,full_frames,
                              output_fault,config_fault))return false;
    boot->stable=true;
    record->magic=OT_SW_RTC_MAGIC;
    record->crash_streak=0u;
    record->stable_boot=1u;
    record->check=ot_sw_rtc_check(record);
    return true;
}
bool ot_sw_render_progress_eligible(bool all_lanes_completed,
                                     bool diagnosed_output_fault){
    return all_lanes_completed||diagnosed_output_fault;
}
bool ot_sw_decorative_inhibited(const ot_sw_boot_t *boot){
    return boot && boot->boot_loop_seen&&!boot->stable;
}
const char *ot_sw_reset_name(ot_sw_reset_class_t reason){
    switch(reason){
        case OT_SW_RESET_POWER:return "power_on";
        case OT_SW_RESET_SOFTWARE:return "software";
        case OT_SW_RESET_PANIC:return "panic";
        case OT_SW_RESET_TASK_WDT:return "task_wdt";
        case OT_SW_RESET_INTERRUPT_WDT:return "interrupt_wdt";
        case OT_SW_RESET_OTHER_WDT:return "other_wdt";
        case OT_SW_RESET_BROWNOUT:return "brownout";
        case OT_SW_RESET_CPU_LOCKUP:return "cpu_lockup";
        default:return "other";
    }
}
