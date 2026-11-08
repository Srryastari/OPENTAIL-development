#include "ot_single_watchdog_idf.h"
#include "ot_single_watchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

#if !CONFIG_ESP_TASK_WDT_EN || !CONFIG_ESP_TASK_WDT_INIT || !CONFIG_ESP_TASK_WDT_PANIC
#error "OpenTail requires boot-initialized panic/reset-capable ESP-IDF TWDT"
#endif

/* Hardware RTC no-init retains a short, integrity-checked reset streak across
 * warm WDT/panic resets. This NEVER writes NVS and cannot recover a dead MCU. */
static RTC_NOINIT_ATTR ot_sw_rtc_record_t rtc_record;
static ot_sw_boot_t boot;
static esp_task_wdt_user_handle_t input_user=NULL,render_user=NULL;
static bool watchdog_configured=false;
static uint32_t input_samples=0,completed_frames=0,handled_fault_cycles=0;
static uint32_t last_input_ms=0,last_frame_ms=0;
static uint32_t healthy_since_ms=0;
static bool healthy_window=false;
static unsigned fault_injected=0u;
static bool recovery_inhibit=false;
static bool stable_run=false; /* published atomically; boot policy is render-task-owned */
static const char *TAG="OT_SW_WDT";

/* Compile-time-only bench fault injection. Default is disabled and there is
 * no BLE/HTTP command that can activate it. A selected fault fires once after
 * 5s, then the normal IDF watchdog/boot-loop policy must recover the MCU.
 */
static void inject_fault(unsigned point){
#if CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT > 0
    if(CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT!=(int)point ||
       ot_sw_idf_boot_loop_seen() || clock_ms()<5000u ||
       __atomic_exchange_n(&fault_injected,1u,__ATOMIC_ACQ_REL))return;
    ESP_LOGE(TAG,"BENCH ONLY watchdog fault injection point=%u",point);
    if(point==5u){for(;;){__asm__ volatile("nop");}}
    if(point==6u){portDISABLE_INTERRUPTS();for(;;){__asm__ volatile("nop");}}
    if(point==7u)abort();
    vTaskSuspend(NULL);
#else
    (void)point;
#endif
}

static ot_sw_reset_class_t classify(esp_reset_reason_t reason){
    switch(reason){
        case ESP_RST_POWERON:case ESP_RST_PWR_GLITCH:return OT_SW_RESET_POWER;
        case ESP_RST_SW:return OT_SW_RESET_SOFTWARE;
        case ESP_RST_PANIC:return OT_SW_RESET_PANIC;
        case ESP_RST_TASK_WDT:return OT_SW_RESET_TASK_WDT;
        case ESP_RST_INT_WDT:return OT_SW_RESET_INTERRUPT_WDT;
        case ESP_RST_WDT:return OT_SW_RESET_OTHER_WDT;
        case ESP_RST_BROWNOUT:return OT_SW_RESET_BROWNOUT;
        case ESP_RST_CPU_LOCKUP:return OT_SW_RESET_CPU_LOCKUP;
        default:return OT_SW_RESET_OTHER;
    }
}
static uint32_t clock_ms(void){return (uint32_t)(esp_timer_get_time()/1000LL);}
void ot_sw_idf_boot_record(void){
    /* Capture before NVS recovery, BLE and RMT startup, while UART still logs.
     * Corrupt/uninitialized RTC is fail-safe: never trusted as a valid streak. */
    const esp_reset_reason_t reason=esp_reset_reason();
    boot=ot_sw_boot_record(&rtc_record,classify(reason));
    __atomic_store_n(&stable_run,false,__ATOMIC_RELEASE);
    healthy_since_ms=0u;healthy_window=false;fault_injected=0u;
    __atomic_store_n(&recovery_inhibit,ot_sw_decorative_inhibited(&boot),__ATOMIC_RELEASE);
    ESP_LOGW(TAG,"single ESP reset=%s (%d) crash_streak=%lu boot_loop=%u; no backup light path",
        ot_sw_reset_name(boot.reset_class),(int)reason,
        (unsigned long)boot.crash_streak_at_boot,boot.boot_loop_seen?1u:0u);
}
bool ot_sw_idf_configure(void){
    /* Do not delete/replace bootloader or interrupt WDT. Reconfigure the
     * ESP-IDF task watchdog with both CPU idle tasks retained for starvation
     * detection and a bounded panic-reset interval. */
    const esp_task_wdt_config_t settings={
        .timeout_ms=15000u,.idle_core_mask=3u,.trigger_panic=true
    };
    const esp_err_t e=esp_task_wdt_reconfigure(&settings);
    watchdog_configured=e==ESP_OK;
    if(e!=ESP_OK)ESP_LOGE(TAG,"TWDT reconfigure failed: %s",esp_err_to_name(e));
    return watchdog_configured;
}
bool ot_sw_idf_watch_inputs(void){
    if(!watchdog_configured)return false;
    const esp_err_t e=esp_task_wdt_add_user("ot-input-sampler",&input_user);
    if(e!=ESP_OK)ESP_LOGE(TAG,"Cannot register physical input watchdog: %s",esp_err_to_name(e));
    return e==ESP_OK;
}
bool ot_sw_idf_watch_render(void){
    if(!watchdog_configured)return false;
    const esp_err_t e=esp_task_wdt_add_user("ot-render-progress",&render_user);
    if(e!=ESP_OK)ESP_LOGE(TAG,"Cannot register RMT progress watchdog: %s",esp_err_to_name(e));
    return e==ESP_OK;
}
void ot_sw_idf_cancel_inputs(void){
    if(input_user){(void)esp_task_wdt_delete_user(input_user);input_user=NULL;}
}
void ot_sw_idf_cancel_render(void){
    if(render_user){(void)esp_task_wdt_delete_user(render_user);render_user=NULL;}
}
void ot_sw_idf_input_sampled(void){
    /* Called ONLY after ALL eight physical GPIOs were read/debounced and the
     * sampler published the settled and instantaneous trigger state. */
    if(!input_user)return;
    inject_fault(1u); /* Suspend after sampling, before the input-user feed. */
    __atomic_add_fetch(&input_samples,1u,__ATOMIC_RELAXED);
    __atomic_store_n(&last_input_ms,clock_ms(),__ATOMIC_RELAXED);
    (void)esp_task_wdt_reset_user(input_user);
}
void ot_sw_idf_fault_inject(unsigned point){
    if(point>=3u && point<=4u)inject_fault(point);
}
void ot_sw_idf_render_cycle(bool all_outputs_completed,bool any_output_completed,
                            bool diagnosed_fault,bool configuration_fault){
    if(!render_user){healthy_window=false;return;}
    inject_fault(2u); /* Render task stall after output work, before its feed. */
    inject_fault(5u); /* Starve this core's idle task; IDF TWDT must catch it. */
    inject_fault(6u); /* Block interrupts; IDF interrupt WDT must catch it. */
    inject_fault(7u); /* Panic/reset classification path. */
    if(!ot_sw_render_progress_eligible(all_outputs_completed,any_output_completed)){
        healthy_window=false;
        return;
    }
    /* RMT completion is the only feed: a loop repeating a latched failure
     * without any successful physical driver transfer must NOT feed TWDT.
     * Unfinished DMA/async_done never reaches here and times out. */
    const uint32_t now=clock_ms();
    if(all_outputs_completed){
        __atomic_add_fetch(&completed_frames,1u,__ATOMIC_RELAXED);
        __atomic_store_n(&last_frame_ms,now,__ATOMIC_RELAXED);
    }else{
        __atomic_add_fetch(&handled_fault_cycles,1u,__ATOMIC_RELAXED);
    }
    (void)esp_task_wdt_reset_user(render_user);

    /* "Stable" means 60 seconds of uninterrupted full two-lane completions
     * AND fresh eight-input sampling, not merely 60 seconds since boot.
     * A partial lane, stale sampler, diagnosed output fault or NVS fault
     * restarts this window without starving the surviving lane's WDT feed.
     */
    const uint32_t last_input=__atomic_load_n(&last_input_ms,__ATOMIC_ACQUIRE);
    const bool healthy=all_outputs_completed &&
        (uint32_t)(now-last_input)<100u && !diagnosed_fault && !configuration_fault;
    if(!healthy){
        healthy_window=false;
        return;
    }
    if(!healthy_window){
        healthy_since_ms=now;
        healthy_window=true;
    }
    if(!__atomic_load_n(&stable_run,__ATOMIC_ACQUIRE) &&
       ot_sw_mark_stable(&rtc_record,&boot,(uint32_t)(now-healthy_since_ms),
           __atomic_load_n(&input_samples,__ATOMIC_RELAXED),
           __atomic_load_n(&completed_frames,__ATOMIC_RELAXED),false,false)){
        __atomic_store_n(&stable_run,true,__ATOMIC_RELEASE);
        __atomic_store_n(&recovery_inhibit,false,__ATOMIC_RELEASE);
        ESP_LOGI(TAG,"one ESP has 60s continuous input + full RMT progress; boot streak cleared in RTC, no flash write");
    }
}
bool ot_sw_idf_decorative_inhibited(void){
    return __atomic_load_n(&recovery_inhibit,__ATOMIC_ACQUIRE);
}
bool ot_sw_idf_stable(void){
    return __atomic_load_n(&stable_run,__ATOMIC_ACQUIRE);
}
bool ot_sw_idf_boot_loop_seen(void){return boot.boot_loop_seen;}
void ot_sw_idf_status_json(char *out,size_t size){
    if(!out||!size)return;
    (void)snprintf(out,size,
        "{\"enabled\":%s,\"single_controller\":true,\"redundant_failover\":false,"
        "\"timeout_ms\":15000,\"reset_reason\":\"%s\",\"crash_streak\":%lu,"
        "\"boot_loop_seen\":%s,\"decorative_inhibited\":%s,\"stable\":%s,"
        "\"sampled_input_cycles\":%lu,\"completed_rmt_frames\":%lu,"
        "\"handled_output_fault_cycles\":%lu,\"last_input_progress_ms\":%lu,"
        "\"last_complete_frame_ms\":%lu}",
        watchdog_configured?"true":"false",ot_sw_reset_name(boot.reset_class),
        (unsigned long)boot.crash_streak_at_boot,
        boot.boot_loop_seen?"true":"false",
        ot_sw_idf_decorative_inhibited()?"true":"false",
        ot_sw_idf_stable()?"true":"false",
        (unsigned long)__atomic_load_n(&input_samples,__ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&completed_frames,__ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&handled_fault_cycles,__ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&last_input_ms,__ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&last_frame_ms,__ATOMIC_RELAXED));
}
