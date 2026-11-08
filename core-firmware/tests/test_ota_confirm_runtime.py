#!/usr/bin/env python3
"""Execute the actual OTA confirmation function with a fake IDF boundary.
This verifies admission/retry policy only; bootloader rollback is separately
pinned by sdkconfig and requires target/bench validation.
"""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
header=r'''
#ifndef OT_OTA_CONFIRM_FAKE_H
#define OT_OTA_CONFIRM_FAKE_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_INVALID_ARG 2
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
#endif
'''
test=r'''
#include "fake.h"
#include <stdio.h>
#include "ot_ota_update.c"

static esp_partition_t running={ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,0x600000,"ota_0"};
static esp_ota_img_states_t state=ESP_OTA_IMG_PENDING_VERIFY;
static esp_err_t state_error=ESP_OK,mark_error=ESP_OK;
static unsigned state_reads=0,mark_calls=0;
const esp_partition_t *esp_ota_get_running_partition(void){return &running;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *out){
    (void)p;state_reads++;*out=state;return state_error;
}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void){
    mark_calls++;if(mark_error==ESP_OK)state=ESP_OTA_IMG_VALID;return mark_error;
}
const char *esp_err_to_name(esp_err_t e){(void)e;return "fake";}
void fake_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
void esp_restart(void){}
const esp_partition_t *esp_ota_get_next_update_partition(void *p){(void)p;return NULL;}
const esp_app_desc_t *esp_app_get_description(void){return NULL;}
esp_err_t esp_ota_get_partition_description(const esp_partition_t *p,esp_app_desc_t *d){(void)p;(void)d;return ESP_FAIL;}
esp_err_t esp_ota_begin(const esp_partition_t *p,size_t n,esp_ota_handle_t *h){(void)p;(void)n;(void)h;return ESP_FAIL;}
esp_err_t esp_ota_write(esp_ota_handle_t h,const void *p,size_t n){(void)h;(void)p;(void)n;return ESP_FAIL;}
esp_err_t esp_ota_end(esp_ota_handle_t h){(void)h;return ESP_FAIL;}
esp_err_t esp_ota_abort(esp_ota_handle_t h){(void)h;return ESP_FAIL;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p){(void)p;return ESP_FAIL;}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void){return ESP_FAIL;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *k,const char *v){(void)r;(void)k;(void)v;return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *k,char *v,size_t n){(void)r;(void)k;(void)v;(void)n;return ESP_FAIL;}
int httpd_req_recv(httpd_req_t *r,char *b,size_t n){(void)r;(void)b;(void)n;return -1;}
void vTaskDelay(unsigned n){(void)n;}

static void reset(void){
    running.type=ESP_PARTITION_TYPE_APP;
    running.subtype=ESP_PARTITION_SUBTYPE_APP_OTA_0;
    state=ESP_OTA_IMG_PENDING_VERIFY;state_error=ESP_OK;mark_error=ESP_OK;
    state_reads=0;mark_calls=0;
}
int main(void){
    reset();
    assert(!ot_ota_confirm_boot(false));
    assert(state_reads==0 && mark_calls==0); /* Never confirm before outputs. */

    reset();
    assert(ot_ota_confirm_boot(true));
    assert(state_reads==1 && mark_calls==1 && state==ESP_OTA_IMG_VALID);

    reset();mark_error=ESP_FAIL;
    assert(!ot_ota_confirm_boot(true));
    assert(state_reads==1 && mark_calls==1); /* Flash validation failure is not success. */

    reset();state=ESP_OTA_IMG_VALID;
    assert(ot_ota_confirm_boot(true));
    assert(state_reads==1 && mark_calls==0); /* Already-valid image is idempotent. */

    reset();state_error=ESP_FAIL;
    assert(!ot_ota_confirm_boot(true));
    assert(state_reads==1 && mark_calls==0); /* Unknown OTA state fails closed. */

    reset();running.subtype=0;state_error=ESP_FAIL;
    assert(ot_ota_confirm_boot(true));
    assert(state_reads==0 && mark_calls==0); /* Factory/non-OTA image has no rollback state. */

    puts("PASS actual CORE OTA confirmation: output gate, pending verify, failed flash, already valid, state-read failure and non-OTA slot (fake IDF; bootloader rollback not simulated)");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="ot-ota-confirm-") as tmp:
    d=Path(tmp)
    (d/"fake.h").write_text(header)
    for name in ("freertos/FreeRTOS.h","freertos/task.h","esp_err.h","esp_log.h",
                 "esp_http_server.h","esp_ota_ops.h","esp_partition.h","esp_system.h",
                 "esp_app_format.h","esp_app_desc.h","esp_image_format.h"):
        p=d/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "fake.h"\n')
    (d/"test.c").write_text(test)
    exe=d/"test"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-ffunction-sections","-Wl,--gc-sections","-I",str(d),
                    "-I",str(root/"main"),str(d/"test.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
