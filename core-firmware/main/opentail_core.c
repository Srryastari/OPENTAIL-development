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
#include "palette_profile.h"
#include "arbitration_logic.h"
#include "output_manager.h"
#include "trigger_manager.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "ot_fps_pacer.h"
#include "ot_frame_contract.h"
#include "ot_frame_snapshot.h"

#define OT_FW OT_FW_VERSION
/* No hard-coded bench-derived FPS: frame rate is constrained by each output's
 * actual pixel length, transfer backend and render processing time. */
#define OT_AP_SSID "OpenTail-CORE-Alpha"
static const char *TAG="OPENTAIL";
/* One hardware-RNG byte source for source-derived global palettes. */
static uint8_t ot_palette_hardware_byte(void *unused){
    (void)unused;
    return (uint8_t)esp_random();
}
extern void ot_ble_start(void);
extern void ot_ble_session_hex(char out[33]);
static const gpio_num_t input_pin[OT_IN_COUNT]=OT_INPUT_GPIO_LIST;
static const gpio_num_t out_pin[OT_CHAIN_COUNT]={OT_OUT_LEFT_GPIO,OT_OUT_RIGHT_GPIO};
static led_strip_handle_t strip[OT_CHAIN_COUNT]={0};
static uint16_t strip_capacity[OT_CHAIN_COUNT]={0}; /* Currently allocated/transmitted pixel count per lane. */
static const ot_layout_t layout={OT_LAYOUT_SECTIONS,OT_CHAIN_PIXELS,OT_MAX_BRIGHTNESS};
static TaskHandle_t render_task_handle=NULL; /* physical sampler wakeups */

static portMUX_TYPE state_lock=portMUX_INITIALIZER_UNLOCKED;
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
static uint32_t stop_request_generation=0,stop_render_generation=0;
/* Single render-task owner of physical outputs; HTTP handlers only modify copied state. */
static ot_show_t native_show={0};
static TickType_t native_show_deadline=0;
static uint32_t native_show_started_ms=0;
static bool native_outputs_ready=false;
static bool ble_owns_show=false;
/* The only FX library is internal to CORE. BLE writes effect IDs, not pixel frames. */
static bool ble_fx_enabled=false;
/* Default remains instant until adjustable transition settings are exposed. */
#define OT_FX_ENTRY_FADE_MS 0u
static ot_fx_zone_bank_t fx_bank={0}; /* Independent CORE output/zone effect assignments */
/* Separate ~6.4 KB palette cache, never packed into output/trigger profiles.
 * All updates and render-task frame-snapshot copies hold state_lock briefly. */
static ot_pal_bank_t pal_bank={0};
static ot_ble_fx_packet_t ble_fx_request={0};

static ot_om_output_t om_outputs[OT_OM_OUTPUTS];
static bool om_persistent=false;
static SemaphoreHandle_t om_write_mutex=NULL;
static uint16_t om_revision[OT_OM_OUTPUTS]={1,1};
/* Trigger profile is stored separately from v0.1.17 Output Manager NVS. */
static ot_tm_profile_t tm_profile;
static bool tm_persistent=false;
typedef struct {
    uint32_t magic, version;
    ot_tm_profile_t profile;
} ot_tm_store_t;
static void tm_load(void){
    ot_tm_defaults(&tm_profile);
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READONLY,&h)!=ESP_OK){tm_persistent=false;return;}
    ot_tm_store_t stored={0};size_t size=sizeof(stored);
    esp_err_t e=nvs_get_blob(h,"profile_v1",&stored,&size);
    nvs_close(h);
    tm_persistent=e==ESP_OK && size==sizeof(stored) &&
        stored.magic==OT_TM_MAGIC && stored.version==OT_TM_VERSION && ot_tm_valid(&stored.profile);
    if(tm_persistent)tm_profile=stored.profile;
    else ESP_LOGI(TAG,"No valid trigger profile; preserving original IN1..IN8 defaults");
}
static bool tm_save(const ot_tm_profile_t *p){
    if(!ot_tm_valid(p))return false;
    ot_tm_store_t stored={.magic=OT_TM_MAGIC,.version=OT_TM_VERSION,.profile=*p};
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,"profile_v1",&stored,sizeof(stored));
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
    ot_om_store_t stored={.magic=OT_OM_STORAGE_MAGIC,.version=OT_OM_VERSION};
    memcpy(stored.outputs,items,sizeof(stored.outputs));
    nvs_handle_t h;
    if(nvs_open("ot_output_cfg",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,"profile_v2",&stored,sizeof(stored));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}

/* Dedicated palette NVS keys; no migrations or writes to legacy profiles. */
static bool pal_key(uint8_t index,char out[8]){
    if(!out||index>=OT_PAL_CUSTOM_SLOTS)return false;
    snprintf(out,8,"pal%03u",(unsigned)index);return true;
}
static bool pal_read_nvs(uint8_t index,ot_pal_profile_t *record){
    char key[8];if(!record||!pal_key(index,key))return false;
    nvs_handle_t h;
    if(nvs_open("ot_palettes",NVS_READONLY,&h)!=ESP_OK)return false;
    uint8_t bytes[OT_PAL_RECORD_BYTES];size_t n=sizeof(bytes);
    const esp_err_t e=nvs_get_blob(h,key,bytes,&n);
    nvs_close(h);
    return e==ESP_OK&&ot_pal_decode(bytes,n,record)&&record->index==index;
}
static void pal_load(void){
    memset(&pal_bank,0,sizeof(pal_bank));
    nvs_handle_t h;
    if(nvs_open("ot_palettes",NVS_READONLY,&h)!=ESP_OK)return;
    unsigned count=0;
    for(unsigned i=0;i<OT_PAL_CUSTOM_SLOTS;i++){
        char key[8];pal_key((uint8_t)i,key);
        uint8_t bytes[OT_PAL_RECORD_BYTES];size_t n=sizeof(bytes);
        const esp_err_t e=nvs_get_blob(h,key,bytes,&n);
        if(e==ESP_ERR_NVS_NOT_FOUND)continue;
        ot_pal_profile_t record;
        if(e!=ESP_OK||!ot_pal_decode(bytes,n,&record)||
           record.index!=i||!ot_pal_bank_put(&pal_bank,&record)){
            ESP_LOGW(TAG,"Ignoring corrupt custom palette slot %u",i);
        }else count++;
    }
    nvs_close(h);
    ESP_LOGI(TAG,"Validated %u custom palettes from separate NVS namespace",count);
}
static bool pal_in_use_locked(uint8_t palette_id,uint32_t now_ms){
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
        for(unsigned z=0;z<OT_OM_ZONES;z++){
            const ot_fx_zone_slot_t *slot=&fx_bank.zones[o][z];
            if(ble_fx_enabled&&slot->request.palette_id==palette_id&&
               ot_fx_slot_active(slot,now_ms))return true;
        }
    return false;
}
/* Called only by authenticated GATT palette transport. 0 OK; 1 format;
 * 3 revision conflict; 6 palette in use; 7 NVS failure. */
int ot_ble_palette_commit(const ot_pal_profile_t *incoming,uint16_t expect){
    if(!incoming)return 1;
    ot_pal_profile_t next=*incoming;
    if(next.index>=OT_PAL_CUSTOM_SLOTS||!ot_pal_validate(&next))return 1;
    uint16_t revision=0;
    const uint8_t id=(uint8_t)(200u-next.index);
    portENTER_CRITICAL(&state_lock);
    (void)ot_pal_bank_get(&pal_bank,id,NULL,&revision);
    const bool busy=pal_in_use_locked(id,(uint32_t)(esp_timer_get_time()/1000LL));
    portEXIT_CRITICAL(&state_lock);
    if(busy)return 6;
    if(revision!=expect||revision==UINT16_MAX)return 3;
    next.revision=(uint16_t)(revision+1u);
    uint8_t bytes[OT_PAL_RECORD_BYTES];
    if(!ot_pal_encode(&next,bytes))return 1;
    char key[8];pal_key(next.index,key);
    nvs_handle_t h;
    if(nvs_open("ot_palettes",NVS_READWRITE,&h)!=ESP_OK)return 7;
    esp_err_t e=nvs_set_blob(h,key,bytes,sizeof(bytes));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    if(e!=ESP_OK)return 7; /* RAM cache not touched on NVS failure. */
    portENTER_CRITICAL(&state_lock);
    (void)ot_pal_bank_put(&pal_bank,&next);
    portEXIT_CRITICAL(&state_lock);
    return 0;
}
int ot_ble_palette_delete(uint8_t index,uint16_t expected){
    char key[8];if(!pal_key(index,key))return 1;
    const uint8_t id=(uint8_t)(200u-index);
    uint16_t revision=0;
    portENTER_CRITICAL(&state_lock);
    const bool exists=ot_pal_bank_get(&pal_bank,id,NULL,&revision);
    const bool busy=pal_in_use_locked(id,(uint32_t)(esp_timer_get_time()/1000LL));
    portEXIT_CRITICAL(&state_lock);
    if(!exists)return 4;
    if(busy)return 6;
    if(expected!=revision)return 3;
    nvs_handle_t h;
    if(nvs_open("ot_palettes",NVS_READWRITE,&h)!=ESP_OK)return 7;
    esp_err_t e=nvs_erase_key(h,key);
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    if(e!=ESP_OK)return 7;
    portENTER_CRITICAL(&state_lock);
    (void)ot_pal_bank_remove(&pal_bank,index);
    portEXIT_CRITICAL(&state_lock);
    return 0;
}
bool ot_ble_palette_get(uint8_t index,ot_pal_profile_t *record){
    return pal_read_nvs(index,record);
}

#define OT_SHOW_MAX_MS 60000u


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
static uint8_t signal_mask_of(const char *name) {
    if(!strcmp(name,"park")) return OT_SIG_PARK;
    if(!strcmp(name,"brake")) return OT_SIG_BRAKE;
    if(!strcmp(name,"left")) return OT_SIG_LEFT;
    if(!strcmp(name,"right")) return OT_SIG_RIGHT;
    if(!strcmp(name,"reverse")) return OT_SIG_REVERSE;
    return 0;
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
    cJSON_AddNumberToObject(item,"index",id);
    cJSON_AddNumberToObject(item,"pin",(int)out_pin[id]);
    cJSON_AddStringToObject(item,"name",o->name);
    cJSON_AddNumberToObject(item,"pixels",o->active_pixels);
    cJSON_AddNumberToObject(item,"zone_count",o->zone_count);
    cJSON *cal=cJSON_AddObjectToObject(item,"calibration");
    cJSON *rgb=cJSON_AddArrayToObject(cal,"rgb_gain");
    cJSON *w=cJSON_AddArrayToObject(cal,"white_gain");
    for(int k=0;k<3;k++){cJSON_AddItemToArray(rgb,cJSON_CreateNumber(o->rgb_gain[k]));cJSON_AddItemToArray(w,cJSON_CreateNumber(o->white_gain[k]));}
    cJSON_AddBoolToObject(cal,"gamma_enabled",o->gamma_enabled);
    cJSON_AddStringToObject(item,"color_order",ot_om_order_name(o->color_order));
    cJSON *zones=cJSON_AddArrayToObject(item,"zones");
    for(int i=0;i<o->zone_count;i++){
        const ot_om_zone_t *z=&o->zones[i];
        cJSON *entry=cJSON_CreateObject();
        cJSON_AddStringToObject(entry,"name",z->name);
        cJSON_AddNumberToObject(entry,"pixels",z->pixels);
        cJSON_AddStringToObject(entry,"role",ot_om_role_name(z->role));
        const bool show_here=(show_zones&(1u<<i))!=0;
        ot_rgb_t overlay={0,0,0};
        bool trigger_here=ot_om_trigger_overlay(z->role,mask,show_here,
                                                 OT_MAX_BRIGHTNESS,&overlay);
        cJSON_AddBoolToObject(entry,"trigger_active",trigger_here);
        cJSON_AddBoolToObject(entry,"effect_selected",show_here);
        cJSON_AddItemToArray(zones,entry);
    }
    return item;
}
static esp_err_t om_get(httpd_req_t *r){
    ot_om_output_t copy[OT_OM_OUTPUTS];ot_tm_profile_t tm;bool persistent;uint8_t mask;
    uint8_t show_zones[OT_OM_OUTPUTS]={0};
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    memcpy(copy,om_outputs,sizeof(copy));persistent=om_persistent;tm=tm_profile;
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
    cJSON_AddNumberToObject(obj,"revision",OT_OM_VERSION);
    cJSON_AddBoolToObject(obj,"saved_on_device",persistent);
    cJSON_AddBoolToObject(obj,"bench_only",true);
    cJSON_AddNumberToObject(obj,"physical_outputs",OT_CHAIN_COUNT);
    cJSON_AddNumberToObject(obj,"future_outputs_target",12);
    /* Hardware build budget, not an active pixel count or a test-derived maximum. */
    cJSON_AddNumberToObject(obj,"pixel_buffer_budget_per_output",OT_CHAIN_PIXELS);
    cJSON_AddNumberToObject(obj,"max_configurable_pixels_per_output",OT_OM_MAX_PIXELS_PER_OUTPUT);
    cJSON_AddNumberToObject(obj,"max_total_pixels",OT_OM_MAX_TOTAL_PIXELS);
    cJSON_AddNumberToObject(obj,"max_zones_per_output",OT_OM_ZONES);
    cJSON_AddBoolToObject(obj,"dynamic_zones",true);
    cJSON_AddNumberToObject(obj,"effective_mask",mask);
    cJSON *outputs=cJSON_AddArrayToObject(obj,"outputs");
    for(int i=0;i<OT_OM_OUTPUTS;i++)cJSON_AddItemToArray(outputs,om_json_output(&copy[i],i,mask,show_zones[i]));
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
    if(!om_write_mutex || xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)
        return respond_err(r,409,"output mapping save busy");
    TickType_t now=xTaskGetTickCount();
    ot_om_output_t next_all[OT_OM_OUTPUTS];
    portENTER_CRITICAL(&state_lock);
    bool busy=live_input_mask || (before_deadline(now,sim_deadline)&&sim_mask) ||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) || show_window(now);
    memcpy(next_all,om_outputs,sizeof(next_all));
    portEXIT_CRITICAL(&state_lock);
    if(busy){xSemaphoreGive(om_write_mutex);return respond_err(r,409,"stop shows and release all 12V triggers before saving output mapping");}
    next_all[index]=next;
    /* No in-memory mutation if NVS commit fails. */
    if(!om_save(next_all)){xSemaphoreGive(om_write_mutex);return respond_err(r,500,"failed to save output mapping in flash");}
    portENTER_CRITICAL(&state_lock);
    memcpy(om_outputs,next_all,sizeof(om_outputs));om_persistent=true;om_revision[index]++;
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
    if(!om_write_mutex || xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)return 2; /* busy */
    TickType_t now=xTaskGetTickCount();ot_om_output_t next_all[OT_OM_OUTPUTS];
    portENTER_CRITICAL(&state_lock);
    bool busy=live_input_mask || (before_deadline(now,sim_deadline)&&sim_mask) ||
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
            memcpy(om_outputs,next_all,sizeof(om_outputs));om_persistent=true;om_revision[index]++;
            portEXIT_CRITICAL(&state_lock);
        }
    }
    xSemaphoreGive(om_write_mutex);
    return result;
}

/* Universal Trigger Manager v1: independent physical-input-to-function profile.
 * Simulated signals remain logical functions, not raw GPIO assignments.
 */
static esp_err_t tm_get(httpd_req_t *r){
    ot_tm_profile_t cfg;bool saved;uint8_t physical,sim;
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    cfg=tm_profile;saved=tm_persistent;physical=live_input_mask;
    sim=before_deadline(now,sim_deadline)?sim_mask:0;
    portEXIT_CRITICAL(&state_lock);
    cJSON *root=cJSON_CreateObject();if(!root)return respond_err(r,500,"out of memory");
    cJSON_AddNumberToObject(root,"revision",OT_TM_VERSION);
    cJSON_AddBoolToObject(root,"saved_on_device",saved);
    cJSON_AddBoolToObject(root,"bench_only",true);
    cJSON_AddNumberToObject(root,"physical_inputs",OT_IN_COUNT);
    cJSON_AddBoolToObject(root,"fixed_active_low",OT_INPUT_ACTIVE_LOW!=0);
    cJSON_AddNumberToObject(root,"raw_input_mask",physical);
    cJSON_AddNumberToObject(root,"physical_function_mask",ot_tm_resolve(&cfg,physical));
    cJSON_AddNumberToObject(root,"effective_mask",ot_tm_resolve(&cfg,physical)|sim);
    cJSON *supported=cJSON_AddArrayToObject(root,"supported_functions");
    for(int k=0;k<=OT_TM_AUX3;k++)cJSON_AddItemToArray(supported,cJSON_CreateString(ot_tm_name(k)));
    cJSON *arr=cJSON_AddArrayToObject(root,"inputs");
    for(int i=0;i<OT_IN_COUNT;i++){
        cJSON *v=cJSON_CreateObject();
        cJSON_AddNumberToObject(v,"index",i+1);
        cJSON_AddNumberToObject(v,"gpio",(int)input_pin[i]);
        cJSON_AddStringToObject(v,"function",ot_tm_name(cfg.function[i]));
        cJSON_AddBoolToObject(v,"active",(physical&(1u<<i))!=0);
        cJSON_AddItemToArray(arr,v);
    }
    char *body=cJSON_PrintUnformatted(root);cJSON_Delete(root);
    if(!body)return respond_err(r,500,"out of memory");
    esp_err_t ret=send_json(r,body);free(body);return ret;
}
static esp_err_t tm_post(httpd_req_t *r){
    cJSON *root=NULL;int status=recv_json(r,&root);
    if(status!=200){cJSON_Delete(root);return respond_err(r,status,"invalid JSON request");}
    unsigned revision=0;
    const cJSON *arr=cJSON_GetObjectItemCaseSensitive(root,"inputs");
    bool valid=om_number(root,"revision",&revision,OT_TM_VERSION,OT_TM_VERSION) &&
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
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    bool busy=live_input_mask!=0 || (before_deadline(now,sim_deadline)&&sim_mask) ||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) || show_window(now);
    portEXIT_CRITICAL(&state_lock);
    if(busy)return respond_err(r,409,"stop shows and release all 12V triggers before saving trigger mapping");
    /* Commit first; previous profile is unchanged on error. */
    if(!tm_save(&next))return respond_err(r,500,"failed to save trigger mapping in flash");
    portENTER_CRITICAL(&state_lock);
    tm_profile=next;tm_persistent=true;
    portEXIT_CRITICAL(&state_lock);
    return tm_get(r);
}
static esp_err_t show_get(httpd_req_t *r){
    TickType_t now=xTaskGetTickCount();
    ot_show_t cfg;bool active;uint32_t remaining;bool ready;
    bool fx_active=false;uint16_t fx_effect_id=0;
    portENTER_CRITICAL(&state_lock);
    cfg=native_show;active=show_window(now);
    if(active && ble_fx_enabled){fx_active=true;fx_effect_id=ble_fx_request.effect_id;}
    remaining=active?(uint32_t)((uint32_t)(native_show_deadline-now)*portTICK_PERIOD_MS):0;
    ready=native_outputs_ready;
    portEXIT_CRITICAL(&state_lock);
    ot_wled_fx_metadata_t meta;
    const char *active_name=fx_active&&ot_effects_1d_metadata(fx_effect_id,&meta)?
      meta.name:ot_show_name(cfg.mode);
    char buf[390];
    snprintf(buf,sizeof(buf),"{\"active\":%s,\"effect\":\"%s\",\"remaining_ms\":%lu,\"brightness\":%u,\"speed\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"chain_mask\":%u,\"section_mask\":%u,\"bench_only\":true,\"outputs_ready\":%s}",
      active?"true":"false",active?active_name:"off",(unsigned long)remaining,
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
    return send_json(r,"{\"ok\":true,\"active\":true,\"bench_only\":true,\"message\":\"native effect started on configured bench outputs; real trigger inputs always override\"}");
}
static esp_err_t show_stop(httpd_req_t *r){
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    ble_owns_show=false;ble_fx_enabled=false;ot_fx_bank_stop_all(&fx_bank);native_show.mode=OT_SHOW_OFF;native_show_deadline=now;
    stop_request_generation++;
    portEXIT_CRITICAL(&state_lock);
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
    return ready;
}
/* Runtime control over BLE; all pixels are computed on CORE, never streamed by the app.
 * Temporary bench effects expire in 60 seconds; vehicle triggers overlay assigned zones.
 */
static bool ot_ble_apply_fx_internal(const ot_ble_fx_packet_t *req,bool standalone,
                                      const ot_fx_spark_style_t *spark,
                                      const ot_fx_overlay_config_t *overlay){
    ot_wled_fx_metadata_t meta;
    if((overlay&&!ot_fx_overlay_valid(overlay))||!req||
       !ot_effects_1d_metadata(req->effect_id,&meta)||
       !(ot_palette_1d_supported(req->effect_id,req->palette_id)||
         ot_palette_1d_external_supported(req->effect_id,req->palette_id))||
       req->chain_mask==0||
       (req->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))!=0||req->zone_mask==0||
       req->brightness>OT_MAX_BRIGHTNESS||req->ttl_sec<1||req->ttl_sec>60)
        return false;
    const TickType_t now=xTaskGetTickCount();
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool ready;
    portENTER_CRITICAL(&state_lock);
    ready=native_outputs_ready;
    if(ready&&ot_palette_1d_external_supported(req->effect_id,req->palette_id))
        ready=ot_pal_bank_get(&pal_bank,req->palette_id,NULL,NULL);
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
        if(ready&&overlay){
            /* Engine overlay is volatile, per zone, and never stored in NVS. */
            for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
                if(!(req->chain_mask&(1u<<o)))continue;
                for(unsigned z=0;z<om_outputs[o].zone_count;z++){
                    if(!(req->zone_mask&(1u<<z))||
                       om_outputs[o].zones[z].pixels==0||
                       om_outputs[o].zones[z].role==OT_OM_OFF)continue;
                    fx_bank.zones[o][z].overlay=*overlay;
                }
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
    /* Optional RAM-only OpenTail overlay parameters apply to ANY unified FX,
     * including Embers, without changing IDs, stored presets or BLE FX ABI.
     * The bench endpoint is intentionally unavailable on production networks. */
    unsigned ov_style=0u,ov_peak=32u,ov_density=160u,ov_radius=4u;
    unsigned ov_fade=120u,ov_r=255u,ov_g=255u,ov_b=255u;
    struct {const char *key;unsigned *value;unsigned limit;} options[]={
       {"overlay_style",&ov_style,3u},{"overlay_peak",&ov_peak,32u},
       {"overlay_density",&ov_density,255u},{"overlay_radius",&ov_radius,12u},
       {"overlay_fade_ms",&ov_fade,1000u},{"overlay_r",&ov_r,255u},
       {"overlay_g",&ov_g,255u},{"overlay_b",&ov_b,255u}
    };
    for(unsigned k=0;k<sizeof(options)/sizeof(options[0]);k++){
        cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,options[k].key);
        if(!v)continue;
        if(!cJSON_IsNumber(v)||v->valuedouble!=v->valueint||
           v->valueint<0||(unsigned)v->valueint>options[k].limit){
            cJSON_Delete(obj);
            return respond_err(r,400,"invalid overlay parameter");
        }
        *options[k].value=(unsigned)v->valueint;
    }
    const ot_fx_overlay_config_t overlay={
       .style=(uint8_t)ov_style,.peak=(uint8_t)ov_peak,
       .density=(uint8_t)ov_density,.radius=(uint8_t)ov_radius,
       .fade_ms=(uint16_t)ov_fade,.color={(uint8_t)ov_r,(uint8_t)ov_g,(uint8_t)ov_b}
    };
    if(!ot_fx_overlay_valid(&overlay)){
       cJSON_Delete(obj);return respond_err(r,400,"invalid overlay fade or radius");
    }
    const cJSON *only=cJSON_GetObjectItemCaseSensitive(obj,"overlay_only");
    if(only&&!cJSON_IsBool(only)){
       cJSON_Delete(obj);return respond_err(r,400,"overlay_only must be boolean");
    }
    const bool overlay_only=cJSON_IsTrue(only);
    cJSON_Delete(obj);
    if(overlay_only){
       /* Modify the active RAM-only overlay without replacing the effect.
        * No generation, elapsed time, timeout, NVS or arbitration changes.
        * Only the isolated bench AP exposes this developer preview. */
       const TickType_t now=xTaskGetTickCount();
       const uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
       bool changed=false;
       portENTER_CRITICAL(&state_lock);
       if(native_outputs_ready&&ble_fx_enabled&&show_window(now))
           changed=ot_fx_bank_set_overlay(&fx_bank,om_outputs,
                      req.chain_mask,req.zone_mask,ms,&overlay);
       portEXIT_CRITICAL(&state_lock);
       if(!changed)return respond_err(r,409,"no active eligible effect zone");
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
    bool stopped=false;
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
        stopped=true;
    }else if(native_show.mode==OT_SHOW_OFF || !show_window(now)){
        /* Stop may be pressed after the 60-second FX timeout. Record the
         * requested post-stop refresh even when no show is still owned. */
        stop_request_generation++;
    }
    portEXIT_CRITICAL(&state_lock);
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
         if(ot_palette_1d_external_supported(p->effect_id,p->palette_id)&&
            !ot_pal_bank_get(&pal_bank,p->palette_id,NULL,NULL)){
             ready=false;break; /* Missing custom palette: scene stays unchanged. */
         }
         if(!ot_fx_bank_apply_configured(&replacement,p,om_outputs,now_ms,true)){
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
 ESP_LOGI(TAG,"BLE disconnect cleanup: owned=%d->%d enabled=%d->%d active_after=%d",
          owned_before,owned_after,enabled_before,enabled_after,active_after);
}

static esp_err_t status_get(httpd_req_t *r) {
    TickType_t now=xTaskGetTickCount();
    uint8_t physical,sim;ot_tm_profile_t tm;bool sim_active,demo_active,show_active;ot_demo_mode_t mode;uint32_t frames;uint16_t target_fps;
    uint32_t refresh0,refresh1,stop_requested,stop_rendered,partial_frames,observed_fps;
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

    refresh0=output_refreshes[0];refresh1=output_refreshes[1];
    for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++)configured_pixels[ch]=om_outputs[ch].active_pixels;
    stop_requested=stop_request_generation;stop_rendered=stop_render_generation;
    partial_frames=incomplete_frames;
    portEXIT_CRITICAL(&state_lock);
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
    char buf[1800];char led_json[384];ot_st_format_json(led_json,sizeof(led_json));
    snprintf(buf,sizeof(buf),"{\"name\":\"OpenTail CORE Alpha\",\"version\":\"%s\",\"bench_only\":true,\"hardware_failover\":false,\"mode\":\"%s\",\"input_mask\":%u,\"simulated_mask\":%u,\"physical_function_mask\":%u,\"effective_mask\":%u,\"demo_active\":%s,\"native_show_active\":%s,\"frames\":%lu,\"measured_fps\":%lu,\"target_fps\":%u,\"active_pixels_per_output\":%s,\"leds_per_chain\":%u,\"brightness_cap\":%u,\"output_refreshes\":[%lu,%lu],\"incomplete_frames\":%lu,\"stop_requested\":%lu,\"stop_rendered\":%lu,\"status_led\":%s,\"arbitration\":%s}",
       OT_FW,"user-configured",
       (unsigned)physical,(unsigned)sim,(unsigned)function_mask,(unsigned)effective,mode==OT_DEMO_OFF?"false":"true",show_active?"true":"false",
       (unsigned long)frames,(unsigned long)observed_fps,(unsigned)target_fps,active_pixels_json,(unsigned)configured_pixels[0],(unsigned)layout.brightness_cap,
       (unsigned long)refresh0,(unsigned long)refresh1,(unsigned long)partial_frames,
       (unsigned long)stop_requested,(unsigned long)stop_rendered,led_json,arb_json);
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
       if(!cJSON_IsString(v)||!signal_mask_of(v->valuestring)){
          cJSON_Delete(obj);return respond_err(r,400,"unrecognized signal");
       }
       mask|=signal_mask_of(v->valuestring);
    }
    unsigned ttl=bounded_int(obj,"ttl_ms",2000,100,OT_DEMO_MAX_MS);
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    sim_mask=mask;
    sim_deadline=mask?now+pdMS_TO_TICKS(ttl):now;
    portEXIT_CRITICAL(&state_lock);
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
static void http_start(void) {
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.max_uri_handlers=30;cfg.stack_size=8192;cfg.recv_wait_timeout=30;
    httpd_handle_t server=NULL;
    ESP_ERROR_CHECK(httpd_start(&server,&cfg));
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
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);++i) ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}
static void wifi_ap_init(void) {
    const char *psk=CONFIG_OPENTAIL_AP_PASSWORD;
    size_t n=strlen(psk);
    if(n<8||n>63){ESP_LOGE(TAG,"AP password must be 8-63 characters");abort();}
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    wifi_config_t conf={0};
    strncpy((char*)conf.ap.ssid,OT_AP_SSID,sizeof(conf.ap.ssid)-1);
    conf.ap.ssid_len=strlen(OT_AP_SSID);
    strncpy((char*)conf.ap.password,psk,sizeof(conf.ap.password)-1);
    conf.ap.channel=1;
    conf.ap.max_connection=2;
    conf.ap.authmode=WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&conf));
    ESP_ERROR_CHECK(esp_wifi_start());
    ot_st_wifi_ready();
    ESP_LOGW(TAG,"BENCH ONLY: connect to Wi-Fi %s; browse http://192.168.4.1/",OT_AP_SSID);
}
static void input_init(void) {
    for(int i=0;i<OT_IN_COUNT;++i){
        gpio_config_t cfg={.pin_bit_mask=1ULL<<input_pin[i],.mode=GPIO_MODE_INPUT,
                           .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,
                           .intr_type=GPIO_INTR_DISABLE};
        ESP_ERROR_CHECK(gpio_config(&cfg));
    }
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
    for(;;){
        bool levels[OT_IN_COUNT];
        for(unsigned i=0;i<OT_IN_COUNT;i++){
            const int level=gpio_get_level(input_pin[i]);
            levels[i]=OT_INPUT_ACTIVE_LOW?(level==0):(level!=0);
        }
        uint8_t physical=ot_debounce(&sampler,levels,OT_DEBOUNCE_TICKS);
        TaskHandle_t target=NULL;
        portENTER_CRITICAL(&state_lock);
        if(physical!=live_input_mask){live_input_mask=physical;target=render_task_handle;}
        portEXIT_CRITICAL(&state_lock);
        if(target)xTaskNotifyGive(target);
        vTaskDelayUntil(&last_wake,period);
    }
}
static void frame_wake_callback(void *arg){
    TaskHandle_t target=(TaskHandle_t)arg;
    if(target)xTaskNotifyGive(target);
}
static void led_task(void *p) {
    (void)p;
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
    /* Immutable color snapshots for one complete frame. Loaded under the
     * same lock as zone assignments, never during effect rendering. */
    static ot_wled_palette16_t custom_frame[OT_CHAIN_COUNT][OT_OM_ZONES];
    static bool custom_frame_valid[OT_CHAIN_COUNT][OT_OM_ZONES];
    ot_fps_pacer_t pacer;
    ot_fps_pacer_init(&pacer);
    /* WLED has exactly ONE random palette shared across active segments.
     * Generate the two source boot palettes, then use the pinned default
     * harmonic source mode on subsequent once-per-frame transitions.
     * Hardware RNG draws are never performed independently per zone. */
    ot_wled_palette_random_t global_palette={0};
    if(!ot_wled_palette_global_init(&global_palette,
                                   ot_palette_hardware_byte,NULL)){
        ot_st_set_fault("global effect palette RNG initialization failed");
        vTaskDelete(NULL);
        return;
    }
    uint32_t fx_perf_window_ms=0,fx_perf_frames=0,fx_perf_max_us=0;
    uint64_t fx_perf_total_us=0;
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
           if(local_fx_enabled){
               memcpy(&local_fx_bank,&fx_bank,sizeof(local_fx_bank));
               for(unsigned o=0;o<OT_CHAIN_COUNT;o++)
                   for(unsigned z=0;z<OT_OM_ZONES;z++){
                       const ot_ble_fx_packet_t *request=&local_fx_bank.zones[o][z].request;
                       custom_frame_valid[o][z]=
                         ot_palette_1d_external_supported(request->effect_id,request->palette_id)&&
                         ot_pal_bank_get(&pal_bank,request->palette_id,
                                          &custom_frame[o][z],NULL);
                   }
           }
       } else {native_show.mode=OT_SHOW_OFF;ble_fx_enabled=false;}
       portEXIT_CRITICAL(&state_lock);
       const ot_arbitration_t decision=ot_arbitrate(combined,local_demo.mode!=OT_DEMO_OFF,local_show.mode!=OT_SHOW_OFF);
       /* Vehicle inputs no longer pause the show engine. Effects render behind
        * per-zone trigger overlays and resume at their current phase. */
       bool native_rendering=decision.show_requested;
       ot_st_show_active(native_rendering);
       { /* render every loop; measured FPS reflects actual blocking RMT throughput */
           bool frame_ready[OT_CHAIN_COUNT]={false};
           bool frame_submitted[OT_CHAIN_COUNT]={false};
           bool frame_captured[OT_CHAIN_COUNT]={false};
           /* Reconfigure only after the preceding frame's async RMT transfers
            * have completed, and before starting ANY new lane transfer. */
           for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++){
               frame_ready[ch]=strip_configure_channel(ch,local_outputs[ch].active_pixels);
               if(!frame_ready[ch])ot_frame_snapshot_unavailable((uint8_t)ch);
           }
           const uint32_t frame_ms=(uint32_t)(esp_timer_get_time()/1000LL);
           uint32_t elapsed=(uint32_t)(frame_ms-show_started);
           uint32_t frame_delta_ms=previous_frame_ms?(uint32_t)(frame_ms-previous_frame_ms):8u;
           previous_frame_ms=frame_ms;
           if(frame_delta_ms<1u)frame_delta_ms=1u;
           if(frame_delta_ms>65535u)frame_delta_ms=65535u;
           /* Advance shared random palette once BEFORE either lane/zone
            * renders. Never advance it from an individual FX zone. */
           if(native_rendering && local_fx_enabled){
               const uint16_t source_frame_ms=(uint16_t)(
                   frame_delta_ms<8u?8u:frame_delta_ms>1000u?1000u:frame_delta_ms);
               if(!ot_wled_palette_global_tick(&global_palette,frame_ms,
                     5u,source_frame_ms,750u,true,ot_palette_hardware_byte,NULL))
                   ot_st_set_fault("global effect palette advance failed");
           }
           const int64_t fx_started_us=esp_timer_get_time();
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
                               /* This frame-stable snapshot is shared by
                                * every zone, regardless of output length. */
                               .resolved_palette=fx->palette_id==1u?
                                      &global_palette.current:
                                      custom_frame_valid[ch][z]?
                                      &custom_frame[ch][z]:NULL
                           };
                           uint32_t zone_elapsed=frame_ms-slot->started_ms;
                           if(!ot_effects_1d_render(&cfg,&fx_state[ch][z],zone_elapsed,
                                                &fx_frame[ch][offset],len)){
                               memset(&fx_frame[ch][offset],0,
                                      (size_t)len*sizeof(fx_frame[ch][0]));
                               ot_st_set_fault("invalid BLE effect frame");
                           } else if(slot->overlay.style &&
                                     !ot_fx_overlay_apply(&slot->overlay,
                                         slot->generation ^ ((uint32_t)ch<<16) ^ ((uint32_t)z<<8),
                                         zone_elapsed,&fx_frame[ch][offset],len,cfg.brightness)){
                               ot_st_set_fault("invalid flare/spark overlay");
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
                   ESP_ERROR_CHECK(led_strip_set_pixel(strip[ch],px,c.r,c.g,c.b));
               }
               // Hardware driver transmits only this output's configured active length.
               ESP_ERROR_CHECK(led_strip_refresh_async(strip[ch]));
               frame_submitted[ch]=true;
           }
           /* Start both output transfers before waiting, so RMT lanes overlap.
            * Wait for both before reusing their pixel buffers on the next frame. */
           for(int ch=0;ch<OT_CHAIN_COUNT;ch++){
               if(!frame_submitted[ch])continue;
               ESP_ERROR_CHECK(led_strip_refresh_async_done(strip[ch]));
               if(frame_captured[ch])ot_frame_snapshot_complete(true);
               portENTER_CRITICAL(&state_lock);
               output_refreshes[ch]++;
               portEXIT_CRITICAL(&state_lock);
           }
           /* Measure wall-time for the active effects/output pipeline,
            * INCLUDING RMT completion waits. It is not pure CPU FX time and
            * cannot establish optical LED output. */
           if(native_rendering && local_fx_enabled){
               const int64_t fx_elapsed=esp_timer_get_time()-fx_started_us;
               const uint32_t us=fx_elapsed<0?0u:
                   (uint32_t)(fx_elapsed>2000000LL?2000000LL:fx_elapsed);
               fx_perf_total_us+=us;fx_perf_frames++;
               if(us>fx_perf_max_us)fx_perf_max_us=us;
           }
           /* Acknowledged Stop and FPS must represent complete multi-output
            * refreshes, not only whichever lane happened to succeed. */
           const bool full_frame=ot_frame_all_submitted(frame_submitted,OT_CHAIN_COUNT);
           /* A created FreeRTOS task does not prove the image can render.
            * Confirm OTA only after every configured RMT lane completes its
            * first frame; startup faults must leave rollback available. */
           if(full_frame && !boot_frame_verified)
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
       /* Per-five-second CORE serial evidence. Idle or incomplete hardware
        * frames cannot be described as an LED optical validation. */
       const uint32_t perf_now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
       if(!fx_perf_window_ms)fx_perf_window_ms=perf_now_ms;
       if((uint32_t)(perf_now_ms-fx_perf_window_ms)>=5000u){
           if(fx_perf_frames){
               const uint32_t mean=(uint32_t)(fx_perf_total_us/fx_perf_frames);
               ESP_LOGI(TAG,"FX pipeline (incl. RMT): mean_us=%lu max_us=%lu "
                   "samples=%lu internal_free=%u internal_min_free=%u "
                   "observed_full_frame_fps=%lu",
                   (unsigned long)mean,(unsigned long)fx_perf_max_us,
                   (unsigned long)fx_perf_frames,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                   (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                   (unsigned long)measured_fps);
           }
           fx_perf_frames=0;fx_perf_max_us=0;fx_perf_total_us=0;
           fx_perf_window_ms=perf_now_ms;
       }
       int64_t work_elapsed=esp_timer_get_time()-frame_started_us;
       uint32_t work_us=work_elapsed<=0?0u:
           (uint32_t)(work_elapsed>2000000LL?2000000LL:work_elapsed);
       uint16_t longest_pixels=0;
       for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++)
           if(local_outputs[ch].active_pixels>longest_pixels)
               longest_pixels=local_outputs[ch].active_pixels;
       ot_fps_pacer_observe(&pacer,work_us,longest_pixels);
       portENTER_CRITICAL(&state_lock);
       adaptive_target_fps=pacer.target_fps;
       portEXIT_CRITICAL(&state_lock);
       const uint32_t wait_us=ot_fps_pacer_wait_us(&pacer,work_us);
       if(wait_us>=100u){
           if(esp_timer_start_once(frame_timer,wait_us)==ESP_OK){
               /* Input sampler can wake the frame early on a trigger change. */
               (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(30u));
               (void)esp_timer_stop(frame_timer);
           }else vTaskDelay(1);
       }else taskYIELD();
    }
}
void app_main(void) {
    ESP_LOGW(TAG,"CORE %s is BENCH-ONLY. No hardware failover. Do not install as road lights.",OT_FW);
    ESP_ERROR_CHECK(nvs_flash_init());
    om_load();
    tm_load();
    pal_load();
    if(!ot_layout_valid(&layout)){ESP_LOGE(TAG,"Invalid LED layout");abort();}
    ot_st_init();
    input_init();
    const bool sampler_ready=xTaskCreate(input_sampler_task,"ot_inputs",3072,NULL,9,NULL)==pdPASS;
    if(!sampler_ready)ot_st_set_fault("physical input sampler could not start");
    bool outputs_ready=sampler_ready&&strip_init();
    /* Do not advertise renderer readiness until led_task initializes its timer. */
    wifi_ap_init();
    om_write_mutex=xSemaphoreCreateMutex();
    if(!om_write_mutex){ESP_LOGE(TAG,"Output Manager save mutex allocation failed");abort();}
    ot_ble_start();
    http_start();
    if(outputs_ready){
        BaseType_t created=xTaskCreate(led_task,"ot_led",8192,NULL,8,NULL);
        if(created!=pdPASS){portENTER_CRITICAL(&state_lock);native_outputs_ready=false;portEXIT_CRITICAL(&state_lock);ot_st_set_fault("render task could not start");}
        /* OTA validation is deferred until led_task finishes its first full frame. */
    }else ESP_LOGE(TAG,"Pixel task intentionally disabled; CORE is in BENCH FAULT state");
}
