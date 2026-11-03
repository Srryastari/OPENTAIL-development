#include "ot_watchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

#if !CONFIG_ESP_TASK_WDT_EN || !CONFIG_ESP_INT_WDT || !CONFIG_ESP_BROWNOUT_DET
#error "Single-controller recovery requires task/interrupt watchdog and brownout protection"
#endif
static RTC_NOINIT_ATTR ot_recovery_record_t retained;
static RTC_NOINIT_ATTR uint32_t retained_phase, retained_phase_inverse;
static portMUX_TYPE recovery_lock=portMUX_INITIALIZER_UNLOCKED;
static esp_task_wdt_user_handle_t users[OT_WATCH_COUNT];
static const char *const names[OT_WATCH_COUNT]={
    "ot-startup", "ot-input-progress", "ot-config-progress", "ot-render-output-progress"
};
static ot_reset_class_t reset_class;
static esp_reset_reason_t raw_reset;
static uint32_t previous_phase, input_ms, healthy_since_ms;
static bool quarantine, healthy_window, stable;
static unsigned injected;
static uint32_t milliseconds(void){return (uint32_t)(esp_timer_get_time()/1000LL);}
static void inject(unsigned point){
    if(CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT==0 ||
       CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT!=(int)point || quarantine ||
       milliseconds()<5000u || __atomic_exchange_n(&injected,1u,__ATOMIC_ACQ_REL))return;
    ESP_LOGE("OT_RECOVERY","BENCH injection=%u t_ms=%lu",point,(unsigned long)milliseconds());
    if(point==8u)abort();
    if(point==7u){portDISABLE_INTERRUPTS();for(;;){__asm__ volatile("nop");}}
    if(point==6u){for(;;){__asm__ volatile("nop");}}
    vTaskSuspend(NULL); /* scheduler/interrupts run, but this progress user cannot feed */
}
static ot_reset_class_t classify(esp_reset_reason_t r){
    switch(r){
    case ESP_RST_POWERON:return OT_RESET_POWER;
    case ESP_RST_SW:return OT_RESET_SOFTWARE;
    case ESP_RST_TASK_WDT:return OT_RESET_TASK_WDT;
    case ESP_RST_INT_WDT:return OT_RESET_INTERRUPT_WDT;
    case ESP_RST_WDT:return OT_RESET_OTHER_WDT;
    case ESP_RST_PANIC:return OT_RESET_PANIC;
    case ESP_RST_BROWNOUT:return OT_RESET_BROWNOUT;
    default:return OT_RESET_OTHER;
    }
}
void ot_watchdog_arm(ot_watch_id_t id){
    ESP_ERROR_CHECK(esp_task_wdt_add_user(names[id],&users[id]));
}
void ot_watchdog_disarm(ot_watch_id_t id){
    ESP_ERROR_CHECK(esp_task_wdt_delete_user(users[id]));
    users[id]=NULL;
}
void ot_watchdog_progress(ot_watch_id_t id){
    if(id==OT_WATCH_INPUTS)inject(1u);
    if(id==OT_WATCH_CONFIG)inject(5u);
    /* Each owner calls ONLY after its actual work. Never from an idle/timer feeder. */
    ESP_ERROR_CHECK(esp_task_wdt_reset_user(users[id]));
    if(id==OT_WATCH_INPUTS)__atomic_store_n(&input_ms,milliseconds(),__ATOMIC_RELEASE);
}
void ot_watchdog_phase(ot_recovery_phase_t phase){
    portENTER_CRITICAL(&recovery_lock);
    /* Rollback executes on esp_timer's task while rendering may still run.
     * Preserve the explicit timeout marker until reboot, rather than letting
     * another frame erase the software-restart classification. */
    if(retained_phase!=OT_PHASE_OTA_TIMEOUT || phase==OT_PHASE_BOOT){
        retained_phase=(uint32_t)phase;
        retained_phase_inverse=~retained_phase;
    }
    portEXIT_CRITICAL(&recovery_lock);
    if(phase==OT_PHASE_RENDER){inject(2u);inject(6u);inject(7u);inject(8u);}
    if(phase==OT_PHASE_SUBMIT_O1)inject(3u);
    if(phase==OT_PHASE_COMPLETE_O1)inject(4u);
}
void ot_watchdog_init(void){
    healthy_window=false;stable=false;healthy_since_ms=0u;input_ms=0u;injected=0u;
    raw_reset=esp_reset_reason();reset_class=classify(raw_reset);
    previous_phase=UINT32_MAX;
    if(reset_class!=OT_RESET_POWER && ot_recovery_record_valid(&retained) &&
       retained_phase==~retained_phase_inverse && retained_phase<=OT_PHASE_OTA_TIMEOUT)
        previous_phase=retained_phase;
    if(raw_reset==ESP_RST_SW && previous_phase==OT_PHASE_OTA_TIMEOUT)
        reset_class=OT_RESET_OTA_FAILURE;
    ot_recovery_boot(&retained,reset_class);
    quarantine=ot_recovery_quarantined(&retained);
    ot_watchdog_phase(OT_PHASE_BOOT);
    /* Preserve ESP-IDF idle monitoring and IWDT/RTC watchdogs. Reconfigure the
     * one system TWDT, not a competing software timeout/reboot task. 10s is a
     * conservative stall bound, not a promised lighting-recovery latency. */
    const esp_task_wdt_config_t config={
        .timeout_ms=10000u,
        .idle_core_mask=(1u<<portNUM_PROCESSORS)-1u,
        .trigger_panic=true
    };
    esp_err_t e=esp_task_wdt_init(&config);
    if(e==ESP_ERR_INVALID_STATE)e=esp_task_wdt_reconfigure(&config);
    ESP_ERROR_CHECK(e);
    ot_watchdog_arm(OT_WATCH_STARTUP);
    ESP_LOGW("OT_RECOVERY","reset=%s raw=%u failed_boots=%lu previous_phase=%lu quarantine=%u (RTC only)",
        ot_recovery_reset_name(reset_class),(unsigned)raw_reset,
        (unsigned long)retained.failures,(unsigned long)previous_phase,(unsigned)quarantine);
}
bool ot_watchdog_quarantined(void){return quarantine;}
void ot_watchdog_frame(bool full_frame,bool confirmed){
    const uint32_t now=milliseconds();
    /* A streak clears only after sustained complete output, live sampler and
     * successful boot confirmation. Failed lanes can never clear a streak. */
    bool healthy=full_frame && confirmed &&
        (uint32_t)(now-__atomic_load_n(&input_ms,__ATOMIC_ACQUIRE))<100u;
    if(!healthy){healthy_window=false;return;}
    if(!healthy_window){healthy_since_ms=now;healthy_window=true;}
    if(!stable && ot_recovery_stable_due(now,healthy_since_ms)){
        portENTER_CRITICAL(&recovery_lock);
        ot_recovery_stable(&retained);
        portEXIT_CRITICAL(&recovery_lock);
        stable=true;
        ESP_LOGI("OT_RECOVERY","60s healthy output/input progress; reset streak cleared (no flash write)");
    }
}
void ot_watchdog_format_json(char *out,size_t size){
    portENTER_CRITICAL(&recovery_lock);
    const uint32_t failures=retained.failures;
    portEXIT_CRITICAL(&recovery_lock);
    snprintf(out,size,"{\"enabled\":true,\"timeout_ms\":10000,\"reset\":\"%s\",\"raw_reset\":%u,"
        "\"failed_boots\":%lu,\"previous_phase\":%lu,\"quarantined\":%s,\"storage\":\"rtc_only\"}",
        ot_recovery_reset_name(reset_class),(unsigned)raw_reset,
        (unsigned long)failures,(unsigned long)previous_phase,quarantine?"true":"false");
}
