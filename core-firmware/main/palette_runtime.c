#include "palette_runtime.h"
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
static const char *TAG="OT_PALETTE_NVS";
static portMUX_TYPE palette_lock=portMUX_INITIALIZER_UNLOCKED;
static ot_palette_entry_t palette_bank[OT_PALETTE_SLOTS];
static void key_for(uint8_t index,char key[8]){
    (void)snprintf(key,8,"p%03u",(unsigned)index);
}
void ot_palette_runtime_load(void){
    nvs_handle_t n;
    /* Missing namespace is a neutral empty bank, never a substituted palette. */
    if(nvs_open("ot_palettes",NVS_READONLY,&n)!=ESP_OK)return;
    for(unsigned i=0;i<OT_PALETTE_SLOTS;i++){
        char key[8];key_for((uint8_t)i,key);
        ot_palette_record_t record={0};size_t size=sizeof(record);
        if(nvs_get_blob(n,key,&record,&size)!=ESP_OK||size!=sizeof(record))continue;
        ot_palette_entry_t entry={0};
        if(ot_palette_record_is_tombstone(&record)){
            entry.revision=record.revision;
        }else if(!ot_palette_entry_from_record(&entry,&record) || record.index!=i){
            ESP_LOGW(TAG,"Invalid stored custom palette at index %u; omitted",i);
            continue;
        }
        portENTER_CRITICAL(&palette_lock);
        palette_bank[i]=entry;
        portEXIT_CRITICAL(&palette_lock);
    }
    nvs_close(n);
}
bool ot_palette_runtime_get(uint8_t index,ot_palette_entry_t *out){
    if(!out||index>=OT_PALETTE_SLOTS)return false;
    portENTER_CRITICAL(&palette_lock);
    *out=palette_bank[index];
    portEXIT_CRITICAL(&palette_lock);
    return true;
}
void ot_palette_runtime_snapshot(const uint8_t *indices,size_t count,
                                 ot_palette_entry_t *out){
    if(!indices||!out)return;
    portENTER_CRITICAL(&palette_lock);
    for(size_t i=0;i<count;i++)
        out[i]=indices[i]<OT_PALETTE_SLOTS?
               palette_bank[indices[i]]:(ot_palette_entry_t){0};
    portEXIT_CRITICAL(&palette_lock);
}
static bool nvs_commit_record(const ot_palette_record_t *record){
    char key[8];key_for(record->index,key);
    nvs_handle_t h;
    if(nvs_open("ot_palettes",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,key,record,sizeof(*record));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}
bool ot_palette_runtime_put(const ot_palette_record_t *record,uint32_t expected){
    ot_palette_entry_t next={0};
    if(!record||!ot_palette_entry_from_record(&next,record)||
       record->revision!=expected+1u)return false;
    ot_palette_entry_t old={0};
    if(!ot_palette_runtime_get(record->index,&old)||
       old.revision!=expected||!nvs_commit_record(record))return false;
    portENTER_CRITICAL(&palette_lock);
    palette_bank[record->index]=next;
    portEXIT_CRITICAL(&palette_lock);
    return true;
}
bool ot_palette_runtime_delete(uint8_t index,uint32_t expected){
    if(index>=OT_PALETTE_SLOTS||expected==UINT32_MAX)return false;
    ot_palette_entry_t old={0};
    if(!ot_palette_runtime_get(index,&old)||!old.present||
       old.revision!=expected)return false;
    ot_palette_record_t dead={0};
    if(!ot_palette_record_tombstone(&dead,index,expected+1u)||
       !nvs_commit_record(&dead))return false;
    portENTER_CRITICAL(&palette_lock);
    palette_bank[index]=(ot_palette_entry_t){.revision=expected+1u};
    portEXIT_CRITICAL(&palette_lock);
    return true;
}
