#include "ot_recovery_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    ot_recovery_record_t r={0};
    assert(!ot_recovery_record_valid(&r));
    ot_recovery_boot(&r,OT_RESET_POWER);
    assert(ot_recovery_record_valid(&r) && r.failures==0u);
    /* Every abnormal reset class participates in bounded boot-loop recovery. */
    const ot_reset_class_t faults[]={OT_RESET_TASK_WDT,OT_RESET_INTERRUPT_WDT,
        OT_RESET_OTHER_WDT,OT_RESET_PANIC,OT_RESET_BROWNOUT,OT_RESET_OTHER,OT_RESET_OTA_FAILURE};
    for(unsigned f=0;f<sizeof(faults)/sizeof(faults[0]);f++){
        ot_recovery_boot(&r,OT_RESET_POWER);
        for(unsigned n=1;n<=3;n++){
            ot_recovery_boot(&r,faults[f]);
            assert(r.failures==n && ot_recovery_quarantined(&r)==(n==3));
            ot_recovery_boot(&r,OT_RESET_SOFTWARE);
            assert(r.failures==n); /* OTA/manual restarts cannot hide failures */
        }
        ot_recovery_boot(&r,faults[f]);assert(r.failures==3u);
        ot_recovery_stable(&r);assert(r.failures==0u);
    }
    ot_recovery_boot(&r,OT_RESET_TASK_WDT);
    r.seal^=1u;assert(!ot_recovery_record_valid(&r));
    ot_recovery_boot(&r,OT_RESET_BROWNOUT);
    assert(r.failures==1u); /* lost/corrupt RTC never fabricated as a streak */
    assert(!ot_recovery_stable_due(59999u,0u));
    assert(ot_recovery_stable_due(60000u,0u));
    assert(ot_recovery_stable_due(59900u,UINT32_MAX-99u));
    assert(strcmp(ot_recovery_reset_name(OT_RESET_TASK_WDT),"task_watchdog")==0);
    assert(strcmp(ot_recovery_reset_name(OT_RESET_INTERRUPT_WDT),"interrupt_watchdog")==0);
    assert(strcmp(ot_recovery_reset_name(OT_RESET_OTA_FAILURE),"ota_confirmation_failure")==0);
    puts("PASS: recovery policy reset classes, RTC integrity, saturation, quarantine and wrap-safe stable window (host only)");
}
