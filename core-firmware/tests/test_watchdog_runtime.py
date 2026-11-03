#!/usr/bin/env python3
"""Execute production checkpoint/RTC logic against fake IDF calls. No reset proof."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
common = r'''
#ifndef OT_FAKE_IDF
#define OT_FAKE_IDF
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define CONFIG_ESP_TASK_WDT_EN 1
#define CONFIG_ESP_INT_WDT 1
#define CONFIG_ESP_BROWNOUT_DET 1
#define CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT 0
#define RTC_NOINIT_ATTR
#define portNUM_PROCESSORS 2
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portDISABLE_INTERRUPTS() ((void)0)
void vTaskSuspend(void *task);
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERROR_CHECK(x) assert((x)==ESP_OK)
void fake_log(const char *tag,const char *fmt,...);
#define ESP_LOGW(...) fake_log(__VA_ARGS__)
#define ESP_LOGE(...) fake_log(__VA_ARGS__)
#define ESP_LOGI(...) fake_log(__VA_ARGS__)
typedef enum {ESP_RST_POWERON,ESP_RST_SW,ESP_RST_TASK_WDT,ESP_RST_INT_WDT,
              ESP_RST_WDT,ESP_RST_PANIC,ESP_RST_BROWNOUT} esp_reset_reason_t;
esp_reset_reason_t esp_reset_reason(void);
int64_t esp_timer_get_time(void);
typedef struct fake_user *esp_task_wdt_user_handle_t;
typedef struct {uint32_t timeout_ms,idle_core_mask;bool trigger_panic;} esp_task_wdt_config_t;
esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *c);
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *c);
esp_err_t esp_task_wdt_add_user(const char *name,esp_task_wdt_user_handle_t *h);
esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t h);
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t h);
#endif
'''
test = r'''
#include "fake_idf.h"
#include "ot_watchdog.h"
#include <string.h>
#include <stdio.h>
struct fake_user {unsigned feeds;bool active;};
static struct fake_user handles[OT_WATCH_COUNT];
static unsigned allocated,initializations,reconfigurations;
static int64_t now;
static esp_reset_reason_t reason=ESP_RST_POWERON;
void fake_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
void vTaskSuspend(void *task){(void)task;assert(false);}
esp_reset_reason_t esp_reset_reason(void){return reason;}
int64_t esp_timer_get_time(void){return now*1000;}
esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *c){
    assert(c->timeout_ms==10000 && c->trigger_panic && c->idle_core_mask==3);
    initializations++;return ESP_ERR_INVALID_STATE;
}
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *c){
    assert(c->trigger_panic);reconfigurations++;return ESP_OK;
}
esp_err_t esp_task_wdt_add_user(const char *name,esp_task_wdt_user_handle_t *h){
    assert(name && allocated<OT_WATCH_COUNT);*h=&handles[allocated++];
    (*h)->active=true;return ESP_OK;
}
esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t h){
    assert(h && h->active);h->active=false;return ESP_OK;
}
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t h){
    assert(h && h->active);h->feeds++;return ESP_OK;
}
static void boot(esp_reset_reason_t r){
    reason=r;now=0;allocated=0;memset(handles,0,sizeof(handles));ot_watchdog_init();
}
static void expect(const char *field){char json[320];ot_watchdog_format_json(json,sizeof(json));assert(strstr(json,field));}
int main(void){
    boot(ESP_RST_POWERON);assert(!ot_watchdog_quarantined());
    ot_watchdog_arm(OT_WATCH_INPUTS);ot_watchdog_arm(OT_WATCH_CONFIG);ot_watchdog_arm(OT_WATCH_RENDER);
    /* Sampling cannot feed startup/config/render. Idle BLE/STOP are not inputs. */
    now=10;ot_watchdog_progress(OT_WATCH_INPUTS);
    assert(handles[1].feeds==1 && handles[0].feeds==0 && handles[2].feeds==0 && handles[3].feeds==0);
    ot_watchdog_progress(OT_WATCH_CONFIG);assert(handles[2].feeds==1);
    ot_watchdog_phase(OT_PHASE_COMPLETE_O1);assert(handles[3].feeds==0);
    ot_watchdog_phase(OT_PHASE_FRAME_DONE);ot_watchdog_progress(OT_WATCH_RENDER);
    assert(handles[3].feeds==1);
    ot_watchdog_disarm(OT_WATCH_STARTUP);assert(!handles[0].active);
    /* Retained phase only describes the last boundary, not a diagnosed cause. */
    ot_watchdog_phase(OT_PHASE_COMPLETE_O2);boot(ESP_RST_TASK_WDT);
    expect("\"failed_boots\":1");expect("\"previous_phase\":6");expect("task_watchdog");
    ot_watchdog_arm(OT_WATCH_INPUTS);
    now=1;ot_watchdog_progress(OT_WATCH_INPUTS);ot_watchdog_frame(true,true);
    now=60001;ot_watchdog_frame(true,true);expect("\"failed_boots\":1"); /* stale sampler */
    ot_watchdog_progress(OT_WATCH_INPUTS);ot_watchdog_frame(true,true);
    now=120001;ot_watchdog_progress(OT_WATCH_INPUTS);ot_watchdog_frame(false,true);
    expect("\"failed_boots\":1"); /* partial output restarts healthy window */
    ot_watchdog_frame(true,false);expect("\"failed_boots\":1"); /* unconfirmed OTA */
    ot_watchdog_frame(true,true);
    now=180001;ot_watchdog_progress(OT_WATCH_INPUTS);ot_watchdog_frame(true,true);
    expect("\"failed_boots\":0");
    boot(ESP_RST_PANIC);boot(ESP_RST_INT_WDT);boot(ESP_RST_TASK_WDT);
    assert(ot_watchdog_quarantined());expect("\"quarantined\":true");
    boot(ESP_RST_SW);assert(ot_watchdog_quarantined());
    boot(ESP_RST_POWERON);assert(!ot_watchdog_quarantined());
    ot_watchdog_phase(OT_PHASE_OTA_TIMEOUT);ot_watchdog_phase(OT_PHASE_RENDER);boot(ESP_RST_SW);
    expect("ota_confirmation_failure");expect("\"failed_boots\":1");
    assert(initializations==reconfigurations);
    puts("PASS: production watchdog checkpoints, independent feed ownership, RTC phases, stable-window and quarantine (fake IDF; NO reset/lighting proof)");
}
'''
with tempfile.TemporaryDirectory(prefix="ot-watchdog-") as tmp:
    d = Path(tmp)
    (d / "fake_idf.h").write_text(common)
    for name in ("freertos/FreeRTOS.h", "freertos/task.h", "esp_attr.h", "esp_err.h",
                 "esp_log.h", "esp_system.h", "esp_task_wdt.h", "esp_timer.h"):
        p = d / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('#include "fake_idf.h"\n')
    (d / "test.c").write_text(test)
    exe = d / "watchdog-test"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-I", str(d), "-I", str(root / "main"), str(d / "test.c"),
                    str(root / "main/ot_watchdog.c"), str(root / "main/ot_recovery_policy.c"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
