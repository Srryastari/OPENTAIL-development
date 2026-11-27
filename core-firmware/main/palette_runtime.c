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
    nvs_handle_t active;
    /* New palette records share the effects/presets namespace. The old
     * ot_palettes namespace is read-only migration input. */
    esp_err_t e=nvs_open("ot_fx_presets",NVS_READWRITE,&active);
    if(e!=ESP_OK){ESP_LOGE(TAG,"Cannot open shared FX/palette namespace: %s",
                            esp_err_to_name(e));return;}
    nvs_handle_t legacy=0;
    const esp_err_t legacy_open=nvs_open("ot_palettes",NVS_READONLY,&legacy);
    const bool legacy_available=legacy_open==ESP_OK;
    if(legacy_open!=ESP_OK&&legacy_open!=ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG,"Legacy palette namespace unavailable: %s",
                 esp_err_to_name(legacy_open));
    bool migrated[OT_PALETTE_SLOTS]={0};
    uint16_t migrated_crc[OT_PALETTE_SLOTS]={0};
    uint32_t migrated_revision[OT_PALETTE_SLOTS]={0};
    uint8_t migrated_count[OT_PALETTE_SLOTS]={0};
    bool migration_dirty=false;
    for(unsigned i=0;i<OT_PALETTE_SLOTS;i++){
        char key[8];key_for((uint8_t)i,key);
        ot_palette_record_t record={0};size_t size=sizeof(record);
        e=nvs_get_blob(active,key,&record,&size);
        bool from_legacy=false;
        if(e==ESP_ERR_NVS_NOT_FOUND&&legacy_available){
            size=sizeof(record);
            e=nvs_get_blob(legacy,key,&record,&size);
            from_legacy=e==ESP_OK;
        }
        if(e!=ESP_OK)continue;
        if(size!=sizeof(record)){
            ESP_LOGW(TAG,"Palette record size mismatch at %u",i);continue;
        }
        /* The NVS key and embedded index must match for LIVE and DELETED
         * records; otherwise corruption could import an unrelated revision. */
        if(record.index!=i){
            ESP_LOGW(TAG,"Palette NVS index/key mismatch at %u",i);
            continue;
        }
        ot_palette_entry_t entry={0};
        if(ot_palette_record_is_tombstone(&record)){
            entry.revision=record.revision;
        }else if(!ot_palette_entry_from_record(&entry,&record) || record.index!=i){
            ESP_LOGW(TAG,"Invalid stored custom palette at index %u; omitted",i);
            continue;
        }
        if(from_legacy){
            const esp_err_t write=nvs_set_blob(active,key,&record,sizeof(record));
            if(write==ESP_OK){
                migration_dirty=true;migrated[i]=true;
                migrated_crc[i]=record.crc;migrated_revision[i]=record.revision;
                migrated_count[i]=record.count;
            }else{
                ESP_LOGW(TAG,"Legacy palette %u remains read-only: %s",
                         i,esp_err_to_name(write));
            }
        }
        portENTER_CRITICAL(&palette_lock);
        palette_bank[i]=entry;
        portEXIT_CRITICAL(&palette_lock);
    }
    if(migration_dirty){
        e=nvs_commit(active);
        if(e!=ESP_OK){
            ESP_LOGW(TAG,"Legacy palette migration commit failed: %s",
                     esp_err_to_name(e));
        }else{
            for(unsigned i=0;i<OT_PALETTE_SLOTS;i++)if(migrated[i]){
                char key[8];key_for((uint8_t)i,key);
                ot_palette_record_t verify={0};size_t size=sizeof(verify);
                e=nvs_get_blob(active,key,&verify,&size);
                const bool valid=e==ESP_OK&&size==sizeof(verify)&&
                    verify.index==i&&verify.revision==migrated_revision[i]&&
                    verify.count==migrated_count[i]&&verify.crc==migrated_crc[i]&&
                    (ot_palette_record_is_tombstone(&verify)||
                     ot_palette_record_valid(&verify,&(ot_wled_palette16_t){0}));
                if(!valid)ESP_LOGE(TAG,"Migrated palette %u readback failed",i);
            }
        }
    }
    if(legacy_available)nvs_close(legacy);
    nvs_close(active);
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
    if(nvs_open("ot_fx_presets",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,key,record,sizeof(*record));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}
bool ot_palette_runtime_read_record(uint8_t index,ot_palette_record_t *out){
    if(index>=OT_PALETTE_SLOTS||!out)return false;
    ot_palette_entry_t live={0};
    if(!ot_palette_runtime_get(index,&live)||!live.present)return false;
    char key[8];key_for(index,key);
    ot_palette_record_t stored={0};size_t n=sizeof(stored);
    nvs_handle_t h;
    esp_err_t e=nvs_open("ot_fx_presets",NVS_READONLY,&h);
    if(e==ESP_OK){
        e=nvs_get_blob(h,key,&stored,&n);
        nvs_close(h);
    }
    /* If migration could not commit, keep authenticated readback available
     * from the untouched legacy record. New writes always target ot_fx_presets. */
    if(e==ESP_ERR_NVS_NOT_FOUND){
        n=sizeof(stored);
        if(nvs_open("ot_palettes",NVS_READONLY,&h)!=ESP_OK)return false;
        e=nvs_get_blob(h,key,&stored,&n);
        nvs_close(h);
    }
    ot_wled_palette16_t checked;
    if(e!=ESP_OK||n!=sizeof(stored)||stored.index!=index||
       !ot_palette_record_valid(&stored,&checked))return false;
    ot_palette_entry_t current={0};
    if(!ot_palette_runtime_get(index,&current)||!current.present||
       current.revision!=stored.revision)return false;
    *out=stored;return true;
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
