/* CORE bench-only OTA receiver. Requires the 16MB two-slot partition table.
 * HTTP is intentionally local to the WPA2 bench AP, never an internet-facing
 * listener. This is not a production authenticated firmware update service.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_app_format.h"
#include "esp_image_format.h"
#include "ot_version.h"
#include "ot_ota_update.h"
static const char *TAG="OT_OTA";
static bool ota_active=false;

static esp_err_t reply(httpd_req_t *r,const char *status,const char *text) {
    httpd_resp_set_status(r,status);
    httpd_resp_set_type(r,"application/json; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_sendstr(r,text);
}
bool ot_ota_available(void) {
    const esp_partition_t *running=esp_ota_get_running_partition();
    const esp_partition_t *next=esp_ota_get_next_update_partition(NULL);
    return running && next && next!=running &&
       running->type==ESP_PARTITION_TYPE_APP &&
       running->subtype>=ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
       running->subtype<=ESP_PARTITION_SUBTYPE_APP_OTA_15;
}
esp_err_t ot_ota_status_get(httpd_req_t *r) {
    const esp_partition_t *running=esp_ota_get_running_partition();
    const esp_partition_t *next=esp_ota_get_next_update_partition(NULL);
    char body[240];
    snprintf(body,sizeof(body),
        "{\"available\":%s,\"version\":\"%s\",\"slot\":\"%s\",\"next_slot\":\"%s\",\"busy\":%s,\"local_upload_only\":true}",
        ot_ota_available()?"true":"false",OT_FW_VERSION,
        running?running->label:"none",next?next->label:"none",
        ota_active?"true":"false");
    return reply(r,"200 OK",body);
}
static bool same_origin_and_confirmed(httpd_req_t *r) {
    char origin[80]={0},host[80]={0},confirm[32]={0};
    if(httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK ||
       strcmp(origin,"http://192.168.4.1")!=0)return false;
    if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK ||
       (strcmp(host,"192.168.4.1")!=0 && strcmp(host,"192.168.4.1:80")!=0))return false;
    if(httpd_req_get_hdr_value_str(r,"X-OpenTail-Update",confirm,sizeof(confirm))!=ESP_OK ||
       strcmp(confirm,"confirmed")!=0)return false;
    char type[64]={0};
    if(httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK ||
       strcmp(type,"application/octet-stream")!=0)return false;
    return true;
}
esp_err_t ot_ota_upload_post(httpd_req_t *r) {
    if(!ot_ota_available())
        return reply(r,"409 Conflict","{\"error\":\"Two-slot OTA layout not installed; USB migration required\"}");
    if(ota_active)
        return reply(r,"409 Conflict","{\"error\":\"An OTA upload is already running\"}");
    if(!same_origin_and_confirmed(r))
        return reply(r,"403 Forbidden","{\"error\":\"Open /dev on the controller Wi-Fi and explicitly confirm the firmware update\"}");
    const esp_partition_t *update=esp_ota_get_next_update_partition(NULL);
    if(!update || r->content_len<32768 || (size_t)r->content_len>update->size)
        return reply(r,"400 Bad Request","{\"error\":\"Invalid image length for the inactive firmware slot\"}");
    uint8_t *chunk=malloc(4096);
    if(!chunk)
        return reply(r,"503 Service Unavailable","{\"error\":\"Insufficient memory for OTA buffer\"}");
    ota_active=true;
    esp_ota_handle_t handle=0;
    esp_err_t err=esp_ota_begin(update,(size_t)r->content_len,&handle);
    if(err!=ESP_OK){
        ota_active=false;free(chunk);
        return reply(r,"500 Internal Server Error","{\"error\":\"OTA erase failed; current firmware retained\"}");
    }
    size_t remaining=(size_t)r->content_len;
    int timeout_count=0;
    bool first=true;
    while(remaining){
        size_t want=remaining<4096?remaining:4096;
        int n=httpd_req_recv(r,(char *)chunk,want);
        if(n==HTTPD_SOCK_ERR_TIMEOUT && ++timeout_count<=5)continue;
        if(n<=0){err=ESP_FAIL;break;}
        timeout_count=0;
        if(first && chunk[0]!=ESP_IMAGE_HEADER_MAGIC){err=ESP_ERR_INVALID_ARG;break;}
        first=false;
        err=esp_ota_write(handle,chunk,(size_t)n);
        if(err!=ESP_OK)break;
        remaining-=(size_t)n;
    }
    free(chunk);
    if(err==ESP_OK && remaining==0){
        err=esp_ota_end(handle);
        handle=0;
    }
    if(err!=ESP_OK){
        if(handle)esp_ota_abort(handle);
        ota_active=false;
        ESP_LOGE(TAG,"OTA receive/verification failed: %s",esp_err_to_name(err));
        return reply(r,"400 Bad Request","{\"error\":\"Image upload or verification failed; existing firmware retained\"}");
    }
    esp_app_desc_t desc={0};
    err=esp_ota_get_partition_description(update,&desc);
    if(err!=ESP_OK || strcmp(desc.project_name,"opentail_core_alpha")!=0 ||
       strncmp(desc.version,OT_FW_VERSION,sizeof(desc.version))==0){
        ota_active=false;
        return reply(r,"400 Bad Request","{\"error\":\"Image is not a different OpenTail CORE build\"}");
    }
    err=esp_ota_set_boot_partition(update);
    if(err!=ESP_OK){
        ota_active=false;
        return reply(r,"500 Internal Server Error","{\"error\":\"Could not activate new firmware; original boot slot retained\"}");
    }
    ESP_LOGW(TAG,"OTA installed version %s in %s; rebooting",desc.version,update->label);
    esp_err_t sent=reply(r,"200 OK","{\"ok\":true,\"rebooting\":true,\"message\":\"Firmware verified; rebooting to inactive slot\"}");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return sent;
}
bool ot_ota_confirm_boot(bool outputs_ready) {
    if(!outputs_ready)return false;
    const esp_partition_t *running=esp_ota_get_running_partition();
    if(!running)return false;
    esp_ota_img_states_t state=ESP_OTA_IMG_UNDEFINED;
    if(esp_ota_get_state_partition(running,&state)!=ESP_OK)return false;
    if(state!=ESP_OTA_IMG_PENDING_VERIFY)return true;
    /* Keep rollback available until the RMT renderer has completed a frame
     * AND ESP-IDF actually accepts the new candidate. Retry if flash fails. */
    const esp_err_t e=esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGW(TAG,"OTA candidate boot validation: %s",esp_err_to_name(e));
    return e==ESP_OK;
}
