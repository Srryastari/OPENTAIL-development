/* OpenTail CORE Alpha — universal user-profile pixel counts, hardware-budgeted, not road firmware.
 * ESP-IDF 5.5.x, ESP32-S3, 2x WS2815-compatible 800kHz RMT outputs.
 * No watchdog, fail-safe, independent backup or OEM-bulb fallback. Do not drive on road.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ble_config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "board_config.h"
#include "ot_version.h"
#include "ot_ota_update.h"
#include "status_led.h"
#include "show_logic.h"
#include "ble_command.h"
#include "ble_fx_command.h"
#include "effects_1d.h"
#include "ot_auto_effects.h"
#include "ot_transition.h"
#include "fx_preset_record.h"
#include "fx_zone_bank.h"
#include "palette_1d.h"
#include "arbitration_logic.h"
#include "output_manager.h"
#include "trigger_manager.h"
#include "ot_config_journal.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "palette_runtime.h"
#include "esp_app_desc.h"
#include "ot_fps_pacer.h"
#include "ot_frame_contract.h"
#include "ot_frame_snapshot.h"

#define OT_FW OT_FW_VERSION
/* No hard-coded bench-derived FPS: frame rate is constrained by each output's
 * actual pixel length, transfer backend and render processing time. */
#define OT_AP_SSID "OpenTail-CORE-Alpha"
static const char *TAG="OPENTAIL";
static uint8_t palette_random_byte(void *unused){
    (void)unused;return (uint8_t)esp_random();
}
static bool palette_resolved(uint8_t id){
    if(id<73u||id>200u)return true;
    ot_palette_entry_t entry={0};
    return ot_palette_runtime_get((uint8_t)(200u-id),&entry)&&entry.present;
}
extern bool ot_ble_start(void);
extern bool ot_ble_radio_ever_ready(void);
extern void ot_ble_session_hex(char out[33]);
static const gpio_num_t input_pin[OT_IN_COUNT]=OT_INPUT_GPIO_LIST;
static const gpio_num_t out_pin[OT_CHAIN_COUNT]={OT_OUT_LEFT_GPIO,OT_OUT_RIGHT_GPIO};
static led_strip_handle_t strip[OT_CHAIN_COUNT]={0};
static uint16_t strip_capacity[OT_CHAIN_COUNT]={0}; /* Currently allocated/transmitted pixel count per lane. */
static const ot_layout_t layout={OT_LAYOUT_SECTIONS,OT_CHAIN_PIXELS,OT_MAX_BRIGHTNESS};
static TaskHandle_t render_task_handle=NULL; /* physical sampler wakeups */

static portMUX_TYPE state_lock=portMUX_INITIALIZER_UNLOCKED;
static uint8_t tm_gpio_input_mask(void);
static void request_render_refresh(void){
    TaskHandle_t task=NULL;
    portENTER_CRITICAL(&state_lock);
    task=render_task_handle;
    portEXIT_CRITICAL(&state_lock);
    if(task)xTaskNotifyGive(task);
}
/* Latches successful physical GPIO setup and sampler task creation before
 * BLE/HTTP configuration endpoints start accepting durable mapping writes. */
static bool input_sampler_ready=false;
static uint8_t live_input_mask=0;
static uint8_t sim_mask=0;
static TickType_t sim_deadline=0;
static ot_demo_t demo={OT_DEMO_OFF,{0,0,0},0};
static TickType_t demo_deadline=0;
static uint32_t frame_index=0;
static uint32_t measured_fps=0;
static uint16_t adaptive_target_fps=OT_FPS_PREFERRED_HIGH;
static uint32_t frame_window_start_ms=0,frame_window_frames=0;
/* Software-side evidence only: successful driver refresh != observed pixel state. */
static uint32_t output_refreshes[OT_CHAIN_COUNT]={0};
/* Render iterations that did not complete every configured lane. */
static uint32_t incomplete_frames=0;
static uint32_t output_runtime_fault_mask=0; /* Latched physical RMT lanes; bench diagnostics. */
static uint32_t stop_request_generation=0,stop_render_generation=0;
/* Single render-task owner of physical outputs; HTTP handlers only modify copied state. */
static ot_show_t native_show={0};
static TickType_t native_show_deadline=0;
static uint32_t native_show_started_ms=0;
static bool native_outputs_ready=false;
static bool ble_startup_ready=false; /* Set once before render task creation. */
static bool http_startup_ready=false; /* Diagnostics are required to confirm a new OTA image. */
static bool ble_owns_show=false;
/* The only FX library is internal to CORE. BLE writes effect IDs, not pixel frames. */
static bool ble_fx_enabled=false;
/* Default remains instant until adjustable transition settings are exposed. */
#define OT_FX_ENTRY_FADE_MS 0u
static ot_fx_zone_bank_t fx_bank={0}; /* Independent CORE output/zone effect assignments */
static ot_ble_fx_packet_t ble_fx_request={0};

static ot_om_output_t om_outputs[OT_OM_OUTPUTS];
static ot_om_pending_t om_deferred; /* Persisted zones await release. */
static bool om_persistent=false;
static SemaphoreHandle_t om_write_mutex=NULL;
static uint16_t om_revision[OT_OM_OUTPUTS];
/* Trigger profile is stored separately from v0.1.17 Output Manager NVS. */
static ot_tm_profile_t tm_profile;
static ot_tm_pending_t tm_deferred; /* Persisted mapping awaits release. */
static bool tm_persistent=false;
/* Invalid newer journal records must fail closed, not silently migrate stale NVS. */
static bool config_recovery_fault=false;
static uint16_t tm_revision=1u;
typedef struct {
    uint32_t magic, version;
    ot_tm_profile_t profile;
} ot_tm_store_t;
static void tm_load(void){
    ot_tm_defaults(&tm_profile);
    tm_persistent=false;
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READONLY,&h)!=ESP_OK)return;
    size_t size=0;
    esp_err_t e=nvs_get_blob(h,"pair_v1",NULL,&size);
    if(e!=ESP_ERR_NVS_NOT_FOUND){
        ot_cfg_tm_journal_t pair={0};
        ot_tm_profile_t requested={0};
        bool valid=e==ESP_OK&&size==sizeof(pair);
        if(valid)valid=nvs_get_blob(h,"pair_v1",&pair,&size)==ESP_OK &&
                       ot_cfg_tm_unpack(&pair,&tm_profile,&requested);
        nvs_close(h);
        if(valid){
            tm_deferred.profile=requested;
            tm_deferred.pending=memcmp(&tm_profile,&requested,sizeof(requested))!=0;
            tm_persistent=true;
            return;
        }
        /* An invalid newer record may represent a torn or corrupt write.
         * Never activate a stale legacy profile or arbitrary default mapping. */
        memset(&tm_profile,0,sizeof(tm_profile));
        config_recovery_fault=true;
        ESP_LOGE(TAG,"Invalid durable trigger recovery journal; all trigger routes disabled");
        return;
    }
    /* Legacy migration is read-only. The first new save writes the paired
     * journal atomically, preserving the legacy record as a fallback artifact. */
    ot_tm_store_t stored={0};size=sizeof(stored);
    e=nvs_get_blob(h,"profile_v1",&stored,&size);
    nvs_close(h);
    tm_persistent=e==ESP_OK && size==sizeof(stored) &&
        stored.magic==OT_TM_MAGIC && stored.version==OT_TM_VERSION && ot_tm_valid(&stored.profile);
    if(tm_persistent)tm_profile=stored.profile;
    else ESP_LOGI(TAG,"No valid trigger profile; preserving original IN1..IN8 defaults");
}
static bool tm_save(const ot_tm_profile_t *p){
    ot_cfg_tm_journal_t pair={0};
    if(!ot_cfg_tm_pack(&tm_profile,p,&pair))return false;
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,"pair_v1",&pair,sizeof(pair));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}
#define OT_OM_STORAGE_MAGIC 0x4f543135u
typedef struct {
    uint32_t magic;
    uint32_t version;
    ot_om_output_t outputs[OT_OM_OUTPUTS];
} ot_om_store_t;
/* Exact original v0.1.15/16 NVS shape: preserve user-tested custom color order and zones. */
typedef struct {
    char name[OT_OM_NAME_LEN];uint16_t pixels;uint8_t role;
} ot_om_zone_v1_t;
typedef struct {
    char name[OT_OM_NAME_LEN];uint16_t active_pixels;uint8_t color_order;
    ot_om_zone_v1_t zones[5];
} ot_om_output_v1_t;
typedef struct {
    uint32_t magic,version;ot_om_output_v1_t outputs[OT_OM_OUTPUTS];
} ot_om_store_v1_t;
_Static_assert(sizeof(ot_om_output_v1_t)==sizeof(char[25])+3+150+2,"v1 NVS ABI changed");
static void om_load(void){
    ot_om_defaults(om_outputs);
    om_persistent=false;
    nvs_handle_t h;
    if(nvs_open("ot_output_cfg",NVS_READONLY,&h)!=ESP_OK)return;
    size_t pair_size=0;
    esp_err_t pair_error=nvs_get_blob(h,"pair_v1",NULL,&pair_size);
    if(pair_error!=ESP_ERR_NVS_NOT_FOUND){
        ot_cfg_om_journal_t pair={0};
        ot_om_output_t requested[OT_OM_OUTPUTS]={0};
        bool valid=pair_error==ESP_OK&&pair_size==sizeof(pair);
        if(valid)valid=nvs_get_blob(h,"pair_v1",&pair,&pair_size)==ESP_OK &&
                       ot_cfg_om_unpack(&pair,om_outputs,requested);
        nvs_close(h);
        if(valid){
            memcpy(om_deferred.outputs,requested,sizeof(requested));
            om_deferred.pending=memcmp(om_outputs,requested,sizeof(requested))!=0;
            om_persistent=true;
            return;
        }
        ot_om_defaults(om_outputs); /* Explicitly inert instead of stale legacy. */
        config_recovery_fault=true;
        ESP_LOGE(TAG,"Invalid durable output recovery journal; output roles disabled");
        return;
    }
    /* A corrupt newer record must not mask the older, previously tested
     * profile. Never erase or rewrite either key during boot migration. */
    const char *const keys[]={"profile_v2","profile"};
    for(unsigned attempt=0;attempt<sizeof(keys)/sizeof(keys[0])&&!om_persistent;attempt++){
        const char *key=keys[attempt];
        size_t n=0;
        esp_err_t e=nvs_get_blob(h,key,NULL,&n);
        if(e!=ESP_OK)continue;
        if(n==sizeof(ot_om_store_t)){
            ot_om_store_t stored={0};
            e=nvs_get_blob(h,key,&stored,&n);
            bool valid=e==ESP_OK&&stored.magic==OT_OM_STORAGE_MAGIC&&
                       stored.version==OT_OM_VERSION;
            for(unsigned i=0;valid&&i<OT_OM_OUTPUTS;i++)
                valid=ot_om_valid(&stored.outputs[i]);
            if(valid){
                memcpy(om_outputs,stored.outputs,sizeof(om_outputs));
                om_persistent=true;
            }
        }else if(n==sizeof(ot_om_store_v1_t)){
            ot_om_store_v1_t old={0};
            e=nvs_get_blob(h,key,&old,&n);
            if(e==ESP_OK&&old.magic==OT_OM_STORAGE_MAGIC&&old.version==1){
                bool ok=true;
                for(int i=0;i<OT_OM_OUTPUTS;i++){
                    ot_om_output_t *dst=&om_outputs[i];const ot_om_output_v1_t *src=&old.outputs[i];
                    memcpy(dst->name,src->name,OT_OM_NAME_LEN);
                    dst->active_pixels=src->active_pixels;dst->color_order=src->color_order;
                    dst->zone_count=5;
                    for(int j=0;j<5;j++){
                        memcpy(dst->zones[j].name,src->zones[j].name,OT_OM_NAME_LEN);
                        dst->zones[j].pixels=src->zones[j].pixels;dst->zones[j].role=src->zones[j].role;
                    }
                    for(int k=0;k<3;k++){dst->rgb_gain[k]=255;dst->white_gain[k]=255;}
                    dst->gamma_enabled=false;
                    if(!ot_om_valid(dst))ok=false;
                }
                if(ok){
                    om_persistent=true;
                    ESP_LOGI(TAG,"Migrated v1 output mapping into RAM; will upgrade NVS schema on next save");
                }
            }
        }
        if(!om_persistent)
            ESP_LOGW(TAG,"Stored output mapping %s unavailable or invalid; checking prior profile",key);
    }
    nvs_close(h);
    if(!om_persistent){
        ot_om_defaults(om_outputs);
        ESP_LOGW(TAG,"No valid output profile; restored neutral unconfigured mapping");
    }
}
static bool om_save(const ot_om_output_t items[OT_OM_OUTPUTS]){
    ot_cfg_om_journal_t pair={0};
    if(!ot_cfg_om_pack(om_outputs,items,&pair))return false;
    nvs_handle_t h;
    if(nvs_open("ot_output_cfg",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,"pair_v1",&pair,sizeof(pair));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}

#define OT_SHOW_MAX_MS 60000u
/* Input simulation is limited to 15 seconds, independent of show TTL. */
#define OT_SIM_MAX_MS 15000u


static bool before_deadline(TickType_t now,TickType_t deadline) {
    return (int32_t)(deadline-now)>0;
}
static unsigned bounded_int(const cJSON *o,const char *key,int def,int low,int high) {
    cJSON *value=cJSON_GetObjectItemCaseSensitive((cJSON *)o,key);
    if(!cJSON_IsNumber(value)) return (unsigned)def;
    int n=value->valueint;
    return (unsigned)(n<low?low:n>high?high:n);
}
static int recv_json(httpd_req_t *req,cJSON **result) {
    if(req->content_len<2 || req->content_len>4096) return 400;
    char *buf=malloc((size_t)req->content_len+1u);
    if(!buf)return 400;
    int len=0;
    while(len<req->content_len) {
        int n=httpd_req_recv(req,buf+len,req->content_len-len);
        if(n<=0){free(buf);return 408;}
        len+=n;
    }
    buf[len]=0;
    *result=cJSON_ParseWithLength(buf,(size_t)len);
    free(buf);
    return cJSON_IsObject(*result)?200:400;
}
static esp_err_t send_json(httpd_req_t *r,const char *body) {
    httpd_resp_set_type(r,"application/json; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_sendstr(r,body);
}
static esp_err_t respond_err(httpd_req_t *r,int status,const char *reason) {
    httpd_resp_set_status(r,status==500?"500 Internal Server Error":status==409?"409 Conflict":status==408?"408 Request Timeout":"400 Bad Request");
    char buf[120];snprintf(buf,sizeof(buf),"{\"error\":\"%s\"}",reason);
    return send_json(r,buf);
}
static bool show_window(TickType_t now){
    if(ble_fx_enabled)return ot_fx_bank_any_active(&fx_bank,(uint32_t)(esp_timer_get_time()/1000LL));
    return native_show.mode!=OT_SHOW_OFF && before_deadline(now,native_show_deadline);
}
static bool om_number(const cJSON *obj,const char *key,unsigned *dest,unsigned low,unsigned high){
    const cJSON *v=cJSON_GetObjectItemCaseSensitive((cJSON *)obj,key);
    if(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint||v->valueint<(int)low||v->valueint>(int)high)return false;
    *dest=(unsigned)v->valueint;return true;
}
static bool om_string(const cJSON *obj,const char *key,char dst[OT_OM_NAME_LEN]){
    const cJSON *v=cJSON_GetObjectItemCaseSensitive((cJSON *)obj,key);
    if(!cJSON_IsString(v)||!v->valuestring)return false;
    size_t n=strlen(v->valuestring);
    if(n<1||n>=OT_OM_NAME_LEN)return false;
    memcpy(dst,v->valuestring,n+1);
    return true;
}
static cJSON *om_json_output(const ot_om_output_t *o,int id,uint8_t mask,uint8_t show_zones){
    cJSON *item=cJSON_CreateObject();if(!item)return NULL;
    if(!cJSON_AddNumberToObject(item,"index",id) ||
       !cJSON_AddNumberToObject(item,"pin",(int)out_pin[id]) ||
       !cJSON_AddStringToObject(item,"name",o->name) ||
       !cJSON_AddNumberToObject(item,"pixels",o->active_pixels) ||
       !cJSON_AddNumberToObject(item,"zone_count",o->zone_count))goto oom;
    cJSON *cal=cJSON_AddObjectToObject(item,"calibration");
    if(!cal)goto oom;
    cJSON *rgb=cJSON_AddArrayToObject(cal,"rgb_gain");
    cJSON *w=cJSON_AddArrayToObject(cal,"white_gain");
    if(!rgb||!w)goto oom;
    for(int k=0;k<3;k++){
        cJSON *r=cJSON_CreateNumber(o->rgb_gain[k]);
        cJSON *white=cJSON_CreateNumber(o->white_gain[k]);
        if(!r||!white){cJSON_Delete(r);cJSON_Delete(white);goto oom;}
        (void)cJSON_AddItemToArray(rgb,r);(void)cJSON_AddItemToArray(w,white);
    }
    if(!cJSON_AddBoolToObject(cal,"gamma_enabled",o->gamma_enabled) ||
       !cJSON_AddStringToObject(item,"color_order",ot_om_order_name(o->color_order)))goto oom;
    cJSON *zones=cJSON_AddArrayToObject(item,"zones");
    if(!zones)goto oom;
    for(int i=0;i<o->zone_count;i++){
        const ot_om_zone_t *z=&o->zones[i];
        cJSON *entry=cJSON_CreateObject();
        if(!entry)goto oom;
        const bool show_here=(show_zones&(1u<<i))!=0;
        ot_rgb_t overlay={0,0,0};
        bool trigger_here=ot_om_trigger_overlay(z->role,mask,show_here,
                                                 OT_MAX_BRIGHTNESS,&overlay);
        if(!cJSON_AddStringToObject(entry,"name",z->name) ||
           !cJSON_AddNumberToObject(entry,"pixels",z->pixels) ||
           !cJSON_AddStringToObject(entry,"role",ot_om_role_name(z->role)) ||
           !cJSON_AddBoolToObject(entry,"trigger_active",trigger_here) ||
           !cJSON_AddBoolToObject(entry,"effect_selected",show_here)){
            cJSON_Delete(entry);goto oom;
        }
        (void)cJSON_AddItemToArray(zones,entry);
    }
    return item;
oom:
    cJSON_Delete(item);
    return NULL;
}
static esp_err_t om_get(httpd_req_t *r){
    ot_om_output_t copy[OT_OM_OUTPUTS];ot_tm_profile_t tm;bool persistent,config_pending;uint8_t mask;
    uint8_t show_zones[OT_OM_OUTPUTS]={0};
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    memcpy(copy,om_outputs,sizeof(copy));config_pending=om_deferred.pending;
    persistent=om_persistent&&!config_pending;tm=tm_profile;
    mask=ot_tm_resolve(&tm,live_input_mask)|(before_deadline(now,sim_deadline)?sim_mask:0);
    if(show_window(now)){
        if(ble_fx_enabled){
            uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
            for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<copy[o].zone_count;z++)
                if(ot_fx_slot_active(&fx_bank.zones[o][z],ms))
                    show_zones[o]=(uint8_t)(show_zones[o]|(1u<<z));
        }else{
            for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
                if(native_show.chain_mask&(1u<<o))
                    show_zones[o]=(uint8_t)native_show.section_mask;
        }
    }
    portEXIT_CRITICAL(&state_lock);
    cJSON *obj=cJSON_CreateObject();
    if(!obj)return respond_err(r,500,"out of memory");
    if(!cJSON_AddNumberToObject(obj,"revision",OT_OM_VERSION) ||
       !cJSON_AddBoolToObject(obj,"saved_on_device",persistent) ||
       !cJSON_AddBoolToObject(obj,"config_apply_pending",config_pending) ||
       !cJSON_AddBoolToObject(obj,"bench_only",true) ||
       !cJSON_AddNumberToObject(obj,"physical_outputs",OT_CHAIN_COUNT) ||
       !cJSON_AddNumberToObject(obj,"future_outputs_target",12) ||
       /* Hardware build budget, not an active pixel count. */
       !cJSON_AddNumberToObject(obj,"pixel_buffer_budget_per_output",OT_CHAIN_PIXELS) ||
       !cJSON_AddNumberToObject(obj,"max_configurable_pixels_per_output",OT_OM_MAX_PIXELS_PER_OUTPUT) ||
       !cJSON_AddNumberToObject(obj,"max_total_pixels",OT_OM_MAX_TOTAL_PIXELS) ||
       !cJSON_AddNumberToObject(obj,"max_zones_per_output",OT_OM_ZONES) ||
       !cJSON_AddBoolToObject(obj,"dynamic_zones",true) ||
       !cJSON_AddNumberToObject(obj,"effective_mask",mask)){
        cJSON_Delete(obj);return respond_err(r,500,"out of memory");
    }
    cJSON *outputs=cJSON_AddArrayToObject(obj,"outputs");
    if(!outputs){cJSON_Delete(obj);return respond_err(r,500,"out of memory");}
    for(int i=0;i<OT_OM_OUTPUTS;i++){
        cJSON *entry=om_json_output(&copy[i],i,mask,show_zones[i]);
        if(!entry){cJSON_Delete(obj);return respond_err(r,500,"out of memory");}
        (void)cJSON_AddItemToArray(outputs,entry);
    }
    char *payload=cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if(!payload)return respond_err(r,500,"out of memory");
    esp_err_t result=send_json(r,payload);free(payload);return result;
}
static esp_err_t om_post(httpd_req_t *r){
    cJSON *obj=NULL;int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    unsigned index=0,pixels=0;
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,"color_order");
    const char *order=cJSON_IsString(v)?v->valuestring:NULL;
    int order_id=ot_om_order_from_name(order);
    cJSON *zones=cJSON_GetObjectItemCaseSensitive(obj,"zones");
    ot_om_output_t next={0};
    bool valid=om_number(obj,"index",&index,0,OT_OM_OUTPUTS-1)&&
      om_number(obj,"pixels",&pixels,1,OT_CHAIN_PIXELS)&&
      om_string(obj,"name",next.name)&&order_id>=0&&cJSON_IsArray(zones)&&
      cJSON_GetArraySize(zones)>=1&&cJSON_GetArraySize(zones)<=OT_OM_ZONES;
    if(valid){
        next.active_pixels=(uint16_t)pixels;next.color_order=(uint8_t)order_id;
        next.zone_count=(uint8_t)cJSON_GetArraySize(zones);
        for(int k=0;k<3;k++){next.rgb_gain[k]=255;next.white_gain[k]=255;}
        const cJSON *cal=cJSON_GetObjectItemCaseSensitive(obj,"calibration");
        if(cal){
            const cJSON *rgb=cJSON_GetObjectItemCaseSensitive((cJSON*)cal,"rgb_gain");
            const cJSON *white=cJSON_GetObjectItemCaseSensitive((cJSON*)cal,"white_gain");
            const cJSON *gamma=cJSON_GetObjectItemCaseSensitive((cJSON*)cal,"gamma_enabled");
            if(!cJSON_IsObject(cal)||!cJSON_IsArray(rgb)||!cJSON_IsArray(white)||
                cJSON_GetArraySize(rgb)!=3||cJSON_GetArraySize(white)!=3||!cJSON_IsBool(gamma))valid=false;
            if(valid){
                next.gamma_enabled=cJSON_IsTrue(gamma);
                for(int k=0;k<3;k++){
                    const cJSON *a=cJSON_GetArrayItem(rgb,k),*b=cJSON_GetArrayItem(white,k);
                    if(!cJSON_IsNumber(a)||!cJSON_IsNumber(b)||a->valuedouble!=a->valueint||b->valuedouble!=b->valueint||
                       a->valueint<0||a->valueint>255||b->valueint<0||b->valueint>255){valid=false;break;}
                    next.rgb_gain[k]=a->valueint;next.white_gain[k]=b->valueint;
                }
            }
        }
        for(int i=0;valid&&i<next.zone_count;i++){
            const cJSON *item=cJSON_GetArrayItem(zones,i);
            unsigned n=0;v=cJSON_GetObjectItemCaseSensitive((cJSON*)item,"role");
            int role=ot_om_role_from_name(cJSON_IsString(v)?v->valuestring:NULL);
            if(!cJSON_IsObject(item)||!om_string(item,"name",next.zones[i].name)||
                !om_number(item,"pixels",&n,0,OT_CHAIN_PIXELS)||role<0){valid=false;break;}
            next.zones[i].pixels=(uint16_t)n;next.zones[i].role=(uint8_t)role;
        }
    }
    if(valid)valid=ot_om_valid(&next);
    cJSON_Delete(obj);
    if(!valid)return respond_err(r,400,"invalid output mapping: zone lengths must total the configured active pixels and fit the board buffer budget");
    if(!input_sampler_ready)
        return respond_err(r,409,"physical input sampler unavailable");
    if(!om_write_mutex || xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)
        return respond_err(r,409,"output mapping save busy");
    /* A freshly asserted optocoupler pin is busy even before debounce. */
    const bool raw_active=tm_gpio_input_mask()!=0;
    TickType_t now=xTaskGetTickCount();
    ot_om_output_t next_all[OT_OM_OUTPUTS];
    portENTER_CRITICAL(&state_lock);
    bool busy=om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask || (before_deadline(now,sim_deadline)&&sim_mask) ||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) || show_window(now);
    memcpy(next_all,om_outputs,sizeof(next_all));
    portEXIT_CRITICAL(&state_lock);
    if(busy){xSemaphoreGive(om_write_mutex);return respond_err(r,409,"stop shows and release all 12V triggers before saving output mapping");}
    next_all[index]=next;
    /* No in-memory mutation if NVS commit fails. */
    if(!om_save(next_all)){xSemaphoreGive(om_write_mutex);return respond_err(r,500,"failed to save output mapping in flash");}
    portENTER_CRITICAL(&state_lock);
    (void)ot_om_stage(&om_deferred,next_all);
    om_persistent=true;om_revision[index]++;
    portEXIT_CRITICAL(&state_lock);
    xSemaphoreGive(om_write_mutex);
    return om_get(r);
}
/* BLE config uses EXACTLY the same output configuration, validator, and NVS key as Wi-Fi.
 * Revision is session-local and prevents a stale draft overwriting recent Wi-Fi edits.
 * Reads and commits run on NimBLE host task, so never hold state_lock during NVS I/O.
 * Bench-only: STOP all shows and release physical inputs before saving.
 */
bool ot_ble_config_get(uint8_t index,ot_om_output_t *out,uint16_t *revision){
    if(index>=OT_OM_OUTPUTS||!out||!revision)return false;
    portENTER_CRITICAL(&state_lock);
    *out=om_outputs[index];*revision=om_revision[index];
    portEXIT_CRITICAL(&state_lock);
    return true;
}
int ot_ble_config_save(uint8_t index,const ot_om_output_t *next,uint16_t base_revision){
    if(index>=OT_OM_OUTPUTS||!next||!ot_om_valid(next))return 1; /* invalid */
    if(!input_sampler_ready)return 2; /* Cannot verify physical inputs. */
    if(!om_write_mutex || xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)return 2; /* busy */
    /* Apply the same physical-input veto as HTTP and trigger-profile edits. */
    const bool raw_active=tm_gpio_input_mask()!=0;
    TickType_t now=xTaskGetTickCount();ot_om_output_t next_all[OT_OM_OUTPUTS];
    portENTER_CRITICAL(&state_lock);
    bool busy=om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask || (before_deadline(now,sim_deadline)&&sim_mask) ||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) || show_window(now);
    bool stale=om_revision[index]!=base_revision;
    memcpy(next_all,om_outputs,sizeof(next_all));
    portEXIT_CRITICAL(&state_lock);
    int result=busy?2:stale?3:0;
    if(!result){
        next_all[index]=*next;
        if(!om_save(next_all))result=4;
        else{
            portENTER_CRITICAL(&state_lock);
            (void)ot_om_stage(&om_deferred,next_all);
            om_persistent=true;om_revision[index]++;
            portEXIT_CRITICAL(&state_lock);
        }
    }
    xSemaphoreGive(om_write_mutex);
    return result;
}

/* Normalized instantaneous GPIO levels (NOT debounced). Used only for
 * diagnostic readback and the pre-save busy gate; the independent sampler
 * still owns the debounced physical mask used for rendering. */
static uint8_t tm_gpio_input_mask(void){
    uint8_t mask=0;
    for(unsigned i=0;i<OT_IN_COUNT;i++){
        const bool high=gpio_get_level(input_pin[i])!=0;
        if(OT_INPUT_ACTIVE_LOW?!high:high)mask|=(uint8_t)(1u<<i);
    }
    return mask;
}
/* Serialize authenticated BLE and bench-HTTP trigger edits with the existing
 * configuration mutex. Commit to NVS before exposing new logical routing.
 * Codes: 0 OK, 1 invalid, 2 busy/active inputs, 3 stale, 4 NVS failure.
 * Physical input sampling never takes this mutex and remains BLE-independent. */
static int tm_commit(const ot_tm_profile_t *next,uint16_t expected,bool require_revision){
    if(!ot_tm_valid(next))return 1;
    if(!input_sampler_ready)return 2; /* Cannot verify physical inputs. */
    if(!om_write_mutex ||
       xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)return 2;
    /* The debounced mask alone has a nominal 40ms assertion blind spot.
     * Reject an already asserted optocoupler pin before any NVS write. */
    const bool raw_active=tm_gpio_input_mask()!=0;
    const TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    const bool busy=om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask!=0 ||
        (before_deadline(now,sim_deadline)&&sim_mask) ||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) ||
        show_window(now);
    const bool stale=require_revision&&tm_revision!=expected;
    portEXIT_CRITICAL(&state_lock);
    int code=busy?2:stale?3:0;
    if(!code){
        if(!tm_save(next))code=4;
        else {
            /* Never activate in a storage writer, even if pins appear idle;
             * the sampler requires a stable physical release first. */
            portENTER_CRITICAL(&state_lock);
            (void)ot_tm_stage(&tm_deferred,next);
            tm_persistent=true;
            tm_revision++;
            portEXIT_CRITICAL(&state_lock);
        }
    }
    xSemaphoreGive(om_write_mutex);
    return code;
}
bool ot_ble_trigger_get(ot_tm_profile_t *out,uint16_t *revision,
                        uint8_t *raw,uint8_t *effective,bool *persistent){
    if(!out||!revision||!raw||!effective||!persistent)return false;
    const TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    *out=tm_profile;
    *revision=tm_revision;
    *raw=live_input_mask;
    *effective=(uint8_t)(ot_tm_resolve(out,*raw)|
        (before_deadline(now,sim_deadline)?sim_mask:0u));
    *persistent=tm_persistent && !tm_deferred.pending;
    portEXIT_CRITICAL(&state_lock);
    return true;
}
int ot_ble_trigger_save(const ot_tm_profile_t *next,uint16_t revision){
    return tm_commit(next,revision,true);
}

/* Universal Trigger Manager v1: independent physical-input-to-function profile.
 * Simulated signals remain logical functions, not raw GPIO assignments.
 */
static esp_err_t tm_get(httpd_req_t *r){
    ot_tm_profile_t cfg;bool saved,pending;uint8_t physical,sim;uint16_t config_revision;
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    cfg=tm_profile;pending=tm_deferred.pending;saved=tm_persistent&&!pending;
    physical=live_input_mask;config_revision=tm_revision;
    sim=before_deadline(now,sim_deadline)?sim_mask:0;
    portEXIT_CRITICAL(&state_lock);
    cJSON *root=cJSON_CreateObject();if(!root)return respond_err(r,500,"out of memory");
    /* An incomplete trigger map must never be acknowledged as HTTP 200. */
    if(!cJSON_AddNumberToObject(root,"revision",OT_TM_VERSION) ||
       !cJSON_AddNumberToObject(root,"config_revision",config_revision) ||
       !cJSON_AddBoolToObject(root,"saved_on_device",saved) ||
       !cJSON_AddBoolToObject(root,"config_apply_pending",pending) ||
       !cJSON_AddBoolToObject(root,"bench_only",true) ||
       !cJSON_AddNumberToObject(root,"physical_inputs",OT_IN_COUNT) ||
       !cJSON_AddBoolToObject(root,"fixed_active_low",OT_INPUT_ACTIVE_LOW!=0) ||
       /* Backward compatibility: raw_input_mask means DEBOUNCED physical
        * inputs. Normalized instantaneous GPIO levels are separate. */
       !cJSON_AddNumberToObject(root,"raw_input_mask",physical) ||
       !cJSON_AddNumberToObject(root,"gpio_input_mask",tm_gpio_input_mask()) ||
       !cJSON_AddNumberToObject(root,"physical_function_mask",ot_tm_resolve(&cfg,physical)) ||
       !cJSON_AddNumberToObject(root,"effective_mask",ot_tm_resolve(&cfg,physical)|sim))
        goto oom;
    cJSON *supported=cJSON_AddArrayToObject(root,"supported_functions");
    if(!supported)goto oom;
    for(int k=0;k<=OT_TM_AUX3;k++){
        cJSON *name=cJSON_CreateString(ot_tm_name(k));
        if(!name)goto oom;
        if(!cJSON_AddItemToArray(supported,name)){cJSON_Delete(name);goto oom;}
    }
    cJSON *arr=cJSON_AddArrayToObject(root,"inputs");
    if(!arr)goto oom;
    for(int i=0;i<OT_IN_COUNT;i++){
        cJSON *v=cJSON_CreateObject();
        if(!v)goto oom;
        if(!cJSON_AddNumberToObject(v,"index",i+1) ||
           !cJSON_AddNumberToObject(v,"gpio",(int)input_pin[i]) ||
           !cJSON_AddStringToObject(v,"function",ot_tm_name(cfg.function[i])) ||
           !cJSON_AddBoolToObject(v,"active",(physical&(1u<<i))!=0)){
            cJSON_Delete(v);goto oom;
        }
        if(!cJSON_AddItemToArray(arr,v)){cJSON_Delete(v);goto oom;}
    }
    char *body=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if(!body)return respond_err(r,500,"out of memory");
    esp_err_t ret=send_json(r,body);free(body);return ret;
oom:
    cJSON_Delete(root);
    return respond_err(r,500,"out of memory");
}
static esp_err_t tm_post(httpd_req_t *r){
    cJSON *root=NULL;int status=recv_json(r,&root);
    if(status!=200){cJSON_Delete(root);return respond_err(r,status,"invalid JSON request");}
    unsigned revision=0,expected_config_revision=0;
    const cJSON *expected=cJSON_GetObjectItemCaseSensitive(root,"expected_config_revision");
    const bool require_revision=expected!=NULL;
    const cJSON *arr=cJSON_GetObjectItemCaseSensitive(root,"inputs");
    bool valid=(!require_revision || om_number(root,"expected_config_revision",
                    &expected_config_revision,0,UINT16_MAX)) &&
        om_number(root,"revision",&revision,OT_TM_VERSION,OT_TM_VERSION) &&
        cJSON_IsArray(arr) && cJSON_GetArraySize(arr)==OT_IN_COUNT;
    ot_tm_profile_t next={0};
    for(int i=0;valid && i<OT_IN_COUNT;i++){
        const cJSON *item=cJSON_GetArrayItem(arr,i);
        unsigned index=0;
        const cJSON *f=cJSON_GetObjectItemCaseSensitive((cJSON *)item,"function");
        int function=ot_tm_from_name(cJSON_IsString(f)?f->valuestring:NULL);
        if(!cJSON_IsObject(item) || !om_number(item,"index",&index,i+1,i+1)||function<0){valid=false;break;}
        next.function[i]=(uint8_t)function;
    }
    cJSON_Delete(root);
    if(!valid||!ot_tm_valid(&next))return respond_err(r,400,"invalid trigger mapping: require IN1..IN8 and supported functions");
    /* Same transaction and busy policy as authenticated BLE. The HTTP
     * 'revision' field is a schema version, not the BLE edit revision. */
    const int result=tm_commit(&next,(uint16_t)expected_config_revision,require_revision);
    if(result==2)return respond_err(r,409,"stop shows and release all 12V triggers before saving trigger mapping");
    if(result==3)return respond_err(r,409,"stale trigger mapping revision; read back current profile before saving");
    if(result==4)return respond_err(r,500,"failed to save trigger mapping in flash");
    if(result!=0)return respond_err(r,400,"invalid trigger mapping");
    return tm_get(r);
}
static esp_err_t show_get(httpd_req_t *r){
    TickType_t now=xTaskGetTickCount();
    ot_show_t cfg;bool active;uint32_t remaining=0;bool ready,persistent=false;
    bool fx_active=false;uint16_t fx_effect_id=0;
    portENTER_CRITICAL(&state_lock);
    cfg=native_show;active=show_window(now);
    if(active && ble_fx_enabled){
        fx_active=true;fx_effect_id=ble_fx_request.effect_id;
        /* The FX bank, not the legacy show deadline, owns per-zone TTLs.
         * Loaded standalone scenes do not expire after the preview TTL. */
        const uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
        for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
            const ot_fx_zone_slot_t *slot=&fx_bank.zones[o][z];
            if(!ot_fx_slot_active(slot,ms))continue;
            if(slot->standalone){persistent=true;continue;}
            const uint32_t duration=(uint32_t)slot->request.ttl_sec*1000u;
            const uint32_t elapsed=ms-slot->started_ms;
            if(duration-elapsed>remaining)remaining=duration-elapsed;
        }
        if(persistent)remaining=0;
    }else if(active){
        remaining=(uint32_t)((uint32_t)(native_show_deadline-now)*portTICK_PERIOD_MS);
    }
    ready=native_outputs_ready;
    portEXIT_CRITICAL(&state_lock);
    ot_wled_fx_metadata_t meta;
    const char *active_name=fx_active&&ot_effects_1d_metadata(fx_effect_id,&meta)?
      meta.name:ot_show_name(cfg.mode);
    char buf[390];
    snprintf(buf,sizeof(buf),"{\"active\":%s,\"effect\":\"%s\",\"remaining_ms\":%lu,\"persistent\":%s,\"brightness\":%u,\"speed\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"chain_mask\":%u,\"section_mask\":%u,\"bench_only\":true,\"outputs_ready\":%s}",
      active?"true":"false",active?active_name:"off",(unsigned long)remaining,persistent?"true":"false",
      cfg.brightness,cfg.speed,cfg.color.r,cfg.color.g,cfg.color.b,cfg.chain_mask,cfg.section_mask,ready?"true":"false");
    return send_json(r,buf);
}
static esp_err_t show_start(httpd_req_t *r){
    cJSON *obj=NULL;int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    cJSON *field=cJSON_GetObjectItemCaseSensitive(obj,"effect");
    const char *effect=cJSON_IsString(field)?field->valuestring:"";
    ot_show_mode_t mode=OT_SHOW_OFF;
    for(int i=OT_SHOW_SOLID;i<=OT_SHOW_RAINBOW;++i){
        if(!strcmp(effect,ot_show_name((ot_show_mode_t)i))){mode=(ot_show_mode_t)i;break;}
    }
    if(mode==OT_SHOW_OFF){cJSON_Delete(obj);return respond_err(r,400,"unknown native effect");}
    ot_show_t cfg={.mode=mode,.color={255,0,0},.brightness=24,.speed=50,
       .chain_mask=(uint8_t)((1u<<OT_CHAIN_COUNT)-1u),.section_mask=(uint16_t)((1u<<OT_SECTION_COUNT)-1u)};
    /* Canonical generic output selector; old left/right chain aliases stay
     * read-compatible for existing bench utilities, not as vehicle defaults. */
    const cJSON *output_mask=cJSON_GetObjectItemCaseSensitive(obj,"output_mask");
    const cJSON *legacy_chain=cJSON_GetObjectItemCaseSensitive(obj,"chain");
    if(output_mask && legacy_chain){cJSON_Delete(obj);return respond_err(r,400,"use output_mask or legacy chain, not both");}
    if(output_mask){
        const unsigned valid_mask=(1u<<OT_CHAIN_COUNT)-1u;
        if(!cJSON_IsNumber(output_mask)||output_mask->valuedouble!=output_mask->valueint||
           output_mask->valueint<1||(unsigned)output_mask->valueint>valid_mask){
            cJSON_Delete(obj);return respond_err(r,400,"output_mask must select one or more available outputs");
        }
        cfg.chain_mask=(uint8_t)output_mask->valueint;
    }else if(legacy_chain){
        if(!cJSON_IsString(legacy_chain)){cJSON_Delete(obj);return respond_err(r,400,"legacy chain must be a string");}
        const char *channels=legacy_chain->valuestring;
        if(!strcmp(channels,"left")||!strcmp(channels,"output1"))cfg.chain_mask=1u;
        else if(!strcmp(channels,"right")||!strcmp(channels,"output2"))cfg.chain_mask=2u;
        else if(strcmp(channels,"both")){cJSON_Delete(obj);return respond_err(r,400,"unknown legacy chain name");}
    }
    field=cJSON_GetObjectItemCaseSensitive(obj,"section_mask");
    if(field){if(!cJSON_IsNumber(field) || field->valuedouble!=field->valueint || field->valueint<1||field->valueint>255){cJSON_Delete(obj);return respond_err(r,400,"section_mask must be 1..255");}cfg.section_mask=(uint16_t)field->valueint;}
    const char *keys[]={"r","g","b","brightness","speed","ttl_ms"};
    for(int i=0;i<6;++i){field=cJSON_GetObjectItemCaseSensitive(obj,keys[i]);
      if(field && (!cJSON_IsNumber(field) || field->valuedouble!=field->valueint)){
          cJSON_Delete(obj);return respond_err(r,400,"integer parameters required");}
    }
    unsigned ttl=bounded_int(obj,"ttl_ms",15000,1000,OT_SHOW_MAX_MS);
    cfg.color=(ot_rgb_t){bounded_int(obj,"r",255,0,255),bounded_int(obj,"g",0,0,255),bounded_int(obj,"b",0,0,255)};
    cfg.brightness=bounded_int(obj,"brightness",24,0,OT_MAX_BRIGHTNESS);
    cfg.speed=bounded_int(obj,"speed",50,0,100);
    if(!ot_show_valid(&cfg,&layout)){cJSON_Delete(obj);return respond_err(r,400,"invalid show settings");}
    cJSON_Delete(obj);
    TickType_t now=xTaskGetTickCount();
    uint32_t start_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    portENTER_CRITICAL(&state_lock);
    bool ready=native_outputs_ready;
    if(ready){ble_owns_show=false;ble_fx_enabled=false;ot_fx_bank_stop_all(&fx_bank);native_show=cfg;native_show_deadline=now+pdMS_TO_TICKS(ttl);
      native_show_started_ms=start_ms;}
    portEXIT_CRITICAL(&state_lock);
    if(!ready)return respond_err(r,409,"pixel outputs not initialized");
    request_render_refresh();
    return send_json(r,"{\"ok\":true,\"active\":true,\"bench_only\":true,\"message\":\"native effect started on configured bench outputs; real trigger inputs always override\"}");
}
static esp_err_t show_stop(httpd_req_t *r){
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    ble_owns_show=false;ble_fx_enabled=false;ot_fx_bank_stop_all(&fx_bank);native_show.mode=OT_SHOW_OFF;native_show_deadline=now;
    stop_request_generation++;
    portEXIT_CRITICAL(&state_lock);
    request_render_refresh();
    ot_st_show_active(false);
    return send_json(r,"{\"ok\":true,\"active\":false}");
}
/* Provision an ephemeral per-boot BLE token through the existing WPA2 CORE AP.
 * BENCH ONLY: local Wi-Fi HTTP and unencrypted BLE are NOT production security.
 * No secrets are logged, cached, persisted, or exposed via the BLE information characteristic.
 */
static esp_err_t ble_session_get(httpd_req_t *r) {
    char token[33]={0};
    ot_ble_session_hex(token);
    if(strlen(token)!=32)return respond_err(r,409,"BLE session unavailable");
    char buf[135];
    snprintf(buf,sizeof(buf),"{\"protocol\":1,\"token\":\"%s\",\"bench_only\":true,\"volatile\":true}",token);
    return send_json(r,buf);
}

/* Called by NimBLE GATT task; render task still owns LED output.
 * Real optical trigger priority is unchanged in led_task.
 */
bool ot_ble_apply_show(const ot_ble_show_packet_t *request){
    if(!request)return false;
    ot_show_t show={.mode=(ot_show_mode_t)request->effect,
        .color={request->r,request->g,request->b},
        .brightness=request->brightness,.speed=request->speed,
        .chain_mask=request->chain_mask,.section_mask=request->zone_mask};
    if(!ot_show_valid(&show,&layout))return false;
    bool ready;
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    ready=native_outputs_ready;
    if(ready){native_show=show;ble_owns_show=true;ble_fx_enabled=false;ot_fx_bank_stop_all(&fx_bank);native_show_started_ms=(uint32_t)(esp_timer_get_time()/1000LL);
        native_show_deadline=now+pdMS_TO_TICKS((uint32_t)request->ttl_sec*1000u);}
    portEXIT_CRITICAL(&state_lock);
    if(ready)request_render_refresh();
    return ready;
}
/* Runtime control over BLE; all pixels are computed on CORE, never streamed by the app.
 * Temporary bench effects expire in 60 seconds; vehicle triggers overlay assigned zones.
 */
static bool ot_ble_apply_fx_internal(const ot_ble_fx_packet_t *req,bool standalone,
                                      const ot_fx_spark_style_t *spark,
                                      const ot_fx_overlay_config_t *overlay){
    ot_wled_fx_metadata_t meta;
    if((overlay&&!ot_fx_overlay_valid(overlay))||
       !req||!ot_effects_1d_metadata(req->effect_id,&meta)||
       !ot_palette_1d_supported(req->effect_id,req->palette_id)||
       req->chain_mask==0||
       (req->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))!=0||req->zone_mask==0||
       req->brightness>OT_MAX_BRIGHTNESS||req->ttl_sec<1||req->ttl_sec>60||
       !palette_resolved(req->palette_id))
        return false;
    const TickType_t now=xTaskGetTickCount();
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool ready;
    portENTER_CRITICAL(&state_lock);
    ready=native_outputs_ready;
    if(ready){
        /* Apply only the requested output/zone slots. Other zones retain FX phase. */
        ready=ot_fx_bank_apply_configured(&fx_bank,req,om_outputs,now_ms,standalone);
        if(ready&&spark){
            /* Optional sparkle tuning is transient per-zone state. Preserve
             * legacy fixed-size BLE FX packets and saved NVS profile layout. */
            for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
                if(!(req->chain_mask&(1u<<o)))continue;
                for(unsigned z=0;z<om_outputs[o].zone_count;z++){
                    if(!(req->zone_mask&(1u<<z))||
                       om_outputs[o].zones[z].pixels==0||
                       om_outputs[o].zones[z].role==OT_OM_OFF)continue;
                    fx_bank.zones[o][z].spark=*spark;
                }
            }
        }
    }
    if(ready&&overlay){
        for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
            if(!(req->chain_mask&(1u<<o)))continue;
            for(unsigned z=0;z<om_outputs[o].zone_count;z++){
                if(!(req->zone_mask&(1u<<z))||!om_outputs[o].zones[z].pixels||
                   om_outputs[o].zones[z].role==OT_OM_OFF)continue;
                fx_bank.zones[o][z].overlay=*overlay;
            }
        }
    }
    if(ready){
        /* Existing show state gates arbitration, deadlines and trigger-independent status. */
        native_show=(ot_show_t){
            .mode=OT_SHOW_SOLID,.color={req->r,req->g,req->b},
            .brightness=req->brightness,.speed=0,
            .chain_mask=req->chain_mask,.section_mask=req->zone_mask
        };
        ble_fx_request=*req;
        ble_fx_enabled=true;
        ble_owns_show=true;
        native_show_started_ms=(uint32_t)(esp_timer_get_time()/1000LL);
        native_show_deadline=now+pdMS_TO_TICKS((uint32_t)req->ttl_sec*1000u);
    }
    portEXIT_CRITICAL(&state_lock);
    if(ready)request_render_refresh();
    return ready;
}
bool ot_ble_apply_fx(const ot_ble_fx_packet_t *req){
    return ot_ble_apply_fx_internal(req,false,NULL,NULL); /* live 60-sec bench preview */
}
/* Firmware-driven effect capability catalog for the CORE developer page.
 * Only the renderer implementations compiled into this firmware are listed.
 * Original and source-derived modes remain distinct from compatibility
 * recipes: the catalog is NEVER evidence of upstream pixel parity.
 */
static esp_err_t fx_catalog_get(httpd_req_t *r){
    uint16_t ids[254]={0};
    const size_t total=ot_effects_1d_supported_ids(ids,254u);
    if(total>254u)return respond_err(r,500,"effect catalog exceeds firmware limit");
    cJSON *root=cJSON_CreateObject();
    if(!root)return respond_err(r,500,"out of memory");
    cJSON_AddBoolToObject(root,"bench_only",true);
    cJSON_AddNumberToObject(root,"count",(double)total);
    cJSON *items=cJSON_AddArrayToObject(root,"effects");
    if(!items){cJSON_Delete(root);return respond_err(r,500,"out of memory");}
    for(size_t i=0;i<total;i++){
        ot_wled_fx_metadata_t meta={0};
        if(!ot_effects_1d_metadata(ids[i],&meta))continue;
        cJSON *entry=cJSON_CreateObject();
        if(!entry){cJSON_Delete(root);return respond_err(r,500,"out of memory");}
        cJSON_AddNumberToObject(entry,"id",ids[i]);
        cJSON_AddStringToObject(entry,"name",meta.name);
        const char *kind=ids[i]>=OT_FX_EMBERS?"original":
                ot_wled_fx_metadata(ids[i],&meta)?"source-derived":"compatibility";
        cJSON_AddStringToObject(entry,"implementation",kind);
        cJSON_AddBoolToObject(entry,"palette_supported",meta.supports_palette);
        cJSON_AddItemToArray(items,entry);
    }
    char *data=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if(!data)return respond_err(r,500,"out of memory");
    const esp_err_t result=send_json(r,data);
    free(data);
    return result;
}
/* HTTP effect preview is strictly for the WPA2 isolated bench AP and
 * retains CORE's existing zone validation, software brightness limit,
 * render state, physical-trigger arbitration and Stop path.
 * It MUST NOT be exposed on production vehicle networks unchanged.
 */
/* Preview endpoints use physical output bit indices, never vehicle sides.
 * Keep legacy left/right input aliases only for existing bench clients. */
static bool preview_output_mask(const cJSON *obj,uint8_t *mask){
    const cJSON *selection=cJSON_GetObjectItemCaseSensitive(obj,"output_mask");
    const cJSON *legacy=cJSON_GetObjectItemCaseSensitive(obj,"chain");
    const unsigned all=(1u<<OT_CHAIN_COUNT)-1u;
    if(!mask||all>UINT8_MAX|| (selection&&legacy))return false;
    *mask=(uint8_t)all;
    if(selection){
        if(!cJSON_IsNumber(selection)||selection->valuedouble!=selection->valueint||
           selection->valueint<1||(unsigned)selection->valueint>all)return false;
        *mask=(uint8_t)selection->valueint;
    }else if(legacy){
        if(!cJSON_IsString(legacy))return false;
        if(!strcmp(legacy->valuestring,"left")||!strcmp(legacy->valuestring,"output1"))
            *mask=1u;
        else if(!strcmp(legacy->valuestring,"right")||!strcmp(legacy->valuestring,"output2"))
            *mask=2u;
        else if(strcmp(legacy->valuestring,"both"))return false;
        if((*mask&~all)!=0u)return false;
    }
    return true;
}
static esp_err_t fx_preview_post(httpd_req_t *r){
    cJSON *obj=NULL;
    int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    unsigned id=0,zone=0,bri=0,speed=0,intensity=0,palette=0,ttl=0;
    unsigned fg[3],bg[3];
    const char *names[]={"r","g","b","bg_r","bg_g","bg_b"};
    unsigned values[6]={0};
    bool valid=om_number(obj,"effect_id",&id,0,65535)&&
        om_number(obj,"zone_mask",&zone,1,255)&&
        om_number(obj,"brightness",&bri,0,OT_MAX_BRIGHTNESS)&&
        om_number(obj,"speed",&speed,0,255)&&
        om_number(obj,"intensity",&intensity,0,255)&&
        om_number(obj,"palette_id",&palette,0,255)&&
        om_number(obj,"ttl_sec",&ttl,1,60);
    for(unsigned i=0;i<6&&valid;i++)
        valid=om_number(obj,names[i],&values[i],0,255);
    uint8_t chain_mask=0;
    valid=valid&&preview_output_mask(obj,&chain_mask);
    if(!valid){cJSON_Delete(obj);return respond_err(r,400,"invalid effect preview parameters");}
    for(unsigned i=0;i<3;i++){fg[i]=values[i];bg[i]=values[i+3];}
    ot_ble_fx_packet_t req={
        .effect_id=(uint16_t)id,.chain_mask=chain_mask,.zone_mask=(uint8_t)zone,
        .r=(uint8_t)fg[0],.g=(uint8_t)fg[1],.b=(uint8_t)fg[2],
        .bg_r=(uint8_t)bg[0],.bg_g=(uint8_t)bg[1],.bg_b=(uint8_t)bg[2],
        .brightness=(uint8_t)bri,.speed=(uint8_t)speed,
        .intensity=(uint8_t)intensity,.palette_id=(uint8_t)palette,
        .ttl_sec=(uint8_t)ttl
    };
    /* Optional RAM-only compositor. Never changes effect ID, trigger priority,
     * saved palette profiles, or BLE FX packet layouts. */
    struct {const char *key;unsigned max;unsigned value;} options[]={
       {"overlay_style",3u,0u},{"overlay_peak",32u,32u},
       {"overlay_density",255u,160u},{"overlay_radius",12u,4u},
       {"overlay_fade_ms",1000u,120u},{"overlay_r",255u,255u},
       {"overlay_g",255u,255u},{"overlay_b",255u,255u}
    };
    for(unsigned k=0;k<sizeof(options)/sizeof(options[0]);k++){
        const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,options[k].key);
        if(!v)continue;
        if(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint||
           v->valueint<0||(unsigned)v->valueint>options[k].max){
            cJSON_Delete(obj);return respond_err(r,400,"invalid overlay setting");
        }
        options[k].value=(unsigned)v->valueint;
    }
    const ot_fx_overlay_config_t overlay={
       .style=(uint8_t)options[0].value,.peak=(uint8_t)options[1].value,
       .density=(uint8_t)options[2].value,.radius=(uint8_t)options[3].value,
       .fade_ms=(uint16_t)options[4].value,
       .color={(uint8_t)options[5].value,(uint8_t)options[6].value,
               (uint8_t)options[7].value}
    };
    const cJSON *only=cJSON_GetObjectItemCaseSensitive(obj,"overlay_only");
    if(!ot_fx_overlay_valid(&overlay)||(only&&!cJSON_IsBool(only))){
        cJSON_Delete(obj);return respond_err(r,400,"invalid overlay parameters");
    }
    const bool overlay_only=cJSON_IsTrue(only);
    cJSON_Delete(obj);
    if(overlay_only){
        const TickType_t now=xTaskGetTickCount();
        const uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
        bool changed=false;
        portENTER_CRITICAL(&state_lock);
        if(native_outputs_ready&&ble_fx_enabled&&show_window(now))
            changed=ot_fx_bank_set_overlay(&fx_bank,om_outputs,
                  req.chain_mask,req.zone_mask,ms,&overlay);
        portEXIT_CRITICAL(&state_lock);
        if(!changed)return respond_err(r,409,"no matching active effect zones");
        request_render_refresh();
        return send_json(r,"{\"ok\":true,\"overlay_only\":true,\"phase_preserved\":true,\"bench_only\":true}");
    }
    if(!ot_ble_apply_fx_internal(&req,false,NULL,&overlay))
        return respond_err(r,409,"unsupported effect, palette, or no configured zones");
    char payload[152];
    snprintf(payload,sizeof(payload),
        "{\"ok\":true,\"effect_id\":%u,\"ttl_sec\":%u,\"bench_only\":true,\"trigger_priority_preserved\":true}",
        id,ttl);
    return send_json(r,payload);
}

/* Manual BENCH preview only. No ignition trigger registration, NVS event
 * enablement, external LED task or power-off path. Reuse the current CORE
 * authenticated-BLE effect renderer's validated zone bank/physical arbitration.
 * Like /show/start, this endpoint is available only to clients on the
 * controller's password-protected BENCH AP; it is not a road-ready API.
 */
static esp_err_t auto_preview_post(httpd_req_t *r){
    cJSON *obj=NULL;
    const int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    cJSON *fx=cJSON_GetObjectItemCaseSensitive(obj,"effect");
    if(!cJSON_IsString(fx)){cJSON_Delete(obj);return respond_err(r,400,"effect is required");}
    uint16_t id=0;
    const char *key=fx->valuestring;
    if(!strcmp(key,"materialize_sparks"))id=OT_FX_MATERIALIZE_SPARKS;
    else if(!strcmp(key,"random_materialize"))id=OT_FX_RANDOM_MATERIALIZE;
    else if(!strcmp(key,"smooth_fade"))id=OT_FX_SMOOTH_FADE;
    else if(!strcmp(key,"plasma_sweep"))id=OT_FX_PLASMA_SWEEP;
    else if(!strcmp(key,"smooth_fade_out"))id=OT_FX_SMOOTH_FADE_OUT;
    else{cJSON_Delete(obj);return respond_err(r,400,"unsupported automatic event preview");}
    /* Preserve the allowlisted label before deleting the request JSON. */
    char event_key[32]={0};
    snprintf(event_key,sizeof(event_key),"%s",key);
    uint8_t chain_mask=0;
    if(!preview_output_mask(obj,&chain_mask)){
        cJSON_Delete(obj);return respond_err(r,400,"invalid output_mask or legacy chain");
    }
    const char *keys[]={"r","g","b","brightness","speed","intensity",
                         "spark_style","spark_peak","spark_r","spark_g","spark_b"};
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++){
        const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,keys[i]);
        if(v&&(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint)){
            cJSON_Delete(obj);return respond_err(r,400,"integer parameters required");
        }
    }
    const uint8_t speed=(uint8_t)bounded_int(obj,"speed",180,0,255);
    const ot_ble_fx_packet_t req={
        .effect_id=id,.chain_mask=chain_mask,.zone_mask=255,
        .r=(uint8_t)bounded_int(obj,"r",255,0,255),
        .g=(uint8_t)bounded_int(obj,"g",31,0,255),
        .b=(uint8_t)bounded_int(obj,"b",20,0,255),
        .bg_r=0,.bg_g=0,.bg_b=0,
        .brightness=(uint8_t)bounded_int(obj,"brightness",20,0,OT_MAX_BRIGHTNESS),
        .speed=speed,
        .intensity=(uint8_t)bounded_int(obj,"intensity",220,0,255),
        .palette_id=0,.ttl_sec=6
    };
    const ot_fx_spark_style_t spark={
        .style=(uint8_t)bounded_int(obj,"spark_style",0,0,3),
        .peak=(uint8_t)bounded_int(obj,"spark_peak",32,1,32),
        .color={
            (uint8_t)bounded_int(obj,"spark_r",255,0,255),
            (uint8_t)bounded_int(obj,"spark_g",255,0,255),
            (uint8_t)bounded_int(obj,"spark_b",255,0,255)
        }
    };
    cJSON_Delete(obj);
    if(!ot_ble_apply_fx_internal(&req,false,&spark,NULL))
        return respond_err(r,409,"no configured active output zones or outputs not ready");
    char payload[185];
    snprintf(payload,sizeof(payload),
       "{\"ok\":true,\"event\":\"%s\",\"duration_ms\":%lu,\"preview_ttl_sec\":6,\"brightness_cap\":%u,\"bench_only\":true}",
       event_key,(unsigned long)ot_auto_fx_duration_ms(speed),(unsigned)OT_MAX_BRIGHTNESS);
    return send_json(r,payload);
}
void ot_ble_stop_show(void){
    const TickType_t now=xTaskGetTickCount();
    bool stopped=false,refresh_requested=false;
    portENTER_CRITICAL(&state_lock);
    /* STOP cancels all active BLE-owned zones, including a scene whose
     * ownership bit was cleared during a pending disconnect. A genuine
     * unrelated HTTP show remains untouched. */
    if(ble_owns_show || ble_fx_enabled){
        ble_owns_show=false;
        ble_fx_enabled=false;
        ot_fx_bank_stop_all(&fx_bank);
        native_show.mode=OT_SHOW_OFF;
        native_show_deadline=now;
        stop_request_generation++;
        stopped=true;refresh_requested=true;
    }else if(native_show.mode==OT_SHOW_OFF || !show_window(now)){
        /* Stop may be pressed after the 60-second FX timeout. Record the
         * requested post-stop refresh even when no show is still owned. */
        stop_request_generation++;
        refresh_requested=true;
    }
    portEXIT_CRITICAL(&state_lock);
    if(refresh_requested)request_render_refresh();
    /* Restore the previous status indication immediately, rather than
     * leaving a stale rainbow until the next BLE preview TTL expires. */
    if(stopped)ot_st_show_active(false);
}
/* Eight opt-in named-by-slot effect snapshots in controller NVS. Flash records are
 * independently checksummed and versioned; they never alter road-lighting maps.
 * Save/load/delete are invoked only through authenticated BLE GATT commands.
 */
static bool fx_slot_key(uint8_t slot,char key[8]){
 if(slot>=OT_FX_PRESET_SLOTS)return false;
 snprintf(key,8,"slot%u",(unsigned)slot);return true;
}
bool ot_ble_save_fx_preset(uint8_t slot){
 char key[8];if(!fx_slot_key(slot,key))return false;
 ot_fx_zone_bank_t snapshot={0};bool active;
 const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
 TickType_t now=xTaskGetTickCount();
 portENTER_CRITICAL(&state_lock);
 active=native_outputs_ready&&ble_fx_enabled&&show_window(now);
 if(active)memcpy(&snapshot,&fx_bank,sizeof(snapshot));
 portEXIT_CRITICAL(&state_lock);
 ot_fx_scene_record_t record;
 if(!active||!ot_fx_scene_pack(&record,&snapshot,now_ms))return false;
 nvs_handle_t h;
 if(nvs_open("ot_fx_presets",NVS_READWRITE,&h)!=ESP_OK)return false;
 esp_err_t e=nvs_set_blob(h,key,&record,sizeof(record));
 if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);
 return e==ESP_OK;
}
bool ot_ble_play_fx_preset(uint8_t slot){
 char key[8];if(!fx_slot_key(slot,key))return false;
 nvs_handle_t h;
 if(nvs_open("ot_fx_presets",NVS_READONLY,&h)!=ESP_OK)return false;
 size_t n=0;esp_err_t e=nvs_get_blob(h,key,NULL,&n);
 if(e!=ESP_OK){nvs_close(h);return false;}
 if(n==sizeof(ot_fx_preset_record_t)){
     /* Migration: earlier eight slots store only one single-effect snapshot.
      * Do not overwrite on load. Re-saving updates to v2 multi-zone format. */
     ot_fx_preset_record_t legacy={0};
     e=nvs_get_blob(h,key,&legacy,&n);
     nvs_close(h);
     return e==ESP_OK&&n==sizeof(legacy)&&ot_fx_preset_valid(&legacy)&&
         ot_ble_apply_fx_internal(&legacy.fx,true,NULL,NULL);
 }
 if(n!=sizeof(ot_fx_scene_record_t)){nvs_close(h);return false;}
 ot_fx_scene_record_t record={0};
 e=nvs_get_blob(h,key,&record,&n);
 nvs_close(h);
 if(e!=ESP_OK||n!=sizeof(record)||!ot_fx_scene_valid(&record))return false;
 const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
 TickType_t now=xTaskGetTickCount();
 bool ready;
 portENTER_CRITICAL(&state_lock);
 ready=native_outputs_ready;
 if(ready){
     ot_fx_zone_bank_t replacement={0};
     uint8_t out_mask=0,zone_mask=0;
     for(unsigned o=0;o<OT_OM_OUTPUTS&&ready;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
         if(!(record.active_mask&(uint16_t)(1u<<(o*OT_OM_ZONES+z))))continue;
         const ot_ble_fx_packet_t *p=&record.effect[o][z];
         if(!palette_resolved(p->palette_id) ||
            !ot_fx_bank_apply_configured(&replacement,p,om_outputs,now_ms,true)){
             ready=false;break; /* layout changed: preserve original live scene */
         }
         out_mask=(uint8_t)(out_mask|(uint8_t)(1u<<o));
         zone_mask=(uint8_t)(zone_mask|(uint8_t)(1u<<z));
     }
     if(ready){
         /* Transaction: no old-zone FX survive scene load. */
         fx_bank=replacement;
         /* Diagnostic legacy show fields describe union of targeted zones. */
         native_show=(ot_show_t){.mode=OT_SHOW_SOLID,
            .color={0,0,0},.brightness=OT_MAX_BRIGHTNESS,.speed=0,
            .chain_mask=out_mask,.section_mask=zone_mask};
         /* Only use last stored entry for legacy "current effect" reporting. */
         for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
             if(record.active_mask&(uint16_t)(1u<<(o*OT_OM_ZONES+z)))
                 ble_fx_request=record.effect[o][z];
         }
         ble_fx_enabled=true;ble_owns_show=true;
         native_show_started_ms=now_ms;
         native_show_deadline=now+pdMS_TO_TICKS(60000u);
     }
 }
 portEXIT_CRITICAL(&state_lock);
 if(ready)request_render_refresh();
 return ready;
}
bool ot_ble_delete_fx_preset(uint8_t slot){
 char key[8];if(!fx_slot_key(slot,key))return false;
 nvs_handle_t h;
 if(nvs_open("ot_fx_presets",NVS_READWRITE,&h)!=ESP_OK)return false;
 esp_err_t e=nvs_erase_key(h,key);
 if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);
 return e==ESP_OK;
}
/* Palette profile NVS writes use the SAME serialization and active-input
 * veto as Output/Trigger Manager saves. No flash I/O runs under state_lock.
 * Unlike geometry edits, a palette may change during an idle-input FX show. */
bool ot_ble_palette_mutation_begin(void){
    if(!input_sampler_ready||!om_write_mutex||
       xSemaphoreTake(om_write_mutex,0)!=pdTRUE)return false;
    /* Immediate GPIO sampling covers the sampler's debounce assertion delay. */
    const bool raw_active=tm_gpio_input_mask()!=0;
    const TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    const bool busy=om_deferred.pending||tm_deferred.pending||raw_active||
        live_input_mask!=0||
        (before_deadline(now,sim_deadline)&&sim_mask!=0);
    portEXIT_CRITICAL(&state_lock);
    if(busy)xSemaphoreGive(om_write_mutex);
    return !busy;
}
void ot_ble_palette_mutation_end(bool changed){
    /* Publish/write is finished before the owner wakes the render task. */
    xSemaphoreGive(om_write_mutex);
    if(changed)request_render_refresh();
}
bool ot_ble_palette_in_use(uint8_t index){
    if(index>=OT_PALETTE_SLOTS)return false;
    const uint8_t id=(uint8_t)(200u-index);
    const uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool used=false;
    portENTER_CRITICAL(&state_lock);
    if(ble_fx_enabled)
        for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
            for(unsigned z=0;z<OT_OM_ZONES;z++)
                if(ot_fx_slot_active(&fx_bank.zones[o][z],ms) &&
                   fx_bank.zones[o][z].request.palette_id==id)used=true;
    portEXIT_CRITICAL(&state_lock);
    return used;
}
void ot_ble_disconnect_show(void){
 /* Disconnect expires only temporary zones; explicitly loaded NVS effects
  * remain local on CORE. Vehicle inputs override only their assigned zones.
  * A legacy HTTP show is not owned by the BLE session. */
 bool owned_before,enabled_before,owned_after,enabled_after,active_after;
 portENTER_CRITICAL(&state_lock);
 owned_before=ble_owns_show;
 enabled_before=ble_fx_enabled;
 if(ble_owns_show){
     ot_fx_bank_stop_temporary(&fx_bank);
     bool active=ble_fx_enabled&&
         ot_fx_bank_any_active(&fx_bank,(uint32_t)(esp_timer_get_time()/1000LL));
     if(!active){
         ble_owns_show=false;ble_fx_enabled=false;native_show.mode=OT_SHOW_OFF;
         stop_request_generation++;
     }
 }
 owned_after=ble_owns_show;
 enabled_after=ble_fx_enabled;
 active_after=show_window(xTaskGetTickCount());
 portEXIT_CRITICAL(&state_lock);
 if(owned_before)request_render_refresh();
 ESP_LOGI(TAG,"BLE disconnect cleanup: owned=%d->%d enabled=%d->%d active_after=%d",
          owned_before,owned_after,enabled_before,enabled_after,active_after);
}

static esp_err_t status_get(httpd_req_t *r) {
    TickType_t now=xTaskGetTickCount();
    uint8_t physical,sim;ot_tm_profile_t tm;bool sim_active,demo_active,show_active,outputs_ready;ot_demo_mode_t mode;uint32_t frames;uint16_t target_fps;
    uint32_t refresh0,refresh1,stop_requested,stop_rendered,partial_frames,observed_fps,faulted_outputs;
    uint16_t configured_pixels[OT_CHAIN_COUNT];
    portENTER_CRITICAL(&state_lock);
    physical=live_input_mask;tm=tm_profile;
    sim_active=before_deadline(now,sim_deadline);
    sim=sim_active?sim_mask:0;
    demo_active=before_deadline(now,demo_deadline);
    mode=demo_active?demo.mode:OT_DEMO_OFF;
    frames=frame_index;
    target_fps=adaptive_target_fps;
    observed_fps=measured_fps;
    show_active=show_window(now);
    outputs_ready=native_outputs_ready;

    refresh0=output_refreshes[0];refresh1=output_refreshes[1];
    for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++)configured_pixels[ch]=om_outputs[ch].active_pixels;
    stop_requested=stop_request_generation;stop_rendered=stop_render_generation;
    partial_frames=incomplete_frames;
    faulted_outputs=output_runtime_fault_mask;
    portEXIT_CRITICAL(&state_lock);
    const bool ble_radio_ready=ot_ble_radio_ever_ready();
    const uint8_t function_mask=ot_tm_resolve(&tm,physical);
    const uint8_t effective=(uint8_t)(function_mask|sim);
    const ot_arbitration_t arb=ot_arbitrate(effective,mode!=OT_DEMO_OFF,show_active);
    /* Runtime telemetry is universal and reflects configured output zones.
     * Historical left/right tail examples belong exclusively in host tests. */
    char arb_json[480];
    snprintf(arb_json,sizeof(arb_json),
      "{\"mode\":\"%s\",\"mapping\":\"configured_output_zones\",\"hazards\":%s,\"brake_over_parking\":%s,"
      "\"show_requested\":%s,\"show_suppressed\":%s,\"show_rendering\":%s,"
      "\"demo_requested\":%s,\"demo_suppressed\":%s}",
      ot_arb_mode_name(arb.mode),arb.hazards?"true":"false",arb.brake_over_parking?"true":"false",
      arb.show_requested?"true":"false",arb.show_suppressed?"true":"false",
      arb.show_requested?"true":"false",arb.demo_requested?"true":"false",
      arb.demo_suppressed?"true":"false");
    char active_pixels_json[OT_CHAIN_COUNT*6u+3u];
    size_t used=0;
    active_pixels_json[used++]='[';
    for(unsigned i=0;i<OT_CHAIN_COUNT;i++){
        int written=snprintf(active_pixels_json+used,sizeof(active_pixels_json)-used,
                             "%s%u",i?",":"",(unsigned)configured_pixels[i]);
        if(written<0||(size_t)written>=sizeof(active_pixels_json)-used)
            return respond_err(r,500,"output status formatting overflow");
        used+=(size_t)written;
    }
    active_pixels_json[used++]=']';
    active_pixels_json[used]=0;
    char elf_sha256[65]={0};
    (void)esp_app_get_elf_sha256(elf_sha256,sizeof(elf_sha256));
    char buf[1800];char led_json[384];ot_st_format_json(led_json,sizeof(led_json));
    snprintf(buf,sizeof(buf),"{\"name\":\"OpenTail CORE Alpha\",\"version\":\"%s\",\"elf_sha256\":\"%s\",\"bench_only\":true,\"hardware_failover\":false,\"ble_ready\":%s,\"ble_radio_ready\":%s,\"outputs_ready\":%s,\"mode\":\"%s\",\"input_mask\":%u,\"simulated_mask\":%u,\"physical_function_mask\":%u,\"effective_mask\":%u,\"demo_active\":%s,\"native_show_active\":%s,\"frames\":%lu,\"measured_fps\":%lu,\"target_fps\":%u,\"active_pixels_per_output\":%s,\"leds_per_chain\":%u,\"brightness_cap\":%u,\"output_refreshes\":[%lu,%lu],\"incomplete_frames\":%lu,\"failed_output_mask\":%lu,\"stop_requested\":%lu,\"stop_rendered\":%lu,\"status_led\":%s,\"arbitration\":%s}",
       OT_FW,elf_sha256,ble_startup_ready?"true":"false",ble_radio_ready?"true":"false",
       outputs_ready?"true":"false","user-configured",
       (unsigned)physical,(unsigned)sim,(unsigned)function_mask,(unsigned)effective,mode==OT_DEMO_OFF?"false":"true",show_active?"true":"false",
       (unsigned long)frames,(unsigned long)observed_fps,(unsigned)target_fps,active_pixels_json,(unsigned)configured_pixels[0],(unsigned)layout.brightness_cap,
       (unsigned long)refresh0,(unsigned long)refresh1,(unsigned long)partial_frames,
       (unsigned long)faulted_outputs,(unsigned long)stop_requested,(unsigned long)stop_rendered,led_json,arb_json);
    return send_json(r,buf);
}
static esp_err_t caps_get(httpd_req_t *r) {
    return send_json(r,"{\"protocol\":\"opentail-core-alpha-v1\",\"bench_only\":true,\"pixel_protocol\":\"WS2815-compatible RMT 800kHz\",\"color_order\":\"GRB\",\"color_order_configurable\":true,\"wled_renderer\":\"compatibility\",\"wled_pixel_parity_verified\":false,\"native_wled_firmware_unified\":false,\"outputs\":2,\"physical_inputs\":8,\"active_functions\":[\"park\",\"brake\",\"left\",\"right\",\"reverse\",\"aux1\",\"aux2\",\"aux3\"],\"reserved_inputs\":[],\"wifi\":\"WPA2 Access Point\",\"bluetooth\":true,\"ble_status_only\":false,\"ble_studio_controls\":false,\"ble_bench_show_v1\":true,\"ble_output_config_v1\":true,\"ble_session_requires_wifi\":true,\"firmware_sequences\":false,\"native_show_engine\":true,\"native_effects\":[\"solid\",\"chase\",\"breathe\",\"wipe\",\"rainbow\"],\"wled_1d_effect_count\":120,\"ble_effect_id_count\":140,\"target_fps\":120,\"fps_policy\":\"adaptive_load_53_to_120\",\"preferred_fps_min\":53,\"preferred_fps_max\":120,\"studio_embedded\":false,\"developer_console\":true,\"studio_wled_api_compatible\":false,\"hardware_failover\":false,\"trigger_arbitration_v1\":true,\"trigger_manager_v1\":true,\"output_manager_v1\":true,\"output_manager_nvs\":true,\"status_led\":true,\"status_led_control\":false}");
}
static esp_err_t status_led_get(httpd_req_t *r){
    char buf[384];ot_st_format_json(buf,sizeof(buf));return send_json(r,buf);
}
/* Fixed-duration, strictly onboard RGB self-test; never touches external WS2815 outputs. */
static esp_err_t status_self_test_post(httpd_req_t *r){
    cJSON *obj=NULL;
    int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,"mode");
    const char *mode=cJSON_IsString(v)?v->valuestring:"";
    ot_st_mode_t test=OT_ST_OFF;
    if(!strcmp(mode,"rainbow"))test=OT_ST_SHOW;
    else if(strcmp(mode,"stop")){cJSON_Delete(obj);return respond_err(r,400,"mode must be rainbow or stop (red reserved for actual faults)");}
    cJSON_Delete(obj);
    if(!ot_st_self_test(test))return respond_err(r,409,"self test unavailable or real fault active");
    return send_json(r,test==OT_ST_OFF?
        "{\"ok\":true,\"self_test_active\":false,\"message\":\"normal status restored\"}":
        "{\"ok\":true,\"self_test_active\":true,\"duration_ms\":5000,\"message\":\"onboard RGB only; no real fault or show started\"}");
}
static esp_err_t status_session_post(httpd_req_t *r){
    cJSON *obj=NULL;int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    const cJSON *active=cJSON_GetObjectItemCaseSensitive(obj,"connected");
    if(!cJSON_IsBool(active)){cJSON_Delete(obj);return respond_err(r,400,"connected must be boolean");}
    ot_st_session(cJSON_IsTrue(active));cJSON_Delete(obj);
    return send_json(r,"{\"ok\":true}");
}
static esp_err_t demo_post(httpd_req_t *r) {
    cJSON *obj=NULL;
    int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    cJSON *mode_json=cJSON_GetObjectItemCaseSensitive(obj,"mode");
    const char *str=cJSON_IsString(mode_json)?mode_json->valuestring:"";
    ot_demo_mode_t mode=OT_DEMO_OFF;
    if(!strcmp(str,"solid")) mode=OT_DEMO_SOLID;
    else if(!strcmp(str,"chase")) mode=OT_DEMO_CHASE;
    else if(strcmp(str,"off")){cJSON_Delete(obj);return respond_err(r,400,"mode must be solid, chase or off");}
    unsigned rcol=bounded_int(obj,"r",0,0,255),gcol=bounded_int(obj,"g",0,0,255),bcol=bounded_int(obj,"b",0,0,255);
    unsigned brightness=bounded_int(obj,"brightness",32,0,OT_MAX_BRIGHTNESS);
    unsigned ttl=bounded_int(obj,"ttl_ms",3000,100,OT_DEMO_MAX_MS);
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    demo=(ot_demo_t){mode,{rcol,gcol,bcol},brightness};
    demo_deadline=mode==OT_DEMO_OFF?now:now+pdMS_TO_TICKS(ttl);
    portEXIT_CRITICAL(&state_lock);
    request_render_refresh();
    cJSON_Delete(obj);
    return send_json(r,"{\"ok\":true,\"message\":\"temporary demo scheduled; real trigger inputs have priority\"}");
}
static esp_err_t simulate_post(httpd_req_t *r) {
    cJSON *obj=NULL;
    int status=recv_json(r,&obj);
    if(status!=200){cJSON_Delete(obj);return respond_err(r,status,"invalid JSON request");}
    cJSON *arr=cJSON_GetObjectItemCaseSensitive(obj,"signals");
    if(!cJSON_IsArray(arr)){cJSON_Delete(obj);return respond_err(r,400,"signals must be an array");}
    uint8_t mask=0;
    cJSON *v=NULL;
    cJSON_ArrayForEach(v,arr){
       if(!cJSON_IsString(v)||!ot_tm_signal_mask_of(v->valuestring)){
          cJSON_Delete(obj);return respond_err(r,400,"unrecognized signal");
       }
       mask|=ot_tm_signal_mask_of(v->valuestring);
    }
    unsigned ttl=bounded_int(obj,"ttl_ms",2000,100,OT_SIM_MAX_MS);
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    sim_mask=mask;
    sim_deadline=mask?now+pdMS_TO_TICKS(ttl):now;
    portEXIT_CRITICAL(&state_lock);
    request_render_refresh();
    cJSON_Delete(obj);
    return send_json(r,"{\"ok\":true,\"message\":\"temporary isolated bench input simulation scheduled\"}");
}
static esp_err_t stop_post(httpd_req_t *r) {
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    demo.mode=OT_DEMO_OFF;demo_deadline=now;
    sim_mask=0;sim_deadline=now;
    ble_owns_show=false;ble_fx_enabled=false;ot_fx_bank_stop_all(&fx_bank);
    native_show.mode=OT_SHOW_OFF;native_show_deadline=now;
    stop_request_generation++;
    portEXIT_CRITICAL(&state_lock);
    request_render_refresh();
    ot_st_show_active(false);
    return send_json(r,"{\"ok\":true,\"message\":\"show, demo and simulation stopped; physical inputs still take precedence\"}");
}
/* CORE ships only its own developer console. The approved Studio is
 * distributed independently as web-studio/opentail.htm and via Android.
 * CMake embeds this static page; no WLED runtime or browser network access
 * beyond the CORE bench AP is needed.
 */
extern const uint8_t ot_dev_console_start[] asm("_binary_dev_console_htm_start");
static esp_err_t dev_get(httpd_req_t *r){
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_sendstr(r,(const char *)ot_dev_console_start);
}
static esp_err_t legacy_dev_redirect(httpd_req_t *r){
    httpd_resp_set_status(r,"302 Found");
    httpd_resp_set_hdr(r,"Location","/dev");
    return httpd_resp_sendstr(r,"Controller tools have moved to /dev. OpenTail Studio is a separate design app.");
}
static bool http_start(void) {
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.max_uri_handlers=30;cfg.stack_size=8192;cfg.recv_wait_timeout=30;
    httpd_handle_t server=NULL;
    esp_err_t e=httpd_start(&server,&cfg);
    if(e!=ESP_OK){
        ESP_LOGE(TAG,"CORE HTTP server startup failed: %s",esp_err_to_name(e));
        return false;
    }
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=dev_get},
        {.uri="/dev",.method=HTTP_GET,.handler=dev_get},
        {.uri="/studio",.method=HTTP_GET,.handler=legacy_dev_redirect},
        {.uri="/api/v1/status",.method=HTTP_GET,.handler=status_get},
        {.uri="/api/v1/ota",.method=HTTP_GET,.handler=ot_ota_status_get},
        {.uri="/api/v1/ota",.method=HTTP_POST,.handler=ot_ota_upload_post},
        {.uri="/api/v1/status-led",.method=HTTP_GET,.handler=status_led_get},
        {.uri="/status-self-test",.method=HTTP_GET,.handler=legacy_dev_redirect},
        {.uri="/api/v1/status-led/self-test",.method=HTTP_POST,.handler=status_self_test_post},
        {.uri="/api/v1/status-led/session",.method=HTTP_POST,.handler=status_session_post},
        {.uri="/api/v1/capabilities",.method=HTTP_GET,.handler=caps_get},
        {.uri="/api/v1/ble/session",.method=HTTP_GET,.handler=ble_session_get},
        {.uri="/api/v1/demo",.method=HTTP_POST,.handler=demo_post},
        {.uri="/api/v1/simulate",.method=HTTP_POST,.handler=simulate_post},
        {.uri="/api/v1/stop",.method=HTTP_POST,.handler=stop_post},
        {.uri="/api/v1/show",.method=HTTP_GET,.handler=show_get},
        {.uri="/api/v1/show/start",.method=HTTP_POST,.handler=show_start},
        {.uri="/api/v1/show/stop",.method=HTTP_POST,.handler=show_stop},
        {.uri="/auto-events/test",.method=HTTP_GET,.handler=legacy_dev_redirect},
        {.uri="/api/v1/auto-events/preview",.method=HTTP_POST,.handler=auto_preview_post},
        {.uri="/api/v1/fx/catalog",.method=HTTP_GET,.handler=fx_catalog_get},
        {.uri="/api/v1/fx/preview",.method=HTTP_POST,.handler=fx_preview_post},
        {.uri="/api/v1/outputs",.method=HTTP_GET,.handler=om_get},
        {.uri="/api/v1/triggers",.method=HTTP_GET,.handler=tm_get},
        {.uri="/api/v1/triggers",.method=HTTP_POST,.handler=tm_post},
        {.uri="/api/v1/outputs",.method=HTTP_POST,.handler=om_post}
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);++i){
        e=httpd_register_uri_handler(server,&routes[i]);
        if(e!=ESP_OK){
            ESP_LOGE(TAG,"CORE HTTP route %s failed: %s",routes[i].uri,esp_err_to_name(e));
            (void)httpd_stop(server);
            return false;
        }
    }
    return true;
}
static bool wifi_ap_init(void) {
    const char *psk=CONFIG_OPENTAIL_AP_PASSWORD;
    size_t n=strlen(psk);
    if(n<8||n>63){ESP_LOGE(TAG,"AP password must be 8-63 characters");return false;}
    esp_err_t e=esp_netif_init();
    if(e==ESP_OK)e=esp_event_loop_create_default();
    if(e!=ESP_OK){
        ESP_LOGE(TAG,"CORE Wi-Fi networking init failed: %s",esp_err_to_name(e));
        return false;
    }
    if(!esp_netif_create_default_wifi_ap()){
        ESP_LOGE(TAG,"CORE Wi-Fi AP interface allocation failed");
        return false;
    }
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    e=esp_wifi_init(&init);
    if(e!=ESP_OK){
        ESP_LOGE(TAG,"CORE Wi-Fi driver init failed: %s",esp_err_to_name(e));
        return false;
    }
    wifi_config_t conf={0};
    strncpy((char*)conf.ap.ssid,OT_AP_SSID,sizeof(conf.ap.ssid)-1);
    conf.ap.ssid_len=strlen(OT_AP_SSID);
    strncpy((char*)conf.ap.password,psk,sizeof(conf.ap.password)-1);
    conf.ap.channel=1;
    conf.ap.max_connection=2;
    conf.ap.authmode=WIFI_AUTH_WPA2_PSK;
    e=esp_wifi_set_mode(WIFI_MODE_AP);
    if(e==ESP_OK)e=esp_wifi_set_config(WIFI_IF_AP,&conf);
    if(e==ESP_OK)e=esp_wifi_start();
    if(e!=ESP_OK){
        ESP_LOGE(TAG,"CORE Wi-Fi AP startup failed: %s",esp_err_to_name(e));
        return false;
    }
    ot_st_wifi_ready();
    ESP_LOGW(TAG,"BENCH ONLY: connect to Wi-Fi %s; browse http://192.168.4.1/",OT_AP_SSID);
    return true;
}
static bool input_init(void) {
    for(int i=0;i<OT_IN_COUNT;++i){
        gpio_config_t cfg={.pin_bit_mask=1ULL<<input_pin[i],.mode=GPIO_MODE_INPUT,
                           .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,
                           .intr_type=GPIO_INTR_DISABLE};
        const esp_err_t e=gpio_config(&cfg);
        if(e!=ESP_OK){
            ESP_LOGE(TAG,"Trigger input IN%u GPIO setup failed: %s",
                     (unsigned)(i+1),esp_err_to_name(e));
            return false;
        }
    }
    return true;
}
/* RMT drivers are created to the SAVED output length, never the board-wide
 * memory safety budget. Resizing is render-task-owned; old pixels are cleared
 * before releasing the old bus so a shorter replacement cannot leave ghosts. */
static bool strip_configure_channel(unsigned ch,unsigned pixels){
    if(ch>=OT_CHAIN_COUNT||pixels<1u||pixels>OT_CHAIN_PIXELS)return false;
    if(strip[ch]&&strip_capacity[ch]==pixels)return true;
    if(strip[ch]){
        /* Never replace a running output unless its old pixels were
         * successfully cleared. A failed clear must not be ignored. */
        esp_err_t e=led_strip_clear(strip[ch]);
        if(e!=ESP_OK){
            ESP_LOGE(TAG,"RMT output %u clear before resize failed: %s",ch,esp_err_to_name(e));
            ot_st_set_fault("external pixel RMT clear failed");return false;
        }
        e=led_strip_del(strip[ch]);
        if(e!=ESP_OK){
            ESP_LOGE(TAG,"RMT output %u teardown failed: %s",ch,esp_err_to_name(e));
            ot_st_set_fault("external pixel RMT teardown failed");return false;
        }
        strip[ch]=NULL;strip_capacity[ch]=0;
    }
    const led_strip_config_t cfg={
        .strip_gpio_num=out_pin[ch],.max_leds=pixels,
        .led_model=LED_MODEL_WS2812, // WS2815-compatible nominal 800 kHz; physical timing remains unverified.
        .color_component_format={.format={.r_pos=1,.g_pos=2,.b_pos=0,.w_pos=3,.num_components=3}},
        .flags={.invert_out=false}
    };
    const led_strip_rmt_config_t rmt={
        .clk_src=RMT_CLK_SRC_DEFAULT,.resolution_hz=10000000,
        .mem_block_symbols=(ch==0?1024:64),
        .flags={.with_dma=(ch==0)}
    };
    led_strip_handle_t next=NULL;
    esp_err_t e=led_strip_new_rmt_device(&cfg,&rmt,&next);
    if(e==ESP_OK)e=led_strip_clear(next);
    if(e!=ESP_OK){
        if(next)(void)led_strip_del(next);
        ESP_LOGE(TAG,"RMT output %u resize to %u pixels failed: %s",ch,pixels,esp_err_to_name(e));
        ot_st_set_fault("external pixel RMT initialization failed");return false;
    }
    strip[ch]=next;strip_capacity[ch]=(uint16_t)pixels;
    return true;
}
static bool strip_init(void){
    for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++)
        if(!strip_configure_channel(ch,om_outputs[ch].active_pixels))return false;
    return true;
}
/* The input sampler is independent of FPS. Four 10ms samples stay 40ms,
 * not four display frames. Wakes pixel rendering after a trigger transition. */
static void input_sampler_task(void *arg){
    (void)arg;
    ot_inputs_t sampler={0};
    TickType_t last_wake=xTaskGetTickCount();
    TickType_t period=pdMS_TO_TICKS(10u);
    if(!period)period=1u;
    unsigned released_samples=0u;
    for(;;){
        bool levels[OT_IN_COUNT];
        uint8_t instantaneous=0;
        for(unsigned i=0;i<OT_IN_COUNT;i++){
            const int level=gpio_get_level(input_pin[i]);
            levels[i]=OT_INPUT_ACTIVE_LOW?(level==0):(level!=0);
            if(levels[i])instantaneous|=(uint8_t)(1u<<i);
        }
        uint8_t physical=ot_debounce(&sampler,levels,OT_DEBOUNCE_TICKS);
        /* Do not promote a saved candidate on the first idle GPIO read
         * after boot or a noisy release: require consecutive clean samples. */
        if(instantaneous||physical)released_samples=0u;
        else if(released_samples<OT_DEBOUNCE_TICKS)released_samples++;
        TaskHandle_t target=NULL;
        portENTER_CRITICAL(&state_lock);
        if(physical!=live_input_mask){live_input_mask=physical;target=render_task_handle;}
        /* This is the only deferred activation point. Do not swap mappings
         * while either a raw pin or its settled level is asserted. */
        if(released_samples>=OT_DEBOUNCE_TICKS){
            if(ot_tm_apply_if_released(&tm_profile,&tm_deferred,
                                       instantaneous,physical))target=render_task_handle;
            if(ot_om_apply_if_released(om_outputs,&om_deferred,
                                       instantaneous,physical))target=render_task_handle;
        }
        portEXIT_CRITICAL(&state_lock);
        if(target)xTaskNotifyGive(target);
        vTaskDelayUntil(&last_wake,period);
    }
}
static void frame_wake_callback(void *arg){
    TaskHandle_t target=(TaskHandle_t)arg;
    if(target)xTaskNotifyGive(target);
}
/* Do not panic/reboot all lighting when one RMT lane fails. The failed handle
 * stays owned by the render task and is not reused or blindly destroyed.
 * Hardware recovery and takeover require a separately verified failover path. */
static void output_runtime_fault(unsigned ch,const char *step,esp_err_t error){
    ESP_LOGE(TAG,"RMT output %u %s failed: %s",ch,step,esp_err_to_name(error));
    ot_st_set_fault("external pixel RMT runtime failure");
    portENTER_CRITICAL(&state_lock);
    native_outputs_ready=false;
    output_runtime_fault_mask|=(1u<<ch);
    portEXIT_CRITICAL(&state_lock);
}
static void led_task(void *p) {
    (void)p;
    bool output_failed[OT_CHAIN_COUNT]={false};
    /* Allocation-free, per-output/per-zone states. One render invocation per zone/frame. */
    static ot_effects_1d_state_t fx_state[OT_CHAIN_COUNT][OT_OM_ZONES];
    static ot_wled_rgb_t fx_frame[OT_CHAIN_COUNT][OT_CHAIN_PIXELS];
    static uint32_t prior_fx_generation[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    /* Retain effect pixels only while the same universal zone is active.
     * Stateful source effects (scanners/trails/particles) need their last frame.
     * Never reuse stale pixels after a zone move, resize or effect replacement. */
    static unsigned prior_fx_offset[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    static unsigned prior_fx_length[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    static bool prior_fx_active[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    ot_fps_pacer_t pacer;
    ot_fps_pacer_init(&pacer);
    /* One source-owned random palette for ALL outputs and zones. */
    ot_wled_palette_random_t global_palette={0};
    if(!ot_wled_palette_global_init(&global_palette,palette_random_byte,NULL)){
        ot_st_set_fault("global palette RNG initialization failed");
        vTaskDelete(NULL);return;
    }
    bool boot_frame_verified=false;
    uint32_t previous_frame_ms=0;
    TaskHandle_t self=xTaskGetCurrentTaskHandle();
    esp_timer_handle_t frame_timer=NULL;
    esp_timer_create_args_t timer_args={
        .callback=frame_wake_callback,.arg=(void*)self,
        .dispatch_method=ESP_TIMER_TASK,.name="ot-frame-pacer"
    };
    if(esp_timer_create(&timer_args,&frame_timer)!=ESP_OK){
        ot_st_set_fault("adaptive frame timer initialization failed");
        portENTER_CRITICAL(&state_lock);
        native_outputs_ready=false;
        portEXIT_CRITICAL(&state_lock);
        vTaskDelete(NULL);
        return;
    }
    portENTER_CRITICAL(&state_lock);
    render_task_handle=self;
    native_outputs_ready=true; /* RMT and frame timer initialized; ready to render. */
    portEXIT_CRITICAL(&state_lock);
    while(1){
       const int64_t frame_started_us=esp_timer_get_time();
       TickType_t now=xTaskGetTickCount();
       uint8_t physical=0;
       ot_demo_t local_demo={0};ot_show_t local_show={0};uint32_t show_started=0;uint8_t combined=0;
       uint32_t local_stop_generation=0;
       bool local_fx_enabled=false;ot_fx_zone_bank_t local_fx_bank={0};
       ot_om_output_t local_outputs[OT_OM_OUTPUTS];ot_tm_profile_t local_tm;
       portENTER_CRITICAL(&state_lock);
       physical=live_input_mask;
       memcpy(local_outputs,om_outputs,sizeof(local_outputs));
       local_tm=tm_profile;
       local_stop_generation=stop_request_generation;
       combined=ot_tm_resolve(&local_tm,physical);
       if(before_deadline(now,sim_deadline)) combined|=sim_mask;
       if(before_deadline(now,demo_deadline)) local_demo=demo;
       if(show_window(now)){
           local_show=native_show;show_started=native_show_started_ms;
           local_fx_enabled=ble_fx_enabled;
           if(local_fx_enabled)memcpy(&local_fx_bank,&fx_bank,sizeof(local_fx_bank));
       } else {native_show.mode=OT_SHOW_OFF;ble_fx_enabled=false;}
       portEXIT_CRITICAL(&state_lock);
       /* One immutable snapshot of every requested custom palette per frame.
        * No locks or NVS access occur in the per-pixel/FX draw loops. */
       uint8_t palette_ids[OT_OM_OUTPUTS*OT_OM_ZONES];
       ot_palette_entry_t frame_palettes[OT_OM_OUTPUTS*OT_OM_ZONES];
       for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
           const uint8_t id=local_fx_bank.zones[o][z].request.palette_id;
           palette_ids[o*OT_OM_ZONES+z]=(id>=73u&&id<=200u)?
                                        (uint8_t)(200u-id):255u;
       }
       ot_palette_runtime_snapshot(palette_ids,OT_OM_OUTPUTS*OT_OM_ZONES,
                                   frame_palettes);
       const ot_arbitration_t decision=ot_arbitrate(combined,local_demo.mode!=OT_DEMO_OFF,local_show.mode!=OT_SHOW_OFF);
       /* Vehicle inputs no longer pause the show engine. Effects render behind
        * per-zone trigger overlays and resume at their current phase. */
       bool native_rendering=decision.show_requested;
       uint16_t longest_pixels=0; /* Only successfully refreshed RMT lanes. */
       ot_st_show_active(native_rendering);
       { /* render every loop; measured FPS reflects actual blocking RMT throughput */
           bool frame_ready[OT_CHAIN_COUNT]={false};
           bool frame_submitted[OT_CHAIN_COUNT]={false};
           bool frame_captured[OT_CHAIN_COUNT]={false};
           /* Reconfigure only after the preceding frame's async RMT transfers
            * have completed, and before starting ANY new lane transfer. */
           for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++){
               if(!output_failed[ch]){
                   frame_ready[ch]=strip_configure_channel(ch,local_outputs[ch].active_pixels);
                   if(!frame_ready[ch]){
                       /* A failed resize may leave an old handle or no handle.
                        * Never retry teardown/allocation on every render frame. */
                       output_runtime_fault(ch,"reconfigure",ESP_FAIL);
                       output_failed[ch]=true;
                   }
               }
               if(!frame_ready[ch])ot_frame_snapshot_unavailable((uint8_t)ch);
           }
           /* A saved mapping can fail to reconfigure its RMT lane at runtime.
            * Keep good lanes rendering, but do not advertise all outputs ready. */
           const bool all_configured=ot_frame_all_submitted(frame_ready,OT_CHAIN_COUNT);
           portENTER_CRITICAL(&state_lock);
           native_outputs_ready=all_configured && output_runtime_fault_mask==0u;
           portEXIT_CRITICAL(&state_lock);
           const uint32_t frame_ms=(uint32_t)(esp_timer_get_time()/1000LL);
           uint32_t elapsed=(uint32_t)(frame_ms-show_started);
           uint32_t frame_delta_ms=previous_frame_ms?(uint32_t)(frame_ms-previous_frame_ms):8u;
           previous_frame_ms=frame_ms;
           if(frame_delta_ms<1u)frame_delta_ms=1u;
           if(frame_delta_ms>65535u)frame_delta_ms=65535u;
           if(native_rendering&&local_fx_enabled &&
              !ot_wled_palette_global_tick(&global_palette,frame_ms,5u,
                    (uint16_t)(frame_delta_ms<8u?8u:frame_delta_ms>1000u?1000u:frame_delta_ms),
                    750u,true,palette_random_byte,NULL))
               ot_st_set_fault("global palette producer failed");
           for(int ch=0;ch<OT_CHAIN_COUNT;++ch){
               if(!frame_ready[ch])continue; /* failed channel is disabled; never write past old capacity */
               const ot_om_output_t *output=&local_outputs[ch];
               ot_layout_t zone_layout;
               if(!ot_om_layout(output,&zone_layout)){
                   ot_st_set_fault("invalid output mapping");
                   frame_ready[ch]=false;
                   ot_frame_snapshot_unavailable((uint8_t)ch);
                   continue; /* No async transfer was submitted. */
               }
               /* Render each selected zone once, even if the native frame is indexed by
                * individual pixels below. No BLE stream and no WLED/Wi-Fi dependency. */
               if(native_rendering && local_fx_enabled){
                   /* Do not clear the entire frame: source-pinned WLED fade_out()
                    * and overlay modes must see the preceding zone frame.
                    * Inactive/replaced/moved zones are cleared individually. */
                   unsigned offset=0;
                   for(int z=0;z<output->zone_count;z++){
                       unsigned len=output->zones[z].pixels;
                       const ot_fx_zone_slot_t *slot=&local_fx_bank.zones[ch][z];
                       if(len && ot_fx_slot_active(slot,frame_ms) &&
                          output->zones[z].role!=OT_OM_OFF &&
                          offset+len<=OT_CHAIN_PIXELS){
                           /* Reset ONLY this zone on replacement. Unrelated zone
                            * particle motion and effect phase are preserved. */
                           if(!prior_fx_active[ch][z] ||
                              prior_fx_generation[ch][z]!=slot->generation ||
                              prior_fx_offset[ch][z]!=offset ||
                              prior_fx_length[ch][z]!=len){
                               memset(&fx_frame[ch][offset],0,
                                      (size_t)len*sizeof(fx_frame[ch][0]));
                               ot_effects_1d_reset(&fx_state[ch][z],
                                   0x1234u+ch*97u+(unsigned)z*19u+slot->generation);
                               prior_fx_generation[ch][z]=slot->generation;
                               prior_fx_offset[ch][z]=offset;
                               prior_fx_length[ch][z]=len;
                           }
                           prior_fx_active[ch][z]=true;
                           const ot_ble_fx_packet_t *fx=&slot->request;
                           ot_wled_fx_config_t cfg={
                               .effect_id=fx->effect_id,
                               .speed=fx->speed,.intensity=fx->intensity,
                               .brightness=fx->brightness,
                               .palette_id=fx->palette_id,.frame_time_ms=(uint16_t)frame_delta_ms,
                               .foreground={fx->r,fx->g,fx->b},
                               .background={fx->bg_r,fx->bg_g,fx->bg_b},
                               .spark_style=slot->spark.style,
                               .spark_peak=slot->spark.peak,
                               .spark_color=slot->spark.color,
                               .resolved_palette=fx->palette_id==1u?
                                   &global_palette.current:
                                   (fx->palette_id>=73u&&fx->palette_id<=200u&&
                                    frame_palettes[ch*OT_OM_ZONES+z].present)?
                                   &frame_palettes[ch*OT_OM_ZONES+z].palette:NULL
                           };
                           uint32_t zone_elapsed=frame_ms-slot->started_ms;
                           if(((fx->palette_id>=73u&&fx->palette_id<=200u)&&
                                 !frame_palettes[ch*OT_OM_ZONES+z].present) ||
                              !ot_effects_1d_render(&cfg,&fx_state[ch][z],zone_elapsed,
                                                &fx_frame[ch][offset],len)){
                               memset(&fx_frame[ch][offset],0,
                                      (size_t)len*sizeof(fx_frame[ch][0]));
                               ot_st_set_fault("invalid BLE effect frame");
                           } else if(slot->overlay.style &&
                                     !ot_fx_overlay_apply(&slot->overlay,
                                         slot->generation ^ ((uint32_t)ch<<16) ^ ((uint32_t)z<<8),
                                         zone_elapsed,&fx_frame[ch][offset],len,cfg.brightness)){
                               ot_st_set_fault("invalid effect overlay");
                           }
                       } else {
                           prior_fx_active[ch][z]=false;
                       }
                       offset+=len;
                   }
               } else {
                   /* Stop, shutdown and show disable invalidate all historical
                    * effect pixels without displaying stale retained frames. */
                   for(int z=0;z<OT_OM_ZONES;z++)prior_fx_active[ch][z]=false;
               }
               /* ot_om_layout() above validated zone totals and ordering.
                * Visit each contiguous zone only once rather than performing
                * a full ot_om_valid() plus name checks for every LED.
                * This keeps the mapping universal and reduces render latency. */
               ot_om_zone_cursor_t zone_cursor={0};
               /* Capture one requested output, after CORE overlays/calibration,
                * before driver color-order packing. No work in normal frames. */
               frame_captured[ch]=ot_frame_snapshot_begin((uint8_t)ch,
                                        output->active_pixels,frame_index+1u);
               bool pixel_write_failed=false;
               for(unsigned px=0;px<output->active_pixels;++px){
                   ot_rgb_t c={0,0,0};
                   int zone=ot_om_section_next(output,&zone_cursor,px);
                   if(zone>=0 && output->zones[zone].role!=OT_OM_OFF){
                       const uint8_t role=output->zones[zone].role;
                       const bool fx_here=native_rendering&&local_fx_enabled&&
                           ot_fx_slot_active(&local_fx_bank.zones[ch][zone],frame_ms);
                       const bool legacy_here=native_rendering&&!local_fx_enabled&&
                           (local_show.chain_mask&(1u<<ch))&&
                           (local_show.section_mask&(1u<<zone));
                       const bool demo_here=!fx_here&&!legacy_here&&
                           local_demo.mode!=OT_DEMO_OFF;
                       const bool decorated=fx_here||legacy_here||demo_here;
                       /* Parking remains a baseline underneath show playback.
                        * Brake/turn/hazard/reverse/AUX win only in their role-owned zones. */
                       bool trigger_here=ot_om_trigger_overlay(role,combined,
                                                 decorated,OT_MAX_BRIGHTNESS,&c);
                       if(!trigger_here){
                           if(fx_here){
                               ot_wled_rgb_t fx=fx_frame[ch][px];
                               const ot_fx_zone_slot_t *slot=&local_fx_bank.zones[ch][zone];
                               const uint16_t progress=ot_transition_progress(
                                   (uint32_t)(frame_ms-slot->started_ms),
                                   OT_FX_ENTRY_FADE_MS,OT_TRANSITION_SMOOTH);
                               const ot_transition_rgb_t black={0,0,0};
                               const ot_transition_rgb_t target={fx.r,fx.g,fx.b};
                               const ot_transition_rgb_t blended=ot_transition_blend(
                                   black,target,progress);
                               c=(ot_rgb_t){blended.r,blended.g,blended.b};
                           }else if(legacy_here){
                               c=ot_show_pixel(&local_show,&zone_layout,ch,px,elapsed);
                           }else if(demo_here){
                               c=ot_pixel_color(&zone_layout,ch,px,0,&local_demo,frame_index);
                           }
                           if(decorated)c=ot_om_calibrate(c,output);
                       }
                   }
                   if(frame_captured[ch])
                       ot_frame_snapshot_pixel((uint16_t)px,c.r,c.g,c.b);
                   c=ot_om_swizzle(c,output->color_order);
                   esp_err_t e=led_strip_set_pixel(strip[ch],px,c.r,c.g,c.b);
                   if(e!=ESP_OK){
                       output_runtime_fault((unsigned)ch,"pixel write",e);
                       output_failed[ch]=true;
                       if(frame_captured[ch])ot_frame_snapshot_complete(false);
                       pixel_write_failed=true;
                       break;
                   }
               }
               if(pixel_write_failed)continue; /* Never transmit a partial pixel buffer. */
               // Hardware driver transmits only this output's configured active length.
               esp_err_t e=led_strip_refresh_async(strip[ch]);
               if(e!=ESP_OK){
                   output_runtime_fault((unsigned)ch,"transfer submit",e);
                   output_failed[ch]=true;
                   if(frame_captured[ch])ot_frame_snapshot_complete(false);
                   continue;
               }
               frame_submitted[ch]=true;
           }
           /* Start both output transfers before waiting, so RMT lanes overlap.
            * Wait for both before reusing their pixel buffers on the next frame. */
           for(int ch=0;ch<OT_CHAIN_COUNT;ch++){
               if(!frame_submitted[ch])continue;
               esp_err_t e=led_strip_refresh_async_done(strip[ch]);
               if(e!=ESP_OK){
                   output_runtime_fault((unsigned)ch,"transfer completion",e);
                   output_failed[ch]=true;
                   frame_submitted[ch]=false; /* No complete-frame/Stop/OTA credit. */
                   if(frame_captured[ch])ot_frame_snapshot_complete(false);
                   continue;
               }
               if(frame_captured[ch])ot_frame_snapshot_complete(true);
               /* A failed lane must not throttle the surviving vehicle lights. */
               if(local_outputs[ch].active_pixels>longest_pixels)
                   longest_pixels=local_outputs[ch].active_pixels;
               portENTER_CRITICAL(&state_lock);
               output_refreshes[ch]++;
               portEXIT_CRITICAL(&state_lock);
           }
           /* Acknowledged Stop and FPS must represent complete multi-output
            * refreshes, not only whichever lane happened to succeed. */
           const bool full_frame=ot_frame_all_submitted(frame_submitted,OT_CHAIN_COUNT);
           /* A created FreeRTOS task does not prove the image can render.
            * Confirm OTA only after every configured RMT lane completes its
            * first frame; startup faults must leave rollback available. */
           if(full_frame && om_write_mutex && ble_startup_ready && http_startup_ready &&
              ot_ble_radio_ever_ready() && !boot_frame_verified)
               boot_frame_verified=ot_ota_confirm_boot(true);
           if(!native_rendering && full_frame){
               portENTER_CRITICAL(&state_lock);
               /* Both software refresh calls completed after this Stop was
                * observed. This cannot establish the actual light state. */
               if(local_stop_generation>stop_render_generation)
                   stop_render_generation=local_stop_generation;
               portEXIT_CRITICAL(&state_lock);
           }
           uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
           if(full_frame)frame_window_frames++;
           else{
               portENTER_CRITICAL(&state_lock);
               incomplete_frames++;
               portEXIT_CRITICAL(&state_lock);
           }
           if(frame_window_start_ms==0)frame_window_start_ms=ms;
           uint32_t span=ms-frame_window_start_ms;
           if(span>=1000u){
               const uint32_t fps=(frame_window_frames*1000u)/span;
               portENTER_CRITICAL(&state_lock);
               measured_fps=fps;
               portEXIT_CRITICAL(&state_lock);
               frame_window_frames=0;frame_window_start_ms=ms;
           }
           portENTER_CRITICAL(&state_lock); frame_index++; portEXIT_CRITICAL(&state_lock);
       }
       /* Observe measured work + RGB800 wire minimum. At overload there is
        * no extra delay; actual FPS may dip below the preferred 53 FPS. */
       int64_t work_elapsed=esp_timer_get_time()-frame_started_us;
       uint32_t work_us=work_elapsed<=0?0u:
           (uint32_t)(work_elapsed>2000000LL?2000000LL:work_elapsed);
       ot_fps_pacer_observe(&pacer,work_us,longest_pixels);
       portENTER_CRITICAL(&state_lock);
       adaptive_target_fps=pacer.target_fps;
       portEXIT_CRITICAL(&state_lock);
       const uint32_t wait_us=ot_fps_pacer_wait_us(&pacer,work_us);
       if(wait_us>=100u){
           if(esp_timer_start_once(frame_timer,wait_us)==ESP_OK){
               /* Input sampler can wake the frame early on a trigger change. */
               const TickType_t timeout=(TickType_t)ot_fps_pacer_wait_ticks(
                   wait_us,configTICK_RATE_HZ);
               (void)ulTaskNotifyTake(pdTRUE,timeout);
               (void)esp_timer_stop(frame_timer);
           }else vTaskDelay(1);
       }else taskYIELD();
    }
}
void app_main(void) {
    ESP_LOGW(TAG,"CORE %s is BENCH-ONLY. No hardware failover. Do not install as road lights.",OT_FW);
    ESP_ERROR_CHECK(nvs_flash_init());
    ot_palette_runtime_load();
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)om_revision[o]=1u;
    om_load();
    tm_load();
    if(!ot_layout_valid(&layout)){ESP_LOGE(TAG,"Invalid LED layout");abort();}
    ot_st_init();
    if(config_recovery_fault)ot_st_set_fault("durable config journal corrupt; mappings fail closed");
    const bool inputs_ready=input_init();
    if(!inputs_ready)ot_st_set_fault("physical trigger GPIO initialization failed");
    const bool sampler_ready=inputs_ready &&
        xTaskCreate(input_sampler_task,"ot_inputs",3072,NULL,9,NULL)==pdPASS;
    if(inputs_ready && !sampler_ready)ot_st_set_fault("physical input sampler could not start");
    input_sampler_ready=sampler_ready; /* Set once before any BLE/HTTP service starts. */
    bool outputs_ready=sampler_ready&&strip_init();
    /* Do not advertise renderer readiness until led_task initializes its timer. */
    const bool wifi_started=wifi_ap_init();
    if(!wifi_started)ot_st_set_fault("Wi-Fi diagnostic AP startup failed");
    om_write_mutex=xSemaphoreCreateMutex();
    /* Configuration writes already reject a missing mutex. Keep local
     * trigger-driven rendering alive rather than rebooting on this fault. */
    if(!om_write_mutex)ot_st_set_fault("Output Manager save mutex allocation failed");
    ble_startup_ready=ot_ble_start();
    if(!ble_startup_ready)ot_st_set_fault("BLE GATT startup failed");
    http_startup_ready=wifi_started&&http_start();
    if(!http_startup_ready)ot_st_set_fault("HTTP diagnostic startup failed");
    if(outputs_ready){
        BaseType_t created=xTaskCreate(led_task,"ot_led",8192,NULL,8,NULL);
        if(created!=pdPASS){portENTER_CRITICAL(&state_lock);native_outputs_ready=false;portEXIT_CRITICAL(&state_lock);ot_st_set_fault("render task could not start");}
        /* OTA validation is deferred until led_task finishes its first full frame. */
    }else ESP_LOGE(TAG,"Pixel task intentionally disabled; CORE is in BENCH FAULT state");
}
