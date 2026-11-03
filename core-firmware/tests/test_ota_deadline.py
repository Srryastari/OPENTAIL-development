#!/usr/bin/env python3
"""Execute OTA boot guard/confirmation races with fake IDF; never flashes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = r'''
#ifndef OT_OTA_FAKE
#define OT_OTA_FAKE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERROR_CHECK(x) assert((x)==ESP_OK)
#define ESP_IMAGE_HEADER_MAGIC 0xe9
#define HTTPD_SOCK_ERR_TIMEOUT -2
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 16
#define ESP_PARTITION_SUBTYPE_APP_OTA_15 31
#define pdMS_TO_TICKS(x) (x)
typedef struct {int content_len;} httpd_req_t;
esp_err_t httpd_resp_set_status(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *,const char *,const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *,const char *);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *,const char *,char *,size_t);
int httpd_req_recv(httpd_req_t *,char *,size_t);
typedef struct {int type,subtype;size_t size;const char *label;} esp_partition_t;
typedef struct {char project_name[32],version[32];uint8_t app_elf_sha256[32];} esp_app_desc_t;
typedef unsigned esp_ota_handle_t;
typedef enum {ESP_OTA_IMG_UNDEFINED,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_VALID} esp_ota_img_states_t;
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(void *);
const esp_app_desc_t *esp_app_get_description(void);
esp_err_t esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
esp_err_t esp_ota_begin(const esp_partition_t *,size_t,esp_ota_handle_t *);
esp_err_t esp_ota_write(esp_ota_handle_t,const void *,size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);
void esp_restart(void);
void vTaskDelay(unsigned);
const char *esp_err_to_name(esp_err_t);
void fake_log(const char *,const char *,...);
#define ESP_LOGE(...) fake_log(__VA_ARGS__)
#define ESP_LOGW(...) fake_log(__VA_ARGS__)
typedef struct {void (*callback)(void *);const char *name;} esp_timer_create_args_t;
typedef void *esp_timer_handle_t;
esp_err_t esp_timer_create(const esp_timer_create_args_t *,esp_timer_handle_t *);
esp_err_t esp_timer_start_once(esp_timer_handle_t,uint64_t);
#endif
'''
test = r'''
#include "fake.h"
#include <setjmp.h>
#include <stdio.h>
#include "ot_ota_update.c"
static esp_partition_t partition={0,16,0x600000,"ota_0"};
static esp_ota_img_states_t image_state;
static esp_err_t read_error,confirm_error;
static bool quarantined,race;
static unsigned timers,confirm_calls,rollback_calls,restart_calls;
static uint64_t deadline;
static void (*callback)(void *);
static ot_recovery_phase_t phase;
static jmp_buf reboot;
const esp_partition_t *esp_ota_get_running_partition(void){return &partition;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *s){
    (void)p;*s=image_state;return read_error;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void){
    confirm_calls++;if(race)callback(NULL);return confirm_error;
}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void){rollback_calls++;return ESP_FAIL;}
void esp_restart(void){restart_calls++;longjmp(reboot,1);}
const char *esp_err_to_name(esp_err_t e){(void)e;return "fake";}
void fake_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
bool ot_watchdog_quarantined(void){return quarantined;}
void ot_watchdog_phase(ot_recovery_phase_t p){phase=p;}
esp_err_t esp_timer_create(const esp_timer_create_args_t *a,esp_timer_handle_t *h){
    callback=a->callback;*h=(void *)a->callback;return ESP_OK;
}
esp_err_t esp_timer_start_once(esp_timer_handle_t h,uint64_t us){
    assert(h);timers++;deadline=us;return ESP_OK;
}
static void reset(void){
    boot_confirmation_state=0;boot_guard=NULL;image_state=ESP_OTA_IMG_PENDING_VERIFY;
    read_error=ESP_OK;confirm_error=ESP_OK;quarantined=false;race=false;
    timers=confirm_calls=rollback_calls=restart_calls=0;partition.subtype=16;
}
int main(void){
    reset();ot_ota_watch_boot();assert(timers==1 && deadline==30000000u);
    assert(!ot_ota_confirm_boot(false) && confirm_calls==0);
    assert(ot_ota_confirm_boot(true));callback(NULL);assert(restart_calls==0);
    reset();ot_ota_watch_boot();confirm_error=ESP_FAIL;
    assert(!ot_ota_confirm_boot(true));assert(!ot_ota_confirm_boot(true));
    if(setjmp(reboot)==0){callback(NULL);assert(false);}
    assert(rollback_calls==1 && restart_calls==1 && phase==OT_PHASE_OTA_TIMEOUT);
    assert(!ot_ota_confirm_boot(true) && confirm_calls==2); /* expiry wins */
    reset();ot_ota_watch_boot();race=true;
    assert(ot_ota_confirm_boot(true));assert(timers==2 && deadline==1000000u);
    callback(NULL);assert(restart_calls==0); /* flash confirmation wins */
    reset();read_error=ESP_FAIL;ot_ota_watch_boot();assert(timers==1);
    read_error=ESP_OK;image_state=ESP_OTA_IMG_VALID;
    assert(ot_ota_confirm_boot(true));callback(NULL);assert(restart_calls==0);
    reset();image_state=ESP_OTA_IMG_VALID;ot_ota_watch_boot();assert(timers==0);
    reset();partition.subtype=0;ot_ota_watch_boot();assert(timers==0);
    reset();quarantined=true;ot_ota_watch_boot();assert(timers==0);
    puts("PASS: actual OTA boot guard deadline, failed validation, expiry/flash races, transient metadata error and quarantine (fake IDF; NO physical rollback proof)");
}
'''
with tempfile.TemporaryDirectory(prefix="ot-ota-guard-") as tmp:
    d = Path(tmp)
    (d / "fake.h").write_text(header)
    for name in ("freertos/FreeRTOS.h", "freertos/task.h", "esp_err.h", "esp_log.h",
                 "esp_http_server.h", "esp_ota_ops.h", "esp_partition.h", "esp_system.h",
                 "esp_app_format.h", "esp_app_desc.h", "esp_image_format.h", "esp_timer.h"):
        p = d / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('#include "fake.h"\n')
    # Avoid a nonportable function-pointer/object-pointer cast in the fake handle.
    test = test.replace('*h=(void *)a->callback;', '*h=(void *)&callback;')
    (d / "test.c").write_text(test)
    exe = d / "test"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-ffunction-sections", "-Wl,--gc-sections", "-I", str(d),
                    "-I", str(root / "main"), str(d / "test.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
