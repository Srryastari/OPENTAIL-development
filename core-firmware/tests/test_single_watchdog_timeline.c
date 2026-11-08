/* Reproducible, synthetic watchdog failure timelines using the ACTUAL pure-C
 * boot/reset policy. This never executes on MCU or asserts physical recovery.
 * JSONL for CI: every entry is clearly HOST_SIMULATED, never PASS optical. */
#include "ot_single_watchdog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void event(unsigned time_ms,const char *scenario,const char *event_name,
                  const char *expected,const char *status){
    printf("{\"evidence\":\"HOST_SIMULATED\",\"time_ms\":%u,\"scenario\":\"%s\","
           "\"event\":\"%s\",\"expected\":\"%s\",\"physical_status\":\"%s\"}\n",
           time_ms,scenario,event_name,expected,status);
}
int main(void){
    ot_sw_rtc_record_t rtc={0};
    ot_sw_boot_t boot=ot_sw_boot_record(&rtc,OT_SW_RESET_POWER);
    assert(boot.crash_streak_at_boot==0u);
    event(0,"boot","power_on","rtc_initialized_no_staged_config_change","NOT_RUN");

    /* At 10ms two actual feed paths would have progressed. From 20ms onward
     * input sampler is frozen, but renderer might still run. ONE expired user
     * correctly forces whole-chip WDT panic/reset after 15s. */
    event(10,"sampler_deadlock","last_valid_8_gpio_sample","input_watchdog_user_fed","NOT_RUN");
    event(20,"sampler_deadlock","sampler_blocked","no_more_input_feed","NOT_RUN");
    event(15020,"sampler_deadlock","input_user_timeout_15s",
          "esp_idf_twdt_panic_reset_pending","NOT_RUN");
    boot=ot_sw_boot_record(&rtc,OT_SW_RESET_TASK_WDT);
    assert(boot.crash_streak_at_boot==1u);
    event(15021,"sampler_deadlock","task_wdt_reset",
          ot_sw_reset_name(boot.reset_class),"NOT_RUN");

    /* Output submission without async_done is not progress even if output
     * driver returned ESP_OK for queueing. */
    assert(!ot_sw_render_progress_eligible(false,false));
    event(0,"rmt_hang","last_completed_frame","render_user_fed","NOT_RUN");
    event(200,"rmt_hang","refresh_async_submitted_no_done",
          "no_render_user_feed","NOT_RUN");
    event(15200,"rmt_hang","render_user_timeout",
          "twdt_resets_if_async_done_blocked","NOT_RUN");
    assert(ot_sw_render_progress_eligible(false,true));
    event(210,"single_lane_fault","gpio16_fault_gpio17_completed",
          "surviving_lane_runs_but_no_full_frame_ota_credit","NOT_RUN");

    /* Three consecutive reset reasons are needed before decorative lockout. */
    boot=ot_sw_boot_record(&rtc,OT_SW_RESET_PANIC);
    assert(boot.crash_streak_at_boot==2);
    event(0,"bootloop","second_panic_restart","crash_streak_2","NOT_RUN");
    boot=ot_sw_boot_record(&rtc,OT_SW_RESET_TASK_WDT);
    assert(boot.crash_streak_at_boot==3 && ot_sw_decorative_inhibited(&boot));
    event(0,"bootloop","third_crash_restart","decorative_inhibited_physical_priority_only","NOT_RUN");
    assert(!ot_sw_mark_stable(&rtc,&boot,59000,800,800,false,false));
    assert(!ot_sw_mark_stable(&rtc,&boot,60000,1000,1000,false,true));
    event(59000,"bootloop","unstable_runtime_or_corrupt_nvs",
          "no_stable_clearance_no_ota_confirm","NOT_RUN");
    assert(ot_sw_mark_stable(&rtc,&boot,60000,1000,1000,false,false));
    assert(!ot_sw_decorative_inhibited(&boot));
    event(60000,"bootloop","stable_input_and_both_rmt_lanes",
          "rtc_streak_cleared_after_complete_dual_lane_progress","NOT_RUN");

    boot=ot_sw_boot_record(&rtc,OT_SW_RESET_BROWNOUT);
    assert(boot.crash_streak_at_boot==0 &&
           strcmp(ot_sw_reset_name(boot.reset_class),"brownout")==0);
    event(0,"supply_brownout","reset_reason","brownout_not_task_wdt","NOT_RUN");
    event(5000,"idle_starvation","render_core_busy_loop",
          "idle_task_TWDT_panic_reset","NOT_RUN");
    event(5000,"interrupt_block","render_core_interrupts_disabled",
          "interrupt_WDT_reset","NOT_RUN");
    event(5000,"software_panic","bench_abort",
          "panic_reset_reason","NOT_RUN");
    event(0,"stable_window","partial_frame_or_stale_sampler",
          "healthy_window_restarts_until_60s_continuous","NOT_RUN");
    event(0,"power_dead","only_mcu_no_supply",
          "watchdog_cannot_produce_oem_brake_fallback","NOT_RUN");
    event(0,"ota_pending","no_stable_full_frame",
          "rollback_remains_available","NOT_RUN");
    event(100,"stop_ble_disconnect","input_and_rmt_continue",
          "watchdog_progress_independent_of_show_or_radio","NOT_RUN");
    return 0;
}
