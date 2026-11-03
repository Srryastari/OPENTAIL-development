#include "ot_single_watchdog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Deterministic event timeline: NO MCU/hardware execution claimed.
 * RTC-only streak, task stall, RMT completion, fault and OTA readiness gates. */
static void repeated_crash_and_bootloop(void){
    ot_sw_rtc_record_t rtc={0};
    assert(!ot_sw_rtc_valid(&rtc));
    ot_sw_boot_t b=ot_sw_boot_record(&rtc,OT_SW_RESET_POWER);
    assert(ot_sw_rtc_valid(&rtc)&&!b.boot_loop_seen && b.crash_streak_at_boot==0);
    assert(!ot_sw_decorative_inhibited(&b));
    for(unsigned i=1;i<=OT_SW_BOOTLOOP_LIMIT;i++){
        b=ot_sw_boot_record(&rtc,OT_SW_RESET_TASK_WDT);
        assert(ot_sw_rtc_valid(&rtc));
        assert(b.crash_streak_at_boot==i);
        assert(b.boot_loop_seen==(i==OT_SW_BOOTLOOP_LIMIT));
    }
    assert(ot_sw_decorative_inhibited(&b));
    assert(!ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS-1,100,100,false,false));
    assert(!ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS,99,100,false,false));
    assert(!ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS,100,99,false,false));
    assert(!ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS,100,100,true,false));
    assert(!ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS,100,100,false,true));
    assert(ot_sw_decorative_inhibited(&b));
    assert(ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS,100,100,false,false));
    assert(b.stable&&!ot_sw_decorative_inhibited(&b));
    assert(rtc.stable_boot==1u && rtc.crash_streak==0u);
    assert(!ot_sw_mark_stable(&rtc,&b,OT_SW_STABLE_MS,100,100,false,false));
    b=ot_sw_boot_record(&rtc,OT_SW_RESET_TASK_WDT);
    assert(b.crash_streak_at_boot==1 && !b.boot_loop_seen);
    rtc.check^=0x10u; /* random RTC/NVS corruption never replays invalid streak */
    assert(!ot_sw_rtc_valid(&rtc));
    b=ot_sw_boot_record(&rtc,OT_SW_RESET_SOFTWARE);
    assert(b.crash_streak_at_boot==0 && ot_sw_rtc_valid(&rtc));
}
static void separate_reset_causes(void){
    ot_sw_rtc_record_t rtc={0};
    const ot_sw_reset_class_t crash[]={OT_SW_RESET_PANIC,OT_SW_RESET_TASK_WDT,
        OT_SW_RESET_INTERRUPT_WDT,OT_SW_RESET_OTHER_WDT,OT_SW_RESET_CPU_LOCKUP};
    const ot_sw_reset_class_t benign[]={OT_SW_RESET_OTHER,OT_SW_RESET_POWER,
        OT_SW_RESET_SOFTWARE,OT_SW_RESET_BROWNOUT};
    for(unsigned i=0;i<sizeof(crash)/sizeof(crash[0]);i++){
        assert(ot_sw_is_crash_reset(crash[i]));
        ot_sw_boot_t b=ot_sw_boot_record(&rtc,crash[i]);
        assert(b.crash_streak_at_boot>=1);
        assert(strcmp(ot_sw_reset_name(crash[i]),"other")!=0);
    }
    for(unsigned i=0;i<sizeof(benign)/sizeof(benign[0]);i++){
        assert(!ot_sw_is_crash_reset(benign[i]));
        ot_sw_boot_t b=ot_sw_boot_record(&rtc,benign[i]);
        assert(!b.boot_loop_seen&&b.crash_streak_at_boot==0u);
    }
}
static void progress_based_liveness(void){
    /* Full 2-lane completion or surviving lane is actual driver progress.
     * A fault latch alone, a submitted-but-pending transfer, render-loop
     * iteration and idle Show/STOP state cannot forge a completed frame. */
    assert(ot_sw_render_progress_eligible(true,true));
    assert(ot_sw_render_progress_eligible(true,false));
    assert(ot_sw_render_progress_eligible(false,true));
    assert(!ot_sw_render_progress_eligible(false,false));
    assert(!ot_sw_can_mark_stable(15000,1000,0,false,false)); /* stalled RMT */
    assert(!ot_sw_can_mark_stable(60000,0,1000,false,false)); /* frozen sampler */
    assert(!ot_sw_can_mark_stable(60000,1000,1000,true,false)); /* failed lane */
    assert(!ot_sw_can_mark_stable(60000,1000,1000,false,true)); /* corrupt staged journal */
    assert(ot_sw_can_mark_stable(60000,1000,1000,false,false));
}
int main(void){
    repeated_crash_and_bootloop();
    separate_reset_causes();
    progress_based_liveness();
    puts("PASS #113 RTC crash streak, panic/brownout/OTA reset classification, 3-crash "
         "bootloop, real sampled-input + completed-RMT progress, partial O1/O2 "
         "fault handling, fail-closed NVS and stable boot; host only");
    return 0;
}
