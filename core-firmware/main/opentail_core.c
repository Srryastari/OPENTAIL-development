/* OpenTail CORE Alpha — universal user-profile pixel counts, hardware-budgeted, not road firmware.
 * ESP-IDF 5.5.x, ESP32-S3, 2x WS2815-compatible 800kHz RMT outputs.
 * One-ESP progress watchdog; no independent light fallback. Do not drive on road.
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
#include "panel_zone.h"
#include "panel_zone_diag.h"
#include "panel_zone_api.h"
#include "input_telemetry.h"
#include "config_journal.h"
#include "panel_zone_pair_store.h"
#include "trigger_manager.h"
#include "nvs.h"
#include "esp_timer.h"
#include "hal/gpio_ll.h"
#include "esp_random.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "palette_runtime.h"
#include "esp_app_desc.h"
#include "ot_fps_pacer.h"
#include "ot_frame_contract.h"
#include "ot_frame_snapshot.h"
#include "ot_single_watchdog_idf.h"

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
static DRAM_ATTR gpio_num_t input_pin_isr[OT_IN_COUNT];
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
/* GPIO any-edge ISR records transitions even during a slow NVS write.
 * A quiet debounced window + unchanged epoch is required to promote. */
static DRAM_ATTR volatile uint32_t input_edge_epoch __attribute__((aligned(4)))=0u;
/* GPIO ISR transition history. A sampler tick never has to infer whether
 * one or more turn edges happened between reads. Each lane is an SPSC ring:
 * ISR publishes head with release semantics; sampler consumes through tail. */
#define OT_TURN_EDGE_QUEUE_CAPACITY 32u
#define OT_TURN_EDGE_QUEUE_MASK (OT_TURN_EDGE_QUEUE_CAPACITY-1u)
typedef struct {
    uint64_t timestamp_us;
    bool asserted;
} ot_turn_edge_event_t;
_Static_assert((OT_TURN_EDGE_QUEUE_CAPACITY & OT_TURN_EDGE_QUEUE_MASK)==0u,
               "turn edge queue capacity must be a power of two");
static DRAM_ATTR ot_turn_edge_event_t turn_edge_events[2][OT_TURN_EDGE_QUEUE_CAPACITY];
static DRAM_ATTR volatile uint32_t turn_edge_head[2]={0u,0u};
static DRAM_ATTR volatile uint32_t turn_edge_tail[2]={0u,0u};
static DRAM_ATTR volatile uint32_t turn_edge_overflow[2]={0u,0u};
static DRAM_ATTR volatile uint32_t turn_edge_writer_active[2]={0u,0u};
/* Telemetry is additive: keep the existing per-lane SPSC turn queues intact. */
_Static_assert(__atomic_always_lock_free(sizeof(uint32_t),0),
               "32-bit ISR telemetry atomics must be lock-free");
static DRAM_ATTR volatile uint32_t input_raw_edges[OT_INPUT_DIAG_INPUTS] __attribute__((aligned(4)));
static DRAM_ATTR volatile uint32_t input_asserted_edges[OT_INPUT_DIAG_INPUTS] __attribute__((aligned(4)));
static DRAM_ATTR volatile uint32_t input_deasserted_edges[OT_INPUT_DIAG_INPUTS] __attribute__((aligned(4)));
static DRAM_ATTR volatile uint32_t input_raw_level_mask __attribute__((aligned(4)));
static DRAM_ATTR volatile uint32_t turn_edge_high_water[2] __attribute__((aligned(4)));
static DRAM_ATTR volatile uint32_t turn_edge_overflow_count[2] __attribute__((aligned(4)));
static uint64_t input_boot_snapshot_us=0u;
static uint8_t input_quiet_samples=0u;
static bool config_recovery_fault=false;
static bool config_promotion_ready=false;
static uint8_t live_input_mask=0;
static uint8_t sim_mask=0;
static TickType_t sim_deadline=0;
static ot_demo_t demo={OT_DEMO_OFF,{0,0,0},0};
static TickType_t demo_deadline=0;
static uint32_t frame_index=0;
/* Random per-boot diagnostic identity; never used as a security credential. */
static uint32_t core_boot_id=0u;
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

static ot_om_output_t om_outputs[OT_OM_OUTPUTS]; /* Compatibility projection for unchanged FX/v1 code. */
static ot_pz_controller_t pz_outputs; /* Authoritative hierarchical schema v3. */
static ot_pz_pending_t pz_deferred;
static ot_pz_preview_t pz_preview;
static ot_pz_controller_t pz_preview_commit_candidate;
static uint16_t pz_preview_commit_lane_revision=0u;
static uint8_t pz_preview_commit_lane=0u;
static bool pz_preview_commit_requested=false;
static bool pz_preview_commit_worker_active=false;
static ot_pz_diag_state_t pz_diag;
#define OT_PZ_RESET_TOMBSTONE_MAGIC 0x4f54525au
#define OT_PZ_RESET_TOMBSTONE_VERSION 1u
#define OT_PZ_RESET_TOMBSTONE_KEY "reset_v3"
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t reset_mask;
    uint8_t reserved;
    uint16_t revision[OT_PZ_OUTPUTS];
    ot_pz_controller_t controller;
    uint32_t crc32;
} ot_pz_reset_tombstone_t;

/* Pair-store and legacy-migration scratch is static BSS, not task stack.
 * Every runtime user holds om_write_mutex; boot recovery runs before tasks. */
static ot_pz_pair_store_workspace_t pz_pair_workspace;
static ot_pz_journal_t pz_legacy_active_journal,pz_legacy_stage_journal;
static ot_pz_reset_tombstone_t pz_legacy_tombstone;
_Static_assert(sizeof(pz_pair_workspace)<=16u*1024u,
               "v4 pair-store workspace exceeds its bounded DRAM budget");
typedef struct {
    bool active;
    uint8_t lane;
    uint16_t revision;
    uint32_t value;
    uint32_t expires_ms;
    uint32_t input_epoch;
    uint32_t stop_generation;
} ot_pz_reset_challenge_t;
static ot_pz_reset_challenge_t pz_reset_challenge;
static uint8_t pz_reset_pending_mask;
static bool pz_pair_commit_pending;
static uint32_t pz_pair_transaction_id;
static uint8_t pz_pair_commit_phase;
static uint32_t pz_pair_commit_phase_txn;
static void pz_promote(uint32_t epoch);
static void pz_preview_cancel_pending_locked(void);
static bool config_activation_idle(uint32_t epoch,bool allow_show);
static bool pz_persistent=false;
static uint16_t pz_revision[OT_PZ_OUTPUTS]={1u,1u};
static uint16_t pz_active_revision[OT_PZ_OUTPUTS]={1u,1u};
static uint16_t pz_render_revision[OT_PZ_OUTPUTS]={0u,0u};
static ot_om_pending_t om_deferred; /* Persisted zones await release. */
static bool om_persistent=false;
/* The old combined Output Manager journal is migration input only. A bad
 * legacy journal must not veto a valid authoritative v4 lane/map pair. */
static bool om_legacy_journal_fault=false;
static SemaphoreHandle_t om_write_mutex=NULL;
static uint16_t om_revision[OT_OM_OUTPUTS]; /* latest durably staged edit token */
static uint16_t om_active_revision[OT_OM_OUTPUTS]; /* currently rendered geometry */
/* Trigger profile is stored separately from v0.1.17 Output Manager NVS. */
static ot_tm_profile_t tm_profile;
static ot_tm_pending_t tm_deferred; /* Persisted mapping awaits release. */
static bool tm_persistent=false;
static uint16_t tm_revision=1u; /* latest durably staged edit token */
static uint16_t tm_active_revision=1u;
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
    /* Pending always wins at boot, even if power failed AFTER writing the
     * future active key. Its previous image is the only safe old routing. */
}
/* Persist only the approved physical input policy values. Live debounce
 * timers, edge timestamps and sampler state are deliberately never stored.
 * Existing trigger profiles migrate by writing this independent versioned
 * policy record with the exact defaults used by this firmware build. */
static bool tm_policy_load(void){
    ot_tm_policy_record_t expected={0},stored={0};
    if(!ot_tm_policy_build(&expected,(uint8_t)(OT_INPUT_ACTIVE_LOW?1u:0u),
        (uint8_t)OT_DEBOUNCE_TICKS,10u,OT_TM_TURN_RELEASE_INPUT_MASK,
        (uint32_t)OT_TURN_RELEASE_HOLD_US))
        return false;
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READWRITE,&h)!=ESP_OK)return false;
    size_t size=0u;
    esp_err_t e=nvs_get_blob(h,OT_TM_POLICY_KEY,NULL,&size);
    bool migrated=false;
    if(e==ESP_ERR_NVS_NOT_FOUND){
        e=nvs_set_blob(h,OT_TM_POLICY_KEY,&expected,sizeof(expected));
        if(e==ESP_OK)e=nvs_commit(h);
        if(e==ESP_OK){
            migrated=true;size=sizeof(stored);
            e=nvs_get_blob(h,OT_TM_POLICY_KEY,&stored,&size);
        }
    }else if(e==ESP_OK){
        if(size!=sizeof(stored))e=ESP_ERR_INVALID_SIZE;
        else e=nvs_get_blob(h,OT_TM_POLICY_KEY,&stored,&size);
    }
    nvs_close(h);
    if(e!=ESP_OK||size!=sizeof(stored)||!ot_tm_policy_valid(&stored)||
       memcmp(&stored,&expected,sizeof(stored))!=0)return false;
    if(migrated)ESP_LOGI(TAG,
        "Migrated trigger input policy: active_low=%u sampled_assert=%u x %ums turn_release_mask=%02x turn_release_us=%lu",
        (unsigned)stored.input_active_low,(unsigned)stored.assertion_samples,
        (unsigned)stored.sample_period_ms,(unsigned)stored.turn_release_input_mask,
        (unsigned long)stored.turn_release_hold_us);
    return true;
}
/* Size/version-aware reader preserves draft-v1 records and the older
 * profile_v1 key. A corrupt newer journal never falls back to legacy. */
static esp_err_t tm_journal_read(nvs_handle_t h,const char *key,ot_tm_journal_t *out){
    size_t n=0;esp_err_t e=nvs_get_blob(h,key,NULL,&n);
    if(e!=ESP_OK)return e;
    if(n==sizeof(*out)){
        e=nvs_get_blob(h,key,out,&n);
        return e==ESP_OK&&ot_tm_journal_valid(out)?ESP_OK:ESP_ERR_INVALID_CRC;
    }
    if(n==sizeof(ot_tm_journal_v1_t)){
        ot_tm_journal_v1_t old={0};
        e=nvs_get_blob(h,key,&old,&n);
        return e==ESP_OK&&ot_tm_journal_upgrade_v1(&old,out)?
               ESP_OK:ESP_ERR_INVALID_CRC;
    }
    return ESP_ERR_INVALID_SIZE;
}
static void tm_journal_load(void){
    nvs_handle_t h;
    esp_err_t e=nvs_open("ot_trig_cfg",NVS_READONLY,&h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return;
    if(e!=ESP_OK){config_recovery_fault=true;return;}
    ot_tm_journal_t active={0},stage={0};
    const esp_err_t a=tm_journal_read(h,OT_CFG_JOURNAL_KEY_ACTIVE,&active);
    const esp_err_t b=tm_journal_read(h,OT_CFG_JOURNAL_KEY_STAGED,&stage);
    const bool has_a=a==ESP_OK,has_b=b==ESP_OK;
    if((a!=ESP_OK&&a!=ESP_ERR_NVS_NOT_FOUND)||
       (b!=ESP_OK&&b!=ESP_ERR_NVS_NOT_FOUND))config_recovery_fault=true;
    nvs_close(h);
    if(!config_recovery_fault&&!ot_tm_journal_recover(
        has_a?&active:NULL,has_b?&stage:NULL,&tm_profile,&tm_deferred))
        config_recovery_fault=true;
    if(config_recovery_fault){ot_tm_defaults(&tm_profile);tm_persistent=false;return;}
    if(has_a||has_b){
        /* Even after stage erasure, the active journal retains its prior
         * mapping: reboot always holds the old routing until a quiet sampler. */
        const ot_tm_journal_t *r=has_b?&stage:&active;
        tm_active_revision=r->previous_revision;
        tm_revision=r->candidate_revision;
        if(!tm_deferred.pending)tm_active_revision=tm_revision;
        tm_persistent=true;
    }
}
static bool tm_save(const ot_tm_profile_t *p){
    if(!ot_tm_valid(p))return false;
    ot_tm_profile_t old;uint16_t old_rev,new_rev;
    portENTER_CRITICAL(&state_lock);
    old=tm_profile;old_rev=tm_active_revision;new_rev=tm_revision;
    portEXIT_CRITICAL(&state_lock);
    if(new_rev==UINT16_MAX)return false; /* BLE 16-bit token cannot wrap. */
    ot_tm_journal_t staged={0};
    if(!ot_tm_journal_build_revisions(&staged,&old,p,old_rev,(uint16_t)(new_rev+1u)))
        return false;
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,OT_CFG_JOURNAL_KEY_STAGED,&staged,sizeof(staged));
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
static esp_err_t om_journal_read(nvs_handle_t h,const char *key,ot_om_journal_t *out){
    size_t n=0;esp_err_t e=nvs_get_blob(h,key,NULL,&n);
    if(e!=ESP_OK)return e;
    if(n==sizeof(*out)){
        e=nvs_get_blob(h,key,out,&n);
        return e==ESP_OK&&ot_om_journal_valid(out)?ESP_OK:ESP_ERR_INVALID_CRC;
    }
    if(n==sizeof(ot_om_journal_v1_t)){
        ot_om_journal_v1_t old={0};
        e=nvs_get_blob(h,key,&old,&n);
        return e==ESP_OK&&ot_om_journal_upgrade_v1(&old,out)?
               ESP_OK:ESP_ERR_INVALID_CRC;
    }
    return ESP_ERR_INVALID_SIZE;
}
static void om_journal_load(void){
    nvs_handle_t h;
    esp_err_t e=nvs_open("ot_output_cfg",NVS_READONLY,&h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return;
    if(e!=ESP_OK){om_legacy_journal_fault=true;return;}
    ot_om_journal_t active={0},stage={0};
    const esp_err_t a=om_journal_read(h,OT_CFG_JOURNAL_KEY_ACTIVE,&active);
    const esp_err_t b=om_journal_read(h,OT_CFG_JOURNAL_KEY_STAGED,&stage);
    const bool has_a=a==ESP_OK,has_b=b==ESP_OK;
    if((a!=ESP_OK&&a!=ESP_ERR_NVS_NOT_FOUND)||
       (b!=ESP_OK&&b!=ESP_ERR_NVS_NOT_FOUND))om_legacy_journal_fault=true;
    nvs_close(h);
    if(!om_legacy_journal_fault&&!ot_om_journal_recover(
        has_a?&active:NULL,has_b?&stage:NULL,om_outputs,&om_deferred))
        om_legacy_journal_fault=true;
    if(om_legacy_journal_fault){
        ot_om_defaults(om_outputs);om_deferred.pending=false;
        om_persistent=false;return;
    }
    if(has_a||has_b){
        const ot_om_journal_t *r=has_b?&stage:&active;
        memcpy(om_active_revision,r->previous_revision,sizeof(om_active_revision));
        memcpy(om_revision,r->candidate_revision,sizeof(om_revision));
        if(!om_deferred.pending)
            memcpy(om_active_revision,om_revision,sizeof(om_revision));
        om_persistent=true;
    }
}
static esp_err_t pz_journal_read(nvs_handle_t h,const char *key,ot_pz_journal_t *out){
    size_t n=0;esp_err_t e=nvs_get_blob(h,key,NULL,&n);
    if(e!=ESP_OK)return e;
    if(n!=sizeof(*out))return ESP_ERR_INVALID_SIZE;
    e=nvs_get_blob(h,key,out,&n);
    return e==ESP_OK&&ot_pz_journal_valid(out)?ESP_OK:ESP_ERR_INVALID_CRC;
}
/* state_lock held: keep the old Output Manager view only as a compatibility
 * projection for the unchanged effects bank and legacy v1 REST/BLE interfaces. */
static void pz_refresh_legacy_projection_locked(void){
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(!ot_pz_to_fx_view(&pz_outputs.outputs[lane],lane,&om_outputs[lane])){
            ot_om_output_t defaults[OT_OM_OUTPUTS];ot_om_defaults(defaults);
            om_outputs[lane]=defaults[lane];
        }
    }
    memcpy(om_revision,pz_revision,sizeof(om_revision));
    memcpy(om_active_revision,pz_active_revision,sizeof(om_active_revision));
    om_persistent=pz_persistent;
}
static bool pz_reset_tombstone_valid(const ot_pz_reset_tombstone_t *record){
    if(!record||record->magic!=OT_PZ_RESET_TOMBSTONE_MAGIC||
       record->version!=OT_PZ_RESET_TOMBSTONE_VERSION||
       (record->reset_mask&~((1u<<OT_PZ_OUTPUTS)-1u))!=0u)return false;
    return record->crc32==ot_cfg_crc32_zeroed(record,sizeof(*record),
        offsetof(ot_pz_reset_tombstone_t,crc32),sizeof(record->crc32));
}
static ot_pz_pair_io_result_t pz_pair_nvs_read(void *context,
        ot_pz_pair_namespace_t name_space,const char *key,void *data,size_t *size){
    (void)context;
    const char *storage_namespace=name_space==OT_PZ_PAIR_NS_OUTPUT_CFG?
        "ot_output_cfg":"ot_zone_map";
    nvs_handle_t h;
    esp_err_t e=nvs_open(storage_namespace,NVS_READONLY,&h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return OT_PZ_PAIR_IO_NOT_FOUND;
    if(e!=ESP_OK)return OT_PZ_PAIR_IO_ERROR;
    e=nvs_get_blob(h,key,data,size);
    nvs_close(h);
    if(e==ESP_OK)return OT_PZ_PAIR_IO_OK;
    if(e==ESP_ERR_NVS_NOT_FOUND)return OT_PZ_PAIR_IO_NOT_FOUND;
    return OT_PZ_PAIR_IO_ERROR;
}
static bool pz_pair_nvs_write_commit(void *context,
        ot_pz_pair_namespace_t name_space,const char *key,
        const void *data,size_t size){
    (void)context;
    const char *storage_namespace=name_space==OT_PZ_PAIR_NS_OUTPUT_CFG?
        "ot_output_cfg":"ot_zone_map";
    nvs_handle_t h;
    if(nvs_open(storage_namespace,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,key,data,size);
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}
static bool pz_pair_nvs_erase_commit(void *context,
        ot_pz_pair_namespace_t name_space,const char *key){
    (void)context;
    const char *storage_namespace=name_space==OT_PZ_PAIR_NS_OUTPUT_CFG?
        "ot_output_cfg":"ot_zone_map";
    nvs_handle_t h;
    if(nvs_open(storage_namespace,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_erase_key(h,key);
    if(e==ESP_ERR_NVS_NOT_FOUND){nvs_close(h);return true;}
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e==ESP_OK;
}
static const ot_pz_pair_store_io_t pz_pair_store_io={
    .context=NULL,
    .read=pz_pair_nvs_read,
    .write_commit=pz_pair_nvs_write_commit,
    .erase_commit=pz_pair_nvs_erase_commit
};
static bool pz_pair_phase_hook(void *context,uint8_t phase,uint32_t transaction_id){
    (void)context;
    __atomic_store_n(&pz_pair_commit_phase_txn,transaction_id,__ATOMIC_RELEASE);
    __atomic_store_n(&pz_pair_commit_phase,phase,__ATOMIC_RELEASE);
    ESP_LOGI(TAG,"PZ_PAIR_PHASE phase=%u transaction=%08lx",
             (unsigned)phase,(unsigned long)transaction_id);
    return true;
}
/* Read-only v3 combined journal/tombstone migration source. Once a v4 pair is
 * authoritative, these legacy records are never consulted as a fallback. */
static bool pz_load_legacy_snapshot(ot_pz_controller_t *previous,
        ot_pz_controller_t *candidate,uint16_t previous_revision[OT_PZ_OUTPUTS],
        uint16_t candidate_revision[OT_PZ_OUTPUTS],uint8_t *reset_mask){
    if(!previous||!candidate||!previous_revision||!candidate_revision||!reset_mask)
        return false;
    ot_pz_defaults(previous);*candidate=*previous;*reset_mask=0u;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        previous_revision[lane]=candidate_revision[lane]=1u;
    ot_pz_journal_t *active=&pz_legacy_active_journal;
    ot_pz_journal_t *stage=&pz_legacy_stage_journal;
    ot_pz_reset_tombstone_t *tombstone=&pz_legacy_tombstone;
    ot_pz_pending_t *deferred=&pz_pair_workspace.pending;
    memset(active,0,sizeof(*active));memset(stage,0,sizeof(*stage));
    memset(tombstone,0,sizeof(*tombstone));memset(deferred,0,sizeof(*deferred));
    nvs_handle_t h;
    esp_err_t e=nvs_open("ot_zone_map",NVS_READONLY,&h);
    if(e==ESP_OK){
        size_t tombstone_size=sizeof(*tombstone);
        const esp_err_t t=nvs_get_blob(h,OT_PZ_RESET_TOMBSTONE_KEY,
                                       tombstone,&tombstone_size);
        const bool has_tombstone=t==ESP_OK&&tombstone_size==sizeof(*tombstone)&&
                                 pz_reset_tombstone_valid(tombstone);
        const esp_err_t a=pz_journal_read(h,OT_PZ_JOURNAL_KEY_ACTIVE,active);
        const esp_err_t b=pz_journal_read(h,OT_PZ_JOURNAL_KEY_STAGED,stage);
        const bool has_a=a==ESP_OK,has_b=b==ESP_OK;
        if((a!=ESP_OK&&a!=ESP_ERR_NVS_NOT_FOUND)||
           (b!=ESP_OK&&b!=ESP_ERR_NVS_NOT_FOUND)||
           (t!=ESP_OK&&t!=ESP_ERR_NVS_NOT_FOUND)||
           (t==ESP_OK&&!has_tombstone)){
            nvs_close(h);return false;
        }
        if(has_a||has_b){
            if(!ot_pz_journal_recover(has_a?active:NULL,has_b?stage:NULL,
                previous,deferred,previous_revision,candidate_revision)){
                nvs_close(h);return false;
            }
            *candidate=deferred->pending?deferred->candidate:*previous;
            if(has_tombstone&&
               memcmp(candidate,&tombstone->controller,sizeof(*candidate))==0)
                *reset_mask=tombstone->reset_mask;
            nvs_close(h);return true;
        }
        if(has_tombstone){
            *previous=tombstone->controller;*candidate=*previous;
            memcpy(previous_revision,tombstone->revision,sizeof(tombstone->revision));
            memcpy(candidate_revision,tombstone->revision,sizeof(tombstone->revision));
            *reset_mask=tombstone->reset_mask;
            nvs_close(h);return true;
        }
        nvs_close(h);
    }else if(e!=ESP_ERR_NVS_NOT_FOUND){
        return false;
    }
    /* A valid v3 controller journal or reset tombstone is self-contained.
     * Only consult the older Output Manager journal when it is the actual
     * migration source; unrelated legacy corruption must not mask valid v3. */
    if(om_legacy_journal_fault)return false;
    if(!ot_pz_migrate_legacy(om_outputs,om_persistent,previous))return false;
    *candidate=*previous;
    if(om_deferred.pending&&
       !ot_pz_migrate_legacy(om_deferred.outputs,om_persistent,candidate))return false;
    memcpy(previous_revision,om_active_revision,sizeof(om_active_revision));
    memcpy(candidate_revision,om_revision,sizeof(om_revision));
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(!previous_revision[lane])previous_revision[lane]=1u;
        if(candidate_revision[lane]<previous_revision[lane])
            candidate_revision[lane]=previous_revision[lane];
    }
    return true;
}
static void pz_load(void){
    ot_pz_pair_store_workspace_t *workspace=&pz_pair_workspace;
    ot_pz_defaults(&pz_outputs);
    pz_deferred.pending=false;pz_persistent=false;
    __atomic_store_n(&pz_pair_commit_pending,false,__ATOMIC_RELAXED);
    __atomic_store_n(&pz_pair_transaction_id,0u,__ATOMIC_RELAXED);
    __atomic_store_n(&pz_pair_commit_phase,0u,__ATOMIC_RELAXED);
    __atomic_store_n(&pz_pair_commit_phase_txn,0u,__ATOMIC_RELAXED);
    pz_reset_pending_mask=0u;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        pz_revision[lane]=pz_active_revision[lane]=1u;
    if(config_recovery_fault)return;

    if(!ot_pz_pair_store_snapshot(&pz_pair_store_io,workspace)){
        config_recovery_fault=true;ot_pz_defaults(&pz_outputs);
        ot_st_set_fault("v4 lane/map pair journal invalid");return;
    }
    const ot_pz_pair_store_snapshot_t *snapshot=&workspace->snapshot;
    /* The durable marker is written in phase 6 after map-active. Its presence
     * alone proves v4 ownership: if either active journal is missing, recovery
     * must fail closed rather than resurrect stale v3 geometry. */
    const bool v4_authoritative=snapshot->has_marker||snapshot->has_intent||
        snapshot->has_map_active||snapshot->has_lane_active;
    if(v4_authoritative){
        bool intent_pending=false;uint32_t transaction_id=0u;uint8_t reset_mask=0u;
        if(!ot_pz_pair_store_recover(&pz_pair_store_io,workspace,NULL,&pz_outputs,
             &pz_deferred,pz_active_revision,pz_revision,&intent_pending,
             &transaction_id,&reset_mask)){
            config_recovery_fault=true;ot_pz_defaults(&pz_outputs);
            pz_deferred.pending=false;pz_persistent=false;
            ot_st_set_fault("v4 lane/map pair recovery mismatch");return;
        }
        __atomic_store_n(&pz_pair_transaction_id,transaction_id,__ATOMIC_RELAXED);
        __atomic_store_n(&pz_pair_commit_phase,intent_pending?0u:9u,__ATOMIC_RELEASE);
        __atomic_store_n(&pz_pair_commit_phase_txn,transaction_id,__ATOMIC_RELEASE);
        __atomic_store_n(&pz_pair_commit_pending,intent_pending,__ATOMIC_RELEASE);
        pz_reset_pending_mask=reset_mask;
        pz_persistent=true;
    }else{
        uint16_t previous_revision[OT_PZ_OUTPUTS],candidate_revision[OT_PZ_OUTPUTS];
        uint8_t reset_mask=0u;
        if(!pz_load_legacy_snapshot(&workspace->previous,&workspace->candidate,
             previous_revision,candidate_revision,&reset_mask)){
            config_recovery_fault=true;ot_pz_defaults(&pz_outputs);
            ot_st_set_fault("legacy configuration migration source invalid");return;
        }
        const bool changed=memcmp(&workspace->previous,&workspace->candidate,
                                  sizeof(workspace->previous))!=0;
        for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
            const bool lane_changed=memcmp(&workspace->previous.outputs[lane],
                &workspace->candidate.outputs[lane],
                sizeof(workspace->previous.outputs[lane]))!=0;
            if(lane_changed&&candidate_revision[lane]<=previous_revision[lane]){
                if(previous_revision[lane]==UINT16_MAX){
                    config_recovery_fault=true;
                    ot_st_set_fault("legacy migration revision exhausted");return;
                }
                candidate_revision[lane]=(uint16_t)(previous_revision[lane]+1u);
            }
        }
        /* A first-time v4 pair needs an intent even when the legacy profile is
         * already stable. Advance one revision without changing its contents. */
        if(!changed){
            if(candidate_revision[0]==UINT16_MAX){
                config_recovery_fault=true;
                ot_st_set_fault("initial v4 pair revision exhausted");return;
            }
            candidate_revision[0]=(uint16_t)(candidate_revision[0]+1u);
        }
        uint32_t transaction_id=esp_random();
        if(transaction_id==0u)transaction_id=esp_random();
        if(transaction_id==0u||
           !ot_pz_pair_store_begin(&pz_pair_store_io,workspace,
                &workspace->previous,&workspace->candidate,previous_revision,
                candidate_revision,transaction_id,reset_mask,true,
                pz_pair_phase_hook,NULL)){
            config_recovery_fault=true;
            ot_st_set_fault("v4 legacy pair migration staging failed");return;
        }
        pz_outputs=workspace->previous;
        pz_deferred.candidate=workspace->candidate;
        pz_deferred.pending=changed;
        memcpy(pz_active_revision,previous_revision,sizeof(pz_active_revision));
        memcpy(pz_revision,candidate_revision,sizeof(pz_revision));
        __atomic_store_n(&pz_pair_transaction_id,transaction_id,__ATOMIC_RELAXED);
        __atomic_store_n(&pz_pair_commit_pending,true,__ATOMIC_RELEASE);
        pz_reset_pending_mask=reset_mask;pz_persistent=true;
    }
    /* All legacy output settings have been joined into the v4 pair. The old
     * Output Manager pending journal must never race the authoritative pair. */
    om_deferred.pending=false;
    portENTER_CRITICAL(&state_lock);
    pz_refresh_legacy_projection_locked();
    portEXIT_CRITICAL(&state_lock);
}
static bool pz_save_controller_locked(const ot_pz_controller_t *candidate){
    if(!candidate||!ot_pz_valid(candidate))return false;
    ot_pz_controller_t *previous=&pz_pair_workspace.previous;
    uint16_t previous_revision[OT_PZ_OUTPUTS],candidate_revision[OT_PZ_OUTPUTS];
    uint8_t reset_mask=0u;
    bool busy=false;
    portENTER_CRITICAL(&state_lock);
    busy=pz_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)||
         (pz_preview.active&&!pz_preview.committing);
    *previous=pz_outputs;
    memcpy(previous_revision,pz_active_revision,sizeof(previous_revision));
    memcpy(candidate_revision,pz_revision,sizeof(candidate_revision));
    reset_mask=pz_reset_pending_mask;
    portEXIT_CRITICAL(&state_lock);
    if(busy)return false;
    bool changed=false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(memcmp(&previous->outputs[lane],&candidate->outputs[lane],
                  sizeof(previous->outputs[lane]))==0)continue;
        if(candidate_revision[lane]==UINT16_MAX)return false;
        candidate_revision[lane]++;changed=true;
    }
    if(!changed){
        for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
            if(reset_mask&(1u<<lane))
                if(!ot_pz_pair_store_mark_reset(&pz_pair_store_io,
                    &pz_pair_workspace,(uint8_t)lane))return false;
        return true;
    }
    uint32_t transaction_id=esp_random();
    if(transaction_id==0u)transaction_id=esp_random();
    if(transaction_id==0u)return false;
    __atomic_store_n(&pz_pair_commit_phase,0u,__ATOMIC_RELEASE);
    __atomic_store_n(&pz_pair_commit_phase_txn,transaction_id,__ATOMIC_RELEASE);
    if(!ot_pz_pair_store_begin(&pz_pair_store_io,&pz_pair_workspace,
        previous,candidate,previous_revision,candidate_revision,
        transaction_id,reset_mask,false,pz_pair_phase_hook,NULL)){
        config_recovery_fault=true;
        ot_st_set_fault("v4 lane/map pair staging failed; reboot reconciliation required");
        return false;
    }
    portENTER_CRITICAL(&state_lock);
    pz_deferred.candidate=*candidate;pz_deferred.pending=true;
    memcpy(pz_revision,candidate_revision,sizeof(pz_revision));
    pz_persistent=true;
    pz_refresh_legacy_projection_locked();
    portEXIT_CRITICAL(&state_lock);
    __atomic_store_n(&pz_pair_transaction_id,transaction_id,__ATOMIC_RELAXED);
    __atomic_store_n(&pz_pair_commit_pending,true,__ATOMIC_RELEASE);
    return true;
}
/* Legacy output writes are lossless only while the current lane is representable
 * as one panel with contiguous zones. Complex hierarchy is never silently flattened. */
static bool om_save(const ot_om_output_t items[OT_OM_OUTPUTS]){
    if(!items)return false;
    ot_pz_controller_t *candidate=&pz_pair_workspace.candidate;
    portENTER_CRITICAL(&state_lock);
    if(pz_preview.active){portEXIT_CRITICAL(&state_lock);return false;}
    *candidate=pz_outputs;
    portEXIT_CRITICAL(&state_lock);
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        ot_om_output_t current={0};
        if(!ot_pz_to_legacy(&candidate->outputs[lane],lane,&current))return false;
        if(memcmp(&current,&items[lane],sizeof(current))==0)continue;
        ot_pz_output_t replacement={0};
        if(!ot_pz_from_legacy_output(&items[lane],lane,candidate,&replacement))return false;
        candidate->outputs[lane]=replacement;
    }
    return pz_save_controller_locked(candidate);
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
    if(ble_fx_enabled){
        const uint32_t ms=(uint32_t)(esp_timer_get_time()/1000LL);
        ot_fx_bank_prune_expired(&fx_bank,ms);
        return ot_fx_bank_any_active(&fx_bank,ms);
    }
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
    ot_om_output_t copy[OT_OM_OUTPUTS];ot_pz_controller_t panel_copy;
    ot_tm_profile_t tm;bool persistent,config_pending;uint8_t mask;
    uint16_t active_revisions[OT_OM_OUTPUTS],staged_revisions[OT_OM_OUTPUTS];
    uint8_t show_zones[OT_OM_OUTPUTS]={0};
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    panel_copy=pz_outputs;config_pending=pz_deferred.pending||om_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
    memcpy(active_revisions,pz_active_revision,sizeof(active_revisions));
    memcpy(staged_revisions,pz_revision,sizeof(staged_revisions));
    persistent=pz_persistent&&!config_pending;tm=tm_profile;
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
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(!ot_pz_to_legacy(&panel_copy.outputs[lane],lane,&copy[lane]))
            return respond_err(r,409,"hierarchical Panel/Zone map cannot be represented by legacy Output v1");
    cJSON *obj=cJSON_CreateObject();
    if(!obj)return respond_err(r,500,"out of memory");
    if(!cJSON_AddNumberToObject(obj,"revision",OT_OM_VERSION) ||
       !cJSON_AddNumberToObject(obj,"panel_zone_schema",OT_PZ_SCHEMA_VERSION) ||
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
    /* Legacy outputs[] is emitted only when the active v3 model can be
     * represented losslessly as one contiguous panel per physical output. */
    cJSON *rev_active=cJSON_AddArrayToObject(obj,"active_config_revisions");
    cJSON *rev_staged=cJSON_AddArrayToObject(obj,"config_revisions");
    if(!rev_active||!rev_staged){cJSON_Delete(obj);return respond_err(r,500,"out of memory");}
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        cJSON *a=cJSON_CreateNumber(active_revisions[i]);
        cJSON *b=cJSON_CreateNumber(staged_revisions[i]);
        if(!a||!b){cJSON_Delete(a);cJSON_Delete(b);cJSON_Delete(obj);return respond_err(r,500,"out of memory");}
        cJSON_AddItemToArray(rev_active,a);
        cJSON_AddItemToArray(rev_staged,b);
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
    unsigned index=0,pixels=0,expected_rev=0;
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
    if(valid)valid=om_number(obj,"expected_config_revision",&expected_rev,1,UINT16_MAX);
    cJSON_Delete(obj);
    if(!valid)return respond_err(r,400,"invalid output mapping: zone lengths must total the configured active pixels and fit the board buffer budget");
    if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault)
        return respond_err(r,409,"physical input sampler or configuration promoter unavailable");
    if(!om_write_mutex || xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)
        return respond_err(r,409,"output mapping save busy");
    /* A freshly asserted optocoupler pin is busy even before debounce. */
    const bool raw_active=tm_gpio_input_mask()!=0;
    TickType_t now=xTaskGetTickCount();
    ot_om_output_t next_all[OT_OM_OUTPUTS];
    portENTER_CRITICAL(&state_lock);
    bool busy=pz_deferred.pending || om_deferred.pending || tm_deferred.pending || __atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE) || pz_preview.active || raw_active || live_input_mask || (before_deadline(now,sim_deadline)&&sim_mask) ||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) || show_window(now);
    const ot_pz_controller_t active_panel=pz_outputs;
    portEXIT_CRITICAL(&state_lock);
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(!ot_pz_to_legacy(&active_panel.outputs[lane],lane,&next_all[lane])){
            xSemaphoreGive(om_write_mutex);
            return respond_err(r,409,"legacy Output v1 cannot modify a multi-panel or multi-range mapping");
        }
    }
    if(busy){xSemaphoreGive(om_write_mutex);return respond_err(r,409,"stop shows and release all 12V triggers before saving output mapping");}
    portENTER_CRITICAL(&state_lock);
    const bool stale=pz_revision[index]!=(uint16_t)expected_rev;
    const bool exhausted=pz_revision[index]==UINT16_MAX;
    portEXIT_CRITICAL(&state_lock);
    if(stale||exhausted){xSemaphoreGive(om_write_mutex);return respond_err(r,409,"stale or exhausted output edit revision");}
    /* Idempotent REST save is a readback, not a new configuration revision. */
    if(memcmp(&next_all[index],&next,sizeof(next))==0){
        xSemaphoreGive(om_write_mutex);return om_get(r);
    }
    next_all[index]=next;
    /* No in-memory mutation if NVS commit fails. */
    if(!om_save(next_all)){xSemaphoreGive(om_write_mutex);return respond_err(r,500,"failed to save output mapping in flash");}
    xSemaphoreGive(om_write_mutex);
    return om_get(r);
}
/* BLE config uses EXACTLY the same output configuration, validator, and NVS key as Wi-Fi.
 * Revision is session-local and prevents a stale draft overwriting recent Wi-Fi edits.
 * Reads and commits run on NimBLE host task, so never hold state_lock during NVS I/O.
 * Bench-only: STOP all shows and release physical inputs before saving.
 */
bool ot_ble_config_get(uint8_t index,ot_om_output_t *out,uint16_t *revision){
    if(index>=OT_PZ_OUTPUTS||!out||!revision)return false;
    ot_pz_output_t active;uint16_t staged;
    portENTER_CRITICAL(&state_lock);
    active=pz_outputs.outputs[index];staged=pz_revision[index];
    portEXIT_CRITICAL(&state_lock);
    if(!ot_pz_to_legacy(&active,index,out))return false;
    *revision=staged;return true;
}
int ot_ble_config_save(uint8_t index,const ot_om_output_t *next,uint16_t base_revision){
    if(index>=OT_PZ_OUTPUTS||!next||!ot_om_valid(next))return 1;
    if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault)return 2;
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)return 2;
    const bool raw_active=tm_gpio_input_mask()!=0;
    TickType_t now=xTaskGetTickCount();
    ot_pz_controller_t candidate;ot_om_output_t current={0};bool busy,stale,exhausted;
    portENTER_CRITICAL(&state_lock);
    candidate=pz_outputs;
    busy=pz_deferred.pending||om_deferred.pending||tm_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)||
        pz_preview.active||raw_active||live_input_mask||
        (before_deadline(now,sim_deadline)&&sim_mask)||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF)||show_window(now);
    stale=pz_revision[index]!=base_revision;
    exhausted=pz_revision[index]==UINT16_MAX;
    portEXIT_CRITICAL(&state_lock);
    if(busy){xSemaphoreGive(om_write_mutex);return 2;}
    if(stale||exhausted){xSemaphoreGive(om_write_mutex);return 3;}
    /* V1 cannot express panel boundaries or multi-range zones. Refuse a
     * destructive downgrade; simple one-panel contiguous profiles remain
     * fully writable through the unchanged 261-byte v1 codec. */
    if(!ot_pz_to_legacy(&candidate.outputs[index],index,&current)){
        xSemaphoreGive(om_write_mutex);return 3;
    }
    if(memcmp(&current,next,sizeof(current))==0){
        xSemaphoreGive(om_write_mutex);return 0;
    }
    ot_pz_output_t replacement={0};
    if(!ot_pz_from_legacy_output(next,index,&candidate,&replacement)){
        xSemaphoreGive(om_write_mutex);return 1;
    }
    candidate.outputs[index]=replacement;
    const bool saved=pz_save_controller_locked(&candidate);
    xSemaphoreGive(om_write_mutex);
    return saved?0:4;
}
/* Versioned authenticated hierarchical Panel/Zone configuration transport. */
bool ot_ble_panel_get(uint8_t lane,ot_pz_output_t *out,uint16_t *active_revision,
                      uint16_t *staged_revision,bool *pending,bool *persistent){
    if(lane>=OT_PZ_OUTPUTS||!out||!active_revision||!staged_revision||
       !pending||!persistent)return false;
    portENTER_CRITICAL(&state_lock);
    *out=pz_outputs.outputs[lane];
    *active_revision=pz_active_revision[lane];
    *staged_revision=pz_revision[lane];
    *pending=pz_deferred.pending||om_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
    *persistent=pz_persistent;
    portEXIT_CRITICAL(&state_lock);
    return true;
}

static bool pz_controller_wire_crc32(const ot_pz_controller_t *candidate,
                                      uint32_t *crc32){
    if(!candidate||!crc32||!ot_pz_valid(candidate))return false;
    uint8_t canonical[OT_PZ_OUTPUTS][OT_PZ_MAX_WIRE_BYTES];
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(!ot_pz_wire_encode(&candidate->outputs[lane],lane,canonical[lane]))
            return false;
    *crc32=ot_cfg_crc32(canonical,sizeof(canonical));
    return true;
}
int ot_ble_panel_preview_begin(uint8_t lane,uint16_t base_revision,
                               uint16_t pixel_capacity){
    if(lane>=OT_PZ_OUTPUTS||pixel_capacity==0u||base_revision==UINT16_MAX)return 1;
    if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault||
       !native_outputs_ready)return 2;
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)
        return 2;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool accepted=false;
    portENTER_CRITICAL(&state_lock);
    const bool pending=pz_deferred.pending||om_deferred.pending||tm_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
    const bool capacity_ok=pz_outputs.outputs[lane].active_pixels==pixel_capacity;
    accepted=!pending&&!pz_preview.active&&capacity_ok&&
        pz_revision[lane]==base_revision&&pz_active_revision[lane]==base_revision&&
        ot_pz_preview_begin(&pz_preview,&pz_outputs,lane,
                            pz_active_revision[lane],base_revision,pending,now_ms);
    portEXIT_CRITICAL(&state_lock);
    xSemaphoreGive(om_write_mutex);
    if(accepted)request_render_refresh();
    return accepted?0:(capacity_ok?3:1);
}
int ot_ble_panel_preview_update_ranges(uint8_t lane,uint16_t base_revision,
                               const ot_pz_range_update_t *updates,size_t count){
    if(lane>=OT_PZ_OUTPUTS||!updates||count==0u||count>4u)return 1;
    /* Slider traffic is deliberately non-blocking and never queues stale edits. */
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,0)!=pdTRUE)return 2;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    ot_pz_preview_t candidate;
    uint16_t active_revision=0u;
    bool pending=false,valid=false;
    uint16_t staged_revision=0u;
    portENTER_CRITICAL(&state_lock);
    candidate=pz_preview;
    active_revision=pz_active_revision[lane];
    staged_revision=pz_revision[lane];
    pending=pz_deferred.pending||om_deferred.pending||tm_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
    portEXIT_CRITICAL(&state_lock);
    valid=!pending&&staged_revision==base_revision&&
        active_revision==base_revision&&
        ot_pz_preview_apply_updates(&candidate,lane,updates,count,
                                    base_revision,active_revision,pending,now_ms);
    if(valid){
        portENTER_CRITICAL(&state_lock);
        valid=pz_preview.active&&!pz_preview.committing&&
            pz_preview.lane==lane&&pz_preview.base_revision==base_revision&&
            pz_preview.expires_ms==candidate.expires_ms&&
            pz_active_revision[lane]==base_revision&&
            pz_revision[lane]==base_revision&&
            !pz_deferred.pending&&!om_deferred.pending&&!tm_deferred.pending&&!__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
        if(valid)pz_preview=candidate;
        portEXIT_CRITICAL(&state_lock);
    }
    xSemaphoreGive(om_write_mutex);
    if(valid)request_render_refresh();
    return valid?0:3;
}
int ot_ble_panel_preview_heartbeat(uint8_t lane,uint16_t base_revision){
    if(lane>=OT_PZ_OUTPUTS)return 1;
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,0)!=pdTRUE)return 2;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool valid=false;
    portENTER_CRITICAL(&state_lock);
    const bool pending=pz_deferred.pending||om_deferred.pending||tm_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
    valid=!pending&&pz_revision[lane]==base_revision&&
        pz_active_revision[lane]==base_revision&&
        ot_pz_preview_heartbeat(&pz_preview,lane,base_revision,now_ms);
    portEXIT_CRITICAL(&state_lock);
    xSemaphoreGive(om_write_mutex);
    return valid?0:3;
}
bool ot_ble_panel_preview_crc32(uint8_t lane,uint16_t base_revision,uint32_t *crc32){
    if(lane>=OT_PZ_OUTPUTS||!crc32)return false;
    ot_pz_controller_t candidate;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool valid=false;
    portENTER_CRITICAL(&state_lock);
    valid=pz_preview.active&&pz_preview.lane==lane&&
        pz_preview.base_revision==base_revision&&
        pz_active_revision[lane]==base_revision&&
        (pz_preview.committing||(int32_t)(now_ms-pz_preview.expires_ms)<0);
    if(valid)candidate=pz_preview.candidate;
    portEXIT_CRITICAL(&state_lock);
    return valid&&pz_controller_wire_crc32(&candidate,crc32);
}
bool ot_ble_panel_preview_commit_pending(uint8_t lane,uint16_t base_revision){
    if(lane>=OT_PZ_OUTPUTS)return false;
    bool pending=false;
    portENTER_CRITICAL(&state_lock);
    pending=(pz_preview_commit_requested||pz_preview_commit_worker_active||
             (pz_deferred.pending&&pz_preview.committing))&&
            pz_preview_commit_lane==lane&&
            pz_preview_commit_lane_revision==base_revision;
    portEXIT_CRITICAL(&state_lock);
    return pending;
}
uint16_t ot_ble_panel_render_revision(uint8_t lane){
    if(lane>=OT_PZ_OUTPUTS)return 0u;
    uint16_t revision=0u;
    portENTER_CRITICAL(&state_lock);
    revision=pz_render_revision[lane];
    portEXIT_CRITICAL(&state_lock);
    return revision;
}
int ot_ble_panel_preview_commit(uint8_t lane,uint16_t base_revision,
                                uint32_t expected_crc32){
    if(lane>=OT_PZ_OUTPUTS)return 1;
    /* Queue a validated immutable snapshot; the configuration worker owns NVS. */
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,0)!=pdTRUE)return 2;
    ot_pz_preview_t snapshot;
    bool accepted=false,no_change=false;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    portENTER_CRITICAL(&state_lock);
    const bool pending=pz_deferred.pending||om_deferred.pending||tm_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)||
        pz_preview_commit_requested||pz_preview_commit_worker_active;
    accepted=!pending&&pz_preview.active&&!pz_preview.committing&&
       pz_preview.lane==lane&&pz_preview.base_revision==base_revision&&
       pz_active_revision[lane]==base_revision&&pz_revision[lane]==base_revision&&
       (int32_t)(now_ms-pz_preview.expires_ms)<0;
    if(accepted){
        snapshot=pz_preview;
        no_change=memcmp(&snapshot.candidate,&pz_outputs,sizeof(pz_outputs))==0;
    }
    portEXIT_CRITICAL(&state_lock);
    uint32_t actual_crc32=0u;
    if(!accepted||!pz_controller_wire_crc32(&snapshot.candidate,&actual_crc32)||
       actual_crc32!=expected_crc32){
        xSemaphoreGive(om_write_mutex);return accepted?1:3;
    }
    portENTER_CRITICAL(&state_lock);
    accepted=pz_preview.active&&!pz_preview.committing&&
       pz_preview.lane==lane&&pz_preview.base_revision==base_revision&&
       pz_active_revision[lane]==base_revision&&pz_revision[lane]==base_revision&&
       pz_preview.expires_ms==snapshot.expires_ms&&
       !pz_deferred.pending&&!om_deferred.pending&&!tm_deferred.pending&&!__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)&&
       !pz_preview_commit_requested&&!pz_preview_commit_worker_active;
    if(accepted){
        if(no_change)ot_pz_preview_cancel(&pz_preview);
        else{
            pz_preview.committing=true;
            pz_preview_commit_candidate=snapshot.candidate;
            pz_preview_commit_lane=lane;
            pz_preview_commit_lane_revision=base_revision;
            pz_preview_commit_requested=true;
        }
    }
    portEXIT_CRITICAL(&state_lock);
    if(!accepted){xSemaphoreGive(om_write_mutex);return 3;}
    if(no_change)request_render_refresh();
    xSemaphoreGive(om_write_mutex);
    return 0;
}
static void pz_preview_cancel_pending_locked(void){
    pz_preview_commit_requested=false;
    memset(&pz_preview_commit_candidate,0,sizeof(pz_preview_commit_candidate));
}
void ot_ble_panel_preview_cancel(void){
    bool active=false;
    portENTER_CRITICAL(&state_lock);
    active=pz_preview.active;
    pz_preview_cancel_pending_locked();
    ot_pz_preview_cancel(&pz_preview);
    portEXIT_CRITICAL(&state_lock);
    if(active)request_render_refresh();
}


int ot_ble_panel_reset_challenge(uint8_t lane,uint16_t expected_revision,
                                 uint32_t *challenge){
    if(lane>=OT_PZ_OUTPUTS||!challenge)return 1;
    if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault||
       !native_outputs_ready||ot_sw_idf_decorative_inhibited())return 2;
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)
        return 2;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    const uint32_t epoch=__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE);
    const uint32_t stop_generation=stop_request_generation;
    bool safe=false;
    portENTER_CRITICAL(&state_lock);
    safe=!pz_deferred.pending&&!om_deferred.pending&&!tm_deferred.pending&&!__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)&&
        pz_revision[lane]==expected_revision&&
        pz_active_revision[lane]==expected_revision&&
        input_quiet_samples>=OT_DEBOUNCE_TICKS&&live_input_mask==0u&&
        !(before_deadline(xTaskGetTickCount(),sim_deadline)&&sim_mask!=0u)&&
        !(before_deadline(xTaskGetTickCount(),demo_deadline)&&demo.mode!=OT_DEMO_OFF)&&
        !show_window(xTaskGetTickCount())&&pz_diag.mode==OT_PZ_DIAG_NONE&&
        !pz_preview.active&&stop_request_generation==stop_render_generation;
    portEXIT_CRITICAL(&state_lock);
    if(!safe||tm_gpio_input_mask()!=0u||
       __atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)!=epoch||
       stop_request_generation!=stop_generation||ot_sw_idf_decorative_inhibited()){
        xSemaphoreGive(om_write_mutex);return 3;
    }
    uint32_t token=esp_random();
    if(token==0u)token=esp_random();
    if(token==0u){xSemaphoreGive(om_write_mutex);return 2;}
    portENTER_CRITICAL(&state_lock);
    pz_reset_challenge=(ot_pz_reset_challenge_t){
        .active=true,.lane=lane,.revision=expected_revision,.value=token,
        .expires_ms=now_ms+OT_PZ_RESET_CHALLENGE_TTL_MS,
        .input_epoch=epoch,.stop_generation=stop_generation
    };
    portEXIT_CRITICAL(&state_lock);
    xSemaphoreGive(om_write_mutex);
    *challenge=token;
    return 0;
}
int ot_ble_panel_reset_confirm(uint8_t lane,uint16_t expected_revision,
                               uint32_t challenge){
    if(lane>=OT_PZ_OUTPUTS||challenge==0u)return 1;
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)
        return 2;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    const uint32_t epoch=__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE);
    const uint32_t stop_generation=stop_request_generation;
    ot_pz_controller_t *candidate=&pz_pair_workspace.candidate;
    bool safe=false;
    portENTER_CRITICAL(&state_lock);
    safe=pz_reset_challenge.active&&pz_reset_challenge.lane==lane&&
        pz_reset_challenge.revision==expected_revision&&
        pz_reset_challenge.value==challenge&&
        (int32_t)(now_ms-pz_reset_challenge.expires_ms)<0&&
        pz_reset_challenge.input_epoch==epoch&&
        pz_reset_challenge.stop_generation==stop_generation&&
        pz_revision[lane]==expected_revision&&
        pz_active_revision[lane]==expected_revision&&
        !pz_deferred.pending&&!om_deferred.pending&&!tm_deferred.pending&&!__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)&&
        input_quiet_samples>=OT_DEBOUNCE_TICKS&&live_input_mask==0u&&
        !(before_deadline(xTaskGetTickCount(),sim_deadline)&&sim_mask!=0u)&&
        !(before_deadline(xTaskGetTickCount(),demo_deadline)&&demo.mode!=OT_DEMO_OFF)&&
        !show_window(xTaskGetTickCount())&&pz_diag.mode==OT_PZ_DIAG_NONE&&
        !pz_preview.active&&stop_request_generation==stop_render_generation;
    if(safe){
        *candidate=pz_outputs;
        candidate->outputs[lane].panel_count=0u;
        candidate->outputs[lane].zone_count=0u;
        memset(candidate->outputs[lane].panels,0,sizeof(candidate->outputs[lane].panels));
        memset(candidate->outputs[lane].zones,0,sizeof(candidate->outputs[lane].zones));
        pz_reset_pending_mask|=(uint8_t)(1u<<lane);
    }
    pz_reset_challenge.active=false; /* single-use even if the confirm fails */
    portEXIT_CRITICAL(&state_lock);
    if(!safe||tm_gpio_input_mask()!=0u||
       __atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)!=epoch||
       stop_request_generation!=stop_generation||ot_sw_idf_decorative_inhibited()){
        portENTER_CRITICAL(&state_lock);
        pz_reset_pending_mask&=(uint8_t)~(1u<<lane);
        portEXIT_CRITICAL(&state_lock);
        xSemaphoreGive(om_write_mutex);return 3;
    }
    if(!ot_pz_valid(candidate)){
        portENTER_CRITICAL(&state_lock);
        pz_reset_pending_mask&=(uint8_t)~(1u<<lane);
        portEXIT_CRITICAL(&state_lock);
        xSemaphoreGive(om_write_mutex);return 1;
    }
    if(!pz_save_controller_locked(candidate)){
        portENTER_CRITICAL(&state_lock);
        pz_reset_pending_mask&=(uint8_t)~(1u<<lane);
        portEXIT_CRITICAL(&state_lock);
        xSemaphoreGive(om_write_mutex);return 4;
    }
    if(pz_deferred.pending){
        pz_promote(epoch);
        portENTER_CRITICAL(&state_lock);
        const bool still_pending=pz_deferred.pending;
        portEXIT_CRITICAL(&state_lock);
        if(still_pending){xSemaphoreGive(om_write_mutex);return 5;}
    }else{
        /* pz_save_controller_locked() already persisted the reset bit through
         * mark_reset() when the lane was unchanged. Do not write the same
         * map-active record a second time after the successful save. */
        portENTER_CRITICAL(&state_lock);
        pz_reset_pending_mask&=(uint8_t)~(1u<<lane);
        pz_reset_challenge.active=false;
        ot_pz_diag_clear(&pz_diag);
        ot_pz_preview_cancel(&pz_preview);
        portEXIT_CRITICAL(&state_lock);
    }
    /* Reset readback is the authoritative v4 pair: map-only geometry and
     * hardware-only lane records must share the same transaction/revision. */
    bool readback_ok=ot_pz_pair_store_snapshot(&pz_pair_store_io,&pz_pair_workspace);
    const ot_pz_pair_store_snapshot_t *readback=&pz_pair_workspace.snapshot;
    readback_ok=readback_ok&&readback->has_map_active&&readback->has_lane_active&&
        readback->map_active.transaction_id==readback->lane_active.transaction_id&&
        readback->map_active.candidate.outputs[lane].panel_count==0u&&
        readback->map_active.candidate.outputs[lane].zone_count==0u&&
        readback->map_active.candidate_revision[lane]==pz_active_revision[lane]&&
        readback->lane_active.candidate_revision[lane]==pz_active_revision[lane]&&
        (readback->map_active.reset_mask&(1u<<lane))!=0u;
    if(!readback_ok){xSemaphoreGive(om_write_mutex);return 5;}
    ot_fx_bank_stop_all(&fx_bank);
    request_render_refresh();
    xSemaphoreGive(om_write_mutex);
    return 0;
}
int ot_ble_panel_save(uint8_t lane,const ot_pz_output_t *next,uint16_t base_revision){
    if(lane>=OT_PZ_OUTPUTS||!next||!ot_pz_output_valid(next,lane))return 1;
    if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault)return 2;
    if(!om_write_mutex||xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(2000))!=pdTRUE)return 2;
    const TickType_t now=xTaskGetTickCount();
    ot_pz_controller_t *candidate=&pz_pair_workspace.candidate;
    bool busy,stale,exhausted;
    portENTER_CRITICAL(&state_lock);
    *candidate=pz_outputs;
    busy=pz_deferred.pending||om_deferred.pending||tm_deferred.pending||__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)||pz_preview.active||
        (before_deadline(now,sim_deadline)&&sim_mask)||
        (before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF);
    stale=pz_revision[lane]!=base_revision;
    exhausted=pz_revision[lane]==UINT16_MAX;
    portEXIT_CRITICAL(&state_lock);
    if(busy){xSemaphoreGive(om_write_mutex);return 2;}
    if(stale||exhausted){xSemaphoreGive(om_write_mutex);return 3;}
    if(memcmp(&candidate->outputs[lane],next,sizeof(*next))==0){
        portENTER_CRITICAL(&state_lock);
        ot_pz_preview_cancel(&pz_preview);
        portEXIT_CRITICAL(&state_lock);
        request_render_refresh();
        xSemaphoreGive(om_write_mutex);return 0;
    }
    candidate->outputs[lane]=*next;
    if(!ot_pz_valid(candidate)){xSemaphoreGive(om_write_mutex);return 1;}
    const bool saved=pz_save_controller_locked(candidate);
    if(saved){
        portENTER_CRITICAL(&state_lock);
        ot_pz_preview_cancel(&pz_preview);
        portEXIT_CRITICAL(&state_lock);
        request_render_refresh();
    }
    xSemaphoreGive(om_write_mutex);
    return saved?0:4;
}
/* GATT callback supplies the live HMAC and capability-negotiation result.
 * This function only updates a volatile CORE diagnostic snapshot; RMT/DMA
 * remains exclusively owned by led_task. */
bool ot_ble_pixel_probe(const uint8_t *packet,size_t size,
                        bool authenticated,bool capability_seen){
    uint16_t lengths[OT_PZ_OUTPUTS];bool accepted=false;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    portENTER_CRITICAL(&state_lock);
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        lengths[lane]=pz_outputs.outputs[lane].active_pixels?
            pz_outputs.outputs[lane].active_pixels:OT_PZ_MAX_PIXELS;
    if(native_outputs_ready)
        accepted=ot_pz_diag_probe(&pz_diag,packet,size,authenticated,
                                  capability_seen,lengths,now_ms);
    portEXIT_CRITICAL(&state_lock);
    if(accepted)request_render_refresh();
    return accepted;
}
void ot_ble_diag_disconnect(void){
    bool active;
    portENTER_CRITICAL(&state_lock);
    active=pz_diag.mode!=OT_PZ_DIAG_NONE||pz_preview.active;
    ot_pz_diag_clear(&pz_diag);
    pz_preview_cancel_pending_locked();
    ot_pz_preview_cancel(&pz_preview);
    pz_reset_challenge.active=false;
    portEXIT_CRITICAL(&state_lock);
    if(active)request_render_refresh();
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
    if(!input_sampler_ready||!config_promotion_ready||config_recovery_fault)return 2; /* Cannot activate durable config. */
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
    int code=busy?2:(stale||tm_revision==UINT16_MAX)?3:0;
    if(!code){
        if(!tm_save(next))code=4;
        else {
            /* Durable staging never changes the active physical role map.
             * The promoter persists and validates the active generation. */
            portENTER_CRITICAL(&state_lock);
            tm_deferred.profile=*next;
            tm_deferred.pending=true;
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
    ot_tm_profile_t cfg;bool saved,pending;uint8_t physical,sim;uint16_t config_revision,active_config_revision;
    TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    cfg=tm_profile;pending=tm_deferred.pending;saved=tm_persistent&&!pending;
    physical=live_input_mask;config_revision=tm_revision;
    active_config_revision=tm_active_revision;
    sim=before_deadline(now,sim_deadline)?sim_mask:0;
    portEXIT_CRITICAL(&state_lock);
    cJSON *root=cJSON_CreateObject();if(!root)return respond_err(r,500,"out of memory");
    /* An incomplete trigger map must never be acknowledged as HTTP 200. */
    if(!cJSON_AddNumberToObject(root,"revision",OT_TM_VERSION) ||
       !cJSON_AddNumberToObject(root,"config_revision",config_revision) ||
       !cJSON_AddNumberToObject(root,"active_config_revision",active_config_revision) ||
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
    if(!require_revision)return respond_err(r,409,"expected_config_revision required; read current revision");
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
    ot_show_t cfg;bool active;uint32_t remaining=0;bool ready,persistent=false,continuous=false;
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
            if(slot->continuous){continuous=true;continue;}
            if(slot->standalone){persistent=true;continue;}
            const uint32_t duration=(uint32_t)slot->request.ttl_sec*1000u;
            const uint32_t elapsed=ms-slot->refreshed_ms;
            if(duration-elapsed>remaining)remaining=duration-elapsed;
        }
        if(persistent)remaining=0;
        if(continuous)remaining=0;
    }else if(active){
        remaining=(uint32_t)((uint32_t)(native_show_deadline-now)*portTICK_PERIOD_MS);
    }
    ready=native_outputs_ready;
    portEXIT_CRITICAL(&state_lock);
    ot_wled_fx_metadata_t meta;
    const char *active_name=fx_active&&ot_effects_1d_metadata(fx_effect_id,&meta)?
      meta.name:ot_show_name(cfg.mode);
    char buf[430];
    snprintf(buf,sizeof(buf),"{\"active\":%s,\"effect\":\"%s\",\"remaining_ms\":%lu,\"persistent\":%s,\"continuous\":%s,\"brightness\":%u,\"speed\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"chain_mask\":%u,\"section_mask\":%u,\"bench_only\":true,\"outputs_ready\":%s}",
      active?"true":"false",active?active_name:"off",(unsigned long)remaining,persistent?"true":"false",continuous?"true":"false",
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
    cfg.brightness=bounded_int(obj,"brightness",24,0,255);
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
    ot_pz_diag_clear(&pz_diag);
    pz_preview_cancel_pending_locked();
    ot_pz_preview_cancel(&pz_preview);
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
                                      const ot_fx_overlay_config_t *overlay,bool continuous,
                                      const ot_ble_fx_tertiary_t *tertiary){
    ot_wled_fx_metadata_t meta;
    if((overlay&&!ot_fx_overlay_valid(overlay))||
       !req||!ot_effects_1d_metadata(req->effect_id,&meta)||
       !ot_palette_1d_supported(req->effect_id,req->palette_id)||
       req->chain_mask==0||
       (req->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))!=0||req->zone_mask==0||
       req->ttl_sec<1||req->ttl_sec>60||
       !palette_resolved(req->palette_id))
        return false;
    const TickType_t now=xTaskGetTickCount();
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    bool ready;
    portENTER_CRITICAL(&state_lock);
    ready=native_outputs_ready;
    if(ready){
        /* Apply only the requested output/zone slots. Other zones retain FX phase. */
        ready=continuous?
            ot_fx_bank_apply_continuous_tertiary_identity(&fx_bank,req,
                om_outputs,pz_outputs.outputs,tertiary,now_ms):
            ot_fx_bank_apply_configured_identity(&fx_bank,req,om_outputs,
                pz_outputs.outputs,now_ms,standalone);
        uint8_t resolved[OT_OM_OUTPUTS][OT_OM_ZONES];
        for(unsigned o=0;o<OT_OM_OUTPUTS&&ready;o++){
            memset(resolved[o],0xff,sizeof(resolved[o]));
            if((req->chain_mask&(1u<<o))&&
               !ot_fx_bank_resolve_zone_identity(&pz_outputs.outputs[o],
                   &fx_bank,o,now_ms,resolved[o]))ready=false;
        }
        if(ready&&spark){
            /* Optional sparkle tuning is transient per-logical-zone state. */
            for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
                if(!(req->chain_mask&(1u<<o)))continue;
                for(unsigned z=0;z<om_outputs[o].zone_count;z++){
                    if(!(req->zone_mask&(1u<<z))||
                       om_outputs[o].zones[z].pixels==0||
                       om_outputs[o].zones[z].role==OT_OM_OFF||
                       resolved[o][z]>=OT_OM_ZONES)continue;
                    fx_bank.zones[o][resolved[o][z]].spark=*spark;
                }
            }
        }
        if(ready&&overlay){
            for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
                if(!(req->chain_mask&(1u<<o)))continue;
                for(unsigned z=0;z<om_outputs[o].zone_count;z++){
                    if(!(req->zone_mask&(1u<<z))||!om_outputs[o].zones[z].pixels||
                       om_outputs[o].zones[z].role==OT_OM_OFF||
                       resolved[o][z]>=OT_OM_ZONES)continue;
                    fx_bank.zones[o][resolved[o][z]].overlay=*overlay;
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
    if(ready)request_render_refresh();
    return ready;
}
bool ot_ble_apply_fx(const ot_ble_fx_packet_t *req){
    return ot_ble_apply_fx_internal(req,false,NULL,NULL,false,NULL); /* live 60-sec bench preview */
}
bool ot_ble_apply_continuous_fx(const ot_ble_fx_packet_t *req){
    return ot_ble_apply_fx_internal(req,true,NULL,NULL,true,NULL);
}
/* Additional volatile third RGB is always scoped to one authenticated
 * continuous request and never enters preset/NVS or standalone records. */
bool ot_ble_apply_continuous_fx_tertiary(const ot_ble_fx_packet_t *req,
                                         const ot_ble_fx_tertiary_t *tertiary){
    return tertiary&&tertiary->set?
        ot_ble_apply_fx_internal(req,true,NULL,NULL,true,tertiary):false;
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
        om_number(obj,"brightness",&bri,0,255)&&
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
    if(!ot_ble_apply_fx_internal(&req,false,NULL,&overlay,false,NULL))
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
        .brightness=(uint8_t)bounded_int(obj,"brightness",20,0,255),
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
    if(!ot_ble_apply_fx_internal(&req,false,&spark,NULL,false,NULL))
        return respond_err(r,409,"no configured active output zones or outputs not ready");
    char payload[185];
    snprintf(payload,sizeof(payload),
       "{\"ok\":true,\"event\":\"%s\",\"duration_ms\":%lu,\"preview_ttl_sec\":6,\"brightness_cap\":%u,\"bench_only\":true}",
       event_key,(unsigned long)ot_auto_fx_duration_ms(speed),(unsigned)OT_MAX_BRIGHTNESS);
    return send_json(r,payload);
}
/* One software STOP owner for both BLE and HTTP. It never writes GPIOs,
 * clears physical input state, or suppresses Brake/Turn/Hazards/Reverse.
 * An already-COMMITTING geometry transaction is deliberately preserved. */
static void core_stop_all_sources(void){
    const TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    demo.mode=OT_DEMO_OFF;demo_deadline=now;
    sim_mask=0u;sim_deadline=now;
    ble_owns_show=false;ble_fx_enabled=false;
    ot_fx_bank_stop_all(&fx_bank);
    native_show.mode=OT_SHOW_OFF;native_show_deadline=now;
    if(pz_diag.mode!=OT_PZ_DIAG_NONE)ot_pz_diag_clear(&pz_diag);
    const bool geometry_commit_active=pz_preview.committing||
        pz_preview_commit_requested||pz_preview_commit_worker_active;
    if(!geometry_commit_active&&pz_preview.active){
        pz_preview_cancel_pending_locked();
        ot_pz_preview_cancel(&pz_preview);
    }
    stop_request_generation++;
    portEXIT_CRITICAL(&state_lock);
    request_render_refresh();
    ot_st_show_active(false);
}
void ot_ble_stop_show(void){
    core_stop_all_sources();
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
         ot_ble_apply_fx_internal(&legacy.fx,true,NULL,NULL,false,NULL);
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
            .color={0,0,0},.brightness=255u,.speed=0,
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
    for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++)
        configured_pixels[ch]=pz_outputs.outputs[ch].active_pixels;
    stop_requested=stop_request_generation;stop_rendered=stop_render_generation;
    partial_frames=incomplete_frames;
    faulted_outputs=output_runtime_fault_mask;
    portEXIT_CRITICAL(&state_lock);
    const bool ble_radio_ready=ot_ble_radio_ever_ready();
    const uint8_t commit_phase=__atomic_load_n(&pz_pair_commit_phase,__ATOMIC_ACQUIRE);
    const uint32_t commit_transaction=__atomic_load_n(&pz_pair_commit_phase_txn,__ATOMIC_ACQUIRE);
    const bool commit_pending=__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
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
    char buf[2700];char led_json[384],wd_json[420];
    ot_st_format_json(led_json,sizeof(led_json));
    ot_sw_idf_status_json(wd_json,sizeof(wd_json));
    snprintf(buf,sizeof(buf),"{\"name\":\"OpenTail CORE Alpha\",\"version\":\"%s\",\"boot_id\":%lu,\"elf_sha256\":\"%s\",\"bench_only\":true,\"hardware_failover\":false,\"ble_ready\":%s,\"ble_radio_ready\":%s,\"outputs_ready\":%s,\"mode\":\"%s\",\"input_mask\":%u,\"simulated_mask\":%u,\"physical_function_mask\":%u,\"effective_mask\":%u,\"demo_active\":%s,\"native_show_active\":%s,\"frames\":%lu,\"measured_fps\":%lu,\"target_fps\":%u,\"active_pixels_per_output\":%s,\"leds_per_chain\":%u,\"brightness_cap\":%u,\"output_refreshes\":[%lu,%lu],\"incomplete_frames\":%lu,\"failed_output_mask\":%lu,\"stop_requested\":%lu,\"stop_rendered\":%lu,\"config_commit_phase\":%u,\"config_commit_transaction_id\":%lu,\"config_commit_pending\":%s,\"status_led\":%s,\"arbitration\":%s,\"watchdog\":%s}",
       OT_FW,(unsigned long)core_boot_id,elf_sha256,
       ble_startup_ready?"true":"false",ble_radio_ready?"true":"false",
       outputs_ready?"true":"false","user-configured",
       (unsigned)physical,(unsigned)sim,(unsigned)function_mask,(unsigned)effective,mode==OT_DEMO_OFF?"false":"true",show_active?"true":"false",
       (unsigned long)frames,(unsigned long)observed_fps,(unsigned)target_fps,active_pixels_json,(unsigned)configured_pixels[0],(unsigned)layout.brightness_cap,
       (unsigned long)refresh0,(unsigned long)refresh1,(unsigned long)partial_frames,
       (unsigned long)faulted_outputs,(unsigned long)stop_requested,(unsigned long)stop_rendered,
        (unsigned)commit_phase,(unsigned long)commit_transaction,
        commit_pending?"true":"false",led_json,arb_json,wd_json);
    return send_json(r,buf);
}
static esp_err_t caps_get(httpd_req_t *r) {
    char buf[2048];
    const int n=snprintf(buf,sizeof(buf),
        "{\"protocol\":\"opentail-core-alpha-v1\",\"bench_only\":true,\"pixel_protocol\":\"WS2815-compatible RMT 800kHz\",\"color_order\":\"GRB\",\"color_order_configurable\":true,\"wled_renderer\":\"compatibility\",\"wled_pixel_parity_verified\":false,\"native_wled_firmware_unified\":false,\"outputs\":2,\"physical_inputs\":8,\"active_functions\":[\"park\",\"brake\",\"left\",\"right\",\"reverse\",\"aux1\",\"aux2\",\"aux3\"],\"reserved_inputs\":[],\"wifi\":\"WPA2 Access Point\",\"bluetooth\":true,\"ble_status_only\":false,\"ble_studio_controls\":false,\"ble_bench_show_v1\":true,\"ble_output_config_v1\":true,\"ble_session_requires_wifi\":true,\"firmware_sequences\":false,\"native_show_engine\":true,\"native_effects\":[\"solid\",\"chase\",\"breathe\",\"wipe\",\"rainbow\"],\"wled_1d_effect_count\":120,\"ble_effect_id_count\":140,\"target_fps\":120,\"fps_policy\":\"adaptive_load_53_to_120\",\"preferred_fps_min\":53,\"preferred_fps_max\":120,\"studio_embedded\":false,\"developer_console\":true,\"studio_wled_api_compatible\":false,\"hardware_failover\":false,\"trigger_arbitration_v1\":true,\"trigger_manager_v1\":true,\"output_manager_v1\":true,\"output_manager_nvs\":true,\"status_led\":true,\"status_led_control\":false,\"panel_zone_schema\":3,\"live_arrangement_preview\":true,\"pixel_probe\":true,\"brightness_contract\":\"8-bit RGB channel values; one effective CORE ceiling applied at final pixel output\",\"brightness_cap\":%u,\"brightness_cap_is_measured_current_limit\":false,\"brightness_ceiling_configurable\":true}",
        (unsigned)OT_MAX_BRIGHTNESS_LIMIT);
    if(n<0||(size_t)n>=sizeof(buf))
        return respond_err(r,500,"capabilities formatting overflow");
    return send_json(r,buf);
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
    unsigned brightness=bounded_int(obj,"brightness",32,0,255);
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
    core_stop_all_sources();
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
/* ESP-IDF 5.5.x esp_timer_get_time is ISR-safe/IRAM-backed; capture only the
 * two turn channels so the task can classify edge time despite wake jitter. */
static void IRAM_ATTR input_edge_isr(void *arg){
    const unsigned input_index=(unsigned)(uintptr_t)arg-1u;
    /* Reject an invalid callback index before any pin/counter/queue lookup. */
    if(input_index>=OT_IN_COUNT)return;
    int level=1;
    bool asserted=false;
    if(input_index<OT_INPUT_DIAG_INPUTS){
        /* LL read is IRAM-safe; normalized asserted=1 honors active-low wiring. */
        level=gpio_ll_get_level(&GPIO,(uint32_t)input_pin_isr[input_index]);
        asserted=OT_INPUT_ACTIVE_LOW?(level==0):(level!=0);
        __atomic_fetch_add(&input_raw_edges[input_index],1u,__ATOMIC_RELAXED);
        if(asserted)
            __atomic_fetch_add(&input_asserted_edges[input_index],1u,__ATOMIC_RELAXED);
        else
            __atomic_fetch_add(&input_deasserted_edges[input_index],1u,__ATOMIC_RELAXED);
        const uint32_t bit=1u<<input_index;
        if(asserted)__atomic_fetch_or(&input_raw_level_mask,bit,__ATOMIC_RELAXED);
        else __atomic_fetch_and(&input_raw_level_mask,~bit,__ATOMIC_RELAXED);
    }
    if(input_index==2u || input_index==3u){
        const unsigned slot=input_index-2u;
        /* Mark the producer active before timestamp capture. The dual-core
         * sampler defers deadline expiry if this edge is not yet published. */
        __atomic_store_n(&turn_edge_writer_active[slot],1u,__ATOMIC_RELEASE);
        const uint64_t edge_time_us=(uint64_t)esp_timer_get_time();
        const uint32_t head=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_RELAXED);
        const uint32_t tail=__atomic_load_n(&turn_edge_tail[slot],__ATOMIC_ACQUIRE);
        if((uint32_t)(head-tail)>=OT_TURN_EDGE_QUEUE_CAPACITY){
            /* Preserve the existing SPSC queue; telemetry counts each full-ring edge. */
            __atomic_store_n(&turn_edge_overflow[slot],1u,__ATOMIC_RELEASE);
            __atomic_fetch_add(&turn_edge_overflow_count[slot],1u,__ATOMIC_RELAXED);
        }else{
            ot_turn_edge_event_t *event=
                &turn_edge_events[slot][head&OT_TURN_EDGE_QUEUE_MASK];
            event->timestamp_us=edge_time_us;
            event->asserted=asserted;
            __atomic_store_n(&turn_edge_head[slot],head+1u,__ATOMIC_RELEASE);
            const uint32_t depth=(uint32_t)(head+1u-tail);
            uint32_t high=__atomic_load_n(&turn_edge_high_water[slot],__ATOMIC_RELAXED);
            /* One bounded best-effort high-water update; the existing ISR is
             * the sole producer for this lane. A racing update may under-report
             * the diagnostic maximum, but it can never extend ISR latency. */
            if(depth>high)(void)__atomic_compare_exchange_n(
                &turn_edge_high_water[slot],&high,depth,true,
                __ATOMIC_RELAXED,__ATOMIC_RELAXED);
        }
        __atomic_store_n(&turn_edge_writer_active[slot],0u,__ATOMIC_RELEASE);
    }
    (void)__atomic_add_fetch(&input_edge_epoch,1u,__ATOMIC_RELAXED);
}

void ot_input_diag_snapshot_get(ot_input_diag_snapshot_t *out){
    if(!out)return;
    memset(out,0,sizeof(*out));
    out->raw_asserted_mask=(uint8_t)__atomic_load_n(&input_raw_level_mask,__ATOMIC_ACQUIRE);
    portENTER_CRITICAL(&state_lock);
    out->debounced_asserted_mask=(uint8_t)(live_input_mask&((1u<<OT_INPUT_DIAG_INPUTS)-1u));
    portEXIT_CRITICAL(&state_lock);
    out->edge_epoch=__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE);
    for(unsigned i=0;i<OT_INPUT_DIAG_INPUTS;i++){
        out->raw_edges[i]=__atomic_load_n(&input_raw_edges[i],__ATOMIC_RELAXED);
        out->asserted_edges[i]=__atomic_load_n(&input_asserted_edges[i],__ATOMIC_RELAXED);
        out->deasserted_edges[i]=__atomic_load_n(&input_deasserted_edges[i],__ATOMIC_RELAXED);
    }
    for(unsigned slot=0;slot<2u;slot++){
        const uint32_t head=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);
        const uint32_t tail=__atomic_load_n(&turn_edge_tail[slot],__ATOMIC_ACQUIRE);
        const uint32_t depth=(uint32_t)(head-tail);
        out->turn_queue_depth[slot]=(uint8_t)(depth>UINT8_MAX?UINT8_MAX:depth);
        const uint32_t high=__atomic_load_n(&turn_edge_high_water[slot],__ATOMIC_RELAXED);
        out->turn_queue_high_water[slot]=(uint8_t)(high>UINT8_MAX?UINT8_MAX:high);
        out->turn_queue_overflow_count[slot]=
            __atomic_load_n(&turn_edge_overflow_count[slot],__ATOMIC_RELAXED);
        out->turn_queue_overflow_flag[slot]=
            (uint8_t)(__atomic_load_n(&turn_edge_overflow[slot],__ATOMIC_ACQUIRE)!=0u);
    }
}

static bool input_init(void) {
    for(int i=0;i<OT_IN_COUNT;++i){
        input_pin_isr[i]=input_pin[i];
        gpio_config_t cfg={.pin_bit_mask=1ULL<<input_pin[i],.mode=GPIO_MODE_INPUT,
                           .pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,
                           .intr_type=GPIO_INTR_ANYEDGE};
        const esp_err_t e=gpio_config(&cfg);
        if(e!=ESP_OK){
            ESP_LOGE(TAG,"Trigger input IN%u GPIO setup failed: %s",
                     (unsigned)(i+1),esp_err_to_name(e));
            return false;
        }
    }
    esp_err_t e=gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if(e!=ESP_OK){
        ESP_LOGE(TAG,"GPIO edge observer unavailable: %s",esp_err_to_name(e));
        return false;
    }
    for(unsigned i=0;i<OT_IN_COUNT;i++){
        e=gpio_isr_handler_add(input_pin[i],input_edge_isr,(void *)(uintptr_t)(i+1u));
        if(e!=ESP_OK){
            ESP_LOGE(TAG,"GPIO edge handler failed IN%u",i+1u);
            return false;
        }
    }
    return true;
}
/* Establish a coherent overflow recovery baseline. The ISR marks itself
 * active before timestamp capture; a changed head or active writer means the
 * GPIO level and queue discard boundary do not yet describe one instant. */
static bool turn_edge_queue_resync(unsigned slot,ot_signal_debouncer_t *state,
                                   uint64_t *baseline_us){
    if(slot>=2u||!state||!baseline_us)return false;
    if(__atomic_load_n(&turn_edge_writer_active[slot],__ATOMIC_ACQUIRE)!=0u)
        return false;
    const uint32_t head_before=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);
    const uint64_t now=(uint64_t)esp_timer_get_time();
    const int level=gpio_get_level(input_pin[slot+2u]);
    const bool raw=OT_INPUT_ACTIVE_LOW?(level==0):(level!=0);
    const uint32_t head_after=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);
    if(__atomic_load_n(&turn_edge_writer_active[slot],__ATOMIC_ACQUIRE)!=0u ||
       head_after!=head_before)
        return false;
    __atomic_store_n(&turn_edge_tail[slot],head_after,__ATOMIC_RELEASE);
    ot_signal_debouncer_resync(state,raw);
    *baseline_us=now;
    /* An ISR that found the ring full while the discard boundary was being
     * installed must be handled by another coherent resync next sampler tick. */
    return __atomic_load_n(&turn_edge_overflow[slot],__ATOMIC_ACQUIRE)==0u;
}

/* Consume all ISR-published turn edges up to this sampler time. Events are
 * replayed in timestamp order before the absolute release deadline is tested.
 * If a producer is in flight or publishes after the cutoff, defer this lane's
 * sample: never expire against a queue head that may be missing an earlier edge. */
static bool turn_edge_queue_update(unsigned slot,ot_signal_debouncer_t *state,
                                   uint64_t now_us,uint64_t boot_snapshot_us,
                                   bool *sample_valid){
    if(sample_valid)*sample_valid=false;
    if(slot>=2u||!state||!sample_valid)return false;
    bool overflow=__atomic_exchange_n(&turn_edge_overflow[slot],0u,__ATOMIC_ACQ_REL)!=0u;
    if(overflow){
        uint64_t baseline_us=0u;
        if(!turn_edge_queue_resync(slot,state,&baseline_us)){
            __atomic_store_n(&turn_edge_overflow[slot],1u,__ATOMIC_RELEASE);
            return state->asserted;
        }
        ESP_LOGE(TAG,"Turn input %u edge history overflow; held state cleared and raw input requalified",
                 slot==0u?3u:4u);
        *sample_valid=true;
        return ot_signal_debouncer_sample(state,baseline_us,OT_DEBOUNCE_TICKS);
    }

    uint32_t tail=__atomic_load_n(&turn_edge_tail[slot],__ATOMIC_RELAXED);
    const uint32_t head=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);
    while(tail!=head){
        const ot_turn_edge_event_t event=
            turn_edge_events[slot][tail&OT_TURN_EDGE_QUEUE_MASK];
        if(event.timestamp_us>now_us)break;
        tail++;
        /* Ignore history at/before the boot GPIO baseline. */
        if(event.timestamp_us>boot_snapshot_us)
            ot_signal_debouncer_transition(state,event.asserted,event.timestamp_us);
    }
    __atomic_store_n(&turn_edge_tail[slot],tail,__ATOMIC_RELEASE);

    /* Catch overflow that occurred while the sampler was draining. */
    if(__atomic_exchange_n(&turn_edge_overflow[slot],0u,__ATOMIC_ACQ_REL)!=0u){
        uint64_t baseline_us=0u;
        if(!turn_edge_queue_resync(slot,state,&baseline_us)){
            __atomic_store_n(&turn_edge_overflow[slot],1u,__ATOMIC_RELEASE);
            return state->asserted;
        }
        ESP_LOGE(TAG,"Turn input %u edge history overflow; held state cleared and raw input requalified",
                 slot==0u?3u:4u);
        *sample_valid=true;
        return ot_signal_debouncer_sample(state,baseline_us,OT_DEBOUNCE_TICKS);
    }

    /* The producer may have started after the initial head snapshot. If its
     * timestamp was captured before this sample cutoff, wait for publication. */
    const uint32_t head_after=__atomic_load_n(&turn_edge_head[slot],__ATOMIC_ACQUIRE);
    if(__atomic_load_n(&turn_edge_writer_active[slot],__ATOMIC_ACQUIRE)!=0u ||
       head_after!=head)
        return state->asserted;

    *sample_valid=true;
    return ot_signal_debouncer_sample(state,now_us,OT_DEBOUNCE_TICKS);
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
    for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++){
        /* Fresh/unconfigured output uses the bounded raw-array fallback extent.
         * This is a runtime buffer budget, never a persisted strip-length claim. */
        const unsigned pixels=pz_outputs.outputs[ch].active_pixels?
            pz_outputs.outputs[ch].active_pixels:OT_PZ_MAX_PIXELS;
        if(!strip_configure_channel(ch,pixels))return false;
    }
    return true;
}
/* Low-priority durable promoter. The independent input sampler never waits
 * for NVS. While staged exists, boot always restores its previous configuration
 * even when active_v3 has been written and power is cut during promotion. */
static bool config_activation_idle(uint32_t epoch,bool allow_show){
    const uint8_t instantaneous=tm_gpio_input_mask();
    const uint32_t observed_epoch=__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE);
    if(instantaneous!=0u||observed_epoch!=epoch)return false;
    const TickType_t now=xTaskGetTickCount();
    portENTER_CRITICAL(&state_lock);
    const bool quiet=ot_pz_activation_inputs_quiet(
        instantaneous,live_input_mask,input_quiet_samples,OT_DEBOUNCE_TICKS,
        __atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE),epoch);
    const bool idle=quiet &&
        !(before_deadline(now,sim_deadline)&&sim_mask!=0u) &&
        !(before_deadline(now,demo_deadline)&&demo.mode!=OT_DEMO_OFF) &&
        (allow_show||!show_window(now));
    portEXIT_CRITICAL(&state_lock);
    return idle&&tm_gpio_input_mask()==0u &&
        __atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch;
}
static bool config_write_active(nvs_handle_t h,const void *data,size_t n){
    esp_err_t e=nvs_set_blob(h,OT_CFG_JOURNAL_KEY_ACTIVE,data,n);
    if(e==ESP_OK)e=nvs_commit(h);
    return e==ESP_OK;
}
static bool config_clear_stage(nvs_handle_t h){
    esp_err_t e=nvs_erase_key(h,OT_CFG_JOURNAL_KEY_STAGED);
    if(e==ESP_OK)e=nvs_commit(h);
    if(e!=ESP_OK)return false;
    /* Explicitly verify the NVS marker is no longer readable. */
    size_t n=0;
    return nvs_get_blob(h,OT_CFG_JOURNAL_KEY_STAGED,NULL,&n)==ESP_ERR_NVS_NOT_FOUND;
}
/* The durable active record retains BOTH the prior and the desired profile.
 * After stage erasure, a concurrent GPIO edge may veto RAM publication.
 * A later promoter retries from active_v3; reboot does likewise. No edge
 * can make the newer profile active merely because stage_v3 disappeared. */
static void tm_promote(uint32_t epoch){
    nvs_handle_t h;
    if(nvs_open("ot_trig_cfg",NVS_READWRITE,&h)!=ESP_OK)return;
    ot_tm_journal_t stage={0};
    esp_err_t read=tm_journal_read(h,OT_CFG_JOURNAL_KEY_STAGED,&stage);
    const bool staged=read==ESP_OK;
    if(read==ESP_ERR_NVS_NOT_FOUND)
        read=tm_journal_read(h,OT_CFG_JOURNAL_KEY_ACTIVE,&stage);
    if(read!=ESP_OK){
        nvs_close(h);config_recovery_fault=true;
        ot_st_set_fault("trigger journal invalid during promotion");return;
    }
    bool matches=false;
    portENTER_CRITICAL(&state_lock);
    matches=tm_deferred.pending&&
        tm_revision==stage.candidate_revision&&
        tm_active_revision==stage.previous_revision&&
        memcmp(&tm_profile,&stage.previous,sizeof(tm_profile))==0&&
        memcmp(&tm_deferred.profile,&stage.candidate,sizeof(tm_deferred.profile))==0;
    portEXIT_CRITICAL(&state_lock);
    if(!matches){
        nvs_close(h);config_recovery_fault=true;
        ot_st_set_fault("trigger staging revision mismatch");return;
    }
    if(!config_activation_idle(epoch,false)){nvs_close(h);return;}
    if(staged){
        if(!config_write_active(h,&stage,sizeof(stage))||
           !config_activation_idle(epoch,false)){nvs_close(h);return;}
        /* CRITICAL ORDER: erase staged recovery marker BEFORE publishing
         * to active RAM, never after; active_v3 itself keeps prior recovery. */
        if(!config_clear_stage(h)){
            nvs_close(h);config_recovery_fault=true;
            ot_st_set_fault("trigger journal finalization failed");return;
        }
    }
    /* GPIO can change during NVS erase; no RAM swap occurs unless the
     * complete durable boundary and the original edge epoch remain quiet. */
    if(!config_activation_idle(epoch,false)){nvs_close(h);return;}
    bool applied=false;
    portENTER_CRITICAL(&state_lock);
    if(input_quiet_samples>=OT_DEBOUNCE_TICKS && live_input_mask==0u &&
       tm_gpio_input_mask()==0u &&
       __atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch){
        applied=ot_tm_apply_if_released(&tm_profile,&tm_deferred,0u,0u);
        if(applied)tm_active_revision=stage.candidate_revision;
    }
    portEXIT_CRITICAL(&state_lock);
    if(applied)request_render_refresh();
    nvs_close(h);
}
static void pz_promote(uint32_t epoch){
    ot_pz_pair_store_workspace_t *workspace=&pz_pair_workspace;
    if(!config_activation_idle(epoch,true))return;
    if(__atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE)){
        const uint32_t transaction_id=__atomic_load_n(
            &pz_pair_transaction_id,__ATOMIC_RELAXED);
        if(!ot_pz_pair_store_finish(&pz_pair_store_io,workspace,transaction_id,
                                    pz_pair_phase_hook,NULL)){
            config_recovery_fault=true;
            ot_st_set_fault("v4 lane/map pair promotion failed; reboot recovery required");
            return;
        }
        __atomic_store_n(&pz_pair_commit_pending,false,__ATOMIC_RELEASE);
    }
    uint16_t active_revision[OT_PZ_OUTPUTS]={0},staged_revision[OT_PZ_OUTPUTS]={0};
    bool intent_pending=false;uint32_t recovered_transaction=0u;uint8_t reset_mask=0u;
    if(!ot_pz_pair_store_snapshot(&pz_pair_store_io,workspace)||
       !workspace->snapshot.has_map_active||!workspace->snapshot.has_lane_active||
       workspace->snapshot.map_active.transaction_id!=
           workspace->snapshot.lane_active.transaction_id||
       !ot_pz_join(&workspace->snapshot.lane_active.candidate,
                   &workspace->snapshot.map_active.candidate,&workspace->candidate)||
       !ot_pz_pair_store_recover(&pz_pair_store_io,workspace,NULL,
            &workspace->previous,&workspace->pending,active_revision,
            staged_revision,&intent_pending,&recovered_transaction,&reset_mask)||
       intent_pending||recovered_transaction!=workspace->snapshot.map_active.transaction_id){
        config_recovery_fault=true;
        ot_st_set_fault("v4 lane/map active pair readback mismatch");return;
    }
    bool matches=false;
    portENTER_CRITICAL(&state_lock);
    if(pz_deferred.pending){
        matches=workspace->pending.pending&&
            memcmp(&pz_outputs,&workspace->previous,sizeof(pz_outputs))==0&&
            memcmp(&pz_deferred.candidate,&workspace->pending.candidate,
                   sizeof(pz_deferred.candidate))==0&&
            memcmp(pz_active_revision,active_revision,sizeof(active_revision))==0&&
            memcmp(pz_revision,staged_revision,sizeof(staged_revision))==0;
    }else{
        matches=memcmp(&pz_outputs,&workspace->candidate,sizeof(pz_outputs))==0;
    }
    portEXIT_CRITICAL(&state_lock);
    if(!matches){
        config_recovery_fault=true;
        ot_st_set_fault("v4 lane/map RAM-to-journal generation mismatch");return;
    }
    if(!config_activation_idle(epoch,true))return;
    bool applied=false,reset_cleanup=false;
    const uint8_t effective_reset_mask=(uint8_t)(reset_mask|pz_reset_pending_mask);
    portENTER_CRITICAL(&state_lock);
    if(input_quiet_samples>=OT_DEBOUNCE_TICKS&&live_input_mask==0u&&
       tm_gpio_input_mask()==0u&&
       __atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch){
        if(pz_deferred.pending){
            pz_outputs=workspace->pending.candidate;
            pz_deferred.pending=false;
            applied=true;
        }
        memcpy(pz_active_revision,workspace->snapshot.map_active.candidate_revision,
               sizeof(pz_active_revision));
        memcpy(pz_revision,workspace->snapshot.map_active.candidate_revision,
               sizeof(pz_revision));
        __atomic_store_n(&pz_pair_commit_pending,false,__ATOMIC_RELEASE);pz_persistent=true;
        if(effective_reset_mask){
            pz_reset_pending_mask&=(uint8_t)~effective_reset_mask;
            pz_reset_challenge.active=false;
            ot_pz_diag_clear(&pz_diag);
            ot_pz_preview_cancel(&pz_preview);
            reset_cleanup=true;
        }
        ot_pz_preview_cancel(&pz_preview);
        pz_refresh_legacy_projection_locked();
    }
    portEXIT_CRITICAL(&state_lock);
    if(reset_cleanup)ot_fx_bank_stop_all(&fx_bank);
    if(applied||reset_cleanup)request_render_refresh();
}
static void config_activation_task(void *unused){
    (void)unused;
    for(;;){
        if(input_sampler_ready&&!config_recovery_fault&&om_write_mutex &&
           xSemaphoreTake(om_write_mutex,pdMS_TO_TICKS(1))==pdTRUE){
            bool trigger_pending,panel_pending;
            portENTER_CRITICAL(&state_lock);
            trigger_pending=tm_deferred.pending;
            panel_pending=pz_deferred.pending||pz_reset_pending_mask!=0u;
            portEXIT_CRITICAL(&state_lock);
            panel_pending=panel_pending||
                __atomic_load_n(&pz_pair_commit_pending,__ATOMIC_ACQUIRE);
            bool preview_commit=false;
            portENTER_CRITICAL(&state_lock);
            if(pz_preview_commit_requested){
                pz_pair_workspace.candidate=pz_preview_commit_candidate;
                pz_preview_commit_requested=false;
                pz_preview_commit_worker_active=true;
                preview_commit=true;
            }
            portEXIT_CRITICAL(&state_lock);
            if(preview_commit){
                const bool saved=pz_save_controller_locked(&pz_pair_workspace.candidate);
                portENTER_CRITICAL(&state_lock);
                pz_preview_commit_worker_active=false;
                if(!saved&&pz_preview.active&&pz_preview.committing&&
                   pz_preview.lane==pz_preview_commit_lane&&
                   pz_preview.base_revision==pz_preview_commit_lane_revision){
                    pz_preview.committing=false;
                    pz_preview.expires_ms=(uint32_t)(esp_timer_get_time()/1000LL)+
                                           OT_PZ_PREVIEW_LEASE_MS;
                }
                memset(&pz_preview_commit_candidate,0,sizeof(pz_preview_commit_candidate));
                memset(&pz_pair_workspace.candidate,0,sizeof(pz_pair_workspace.candidate));
                portEXIT_CRITICAL(&state_lock);
            }
            const uint32_t epoch=__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE);
            if(trigger_pending){
                if(config_activation_idle(epoch,false))tm_promote(epoch);
            }else if(panel_pending){
                if(config_activation_idle(epoch,true))pz_promote(epoch);
            }
            xSemaphoreGive(om_write_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(25u));
    }
}

/* The input sampler is independent of FPS. Four 10ms samples stay 40ms,
 * not four display frames. Wakes pixel rendering after a trigger transition. */
static void input_sampler_task(void *arg){
    (void)arg;
    ot_inputs_t sampler={0};
    ot_signal_debouncer_t turn_signal[2];
    /* Bootstrap both samplers from the GPIO snapshot captured BEFORE any
     * RMT frame. A held turn input must not acquire a phantom release timer. */
    const uint8_t boot_physical=live_input_mask;
    for(unsigned i=0;i<OT_IN_COUNT;i++){
        const bool asserted=(boot_physical&(uint8_t)(1u<<i))!=0u;
        sampler.settled[i]=asserted;
        sampler.pending[i]=asserted;
        sampler.ticks[i]=OT_DEBOUNCE_TICKS;
    }
    ot_signal_debouncer_init(&turn_signal[0],(boot_physical&OT_SIG_LEFT)!=0u);
    ot_signal_debouncer_init(&turn_signal[1],(boot_physical&OT_SIG_RIGHT)!=0u);
    TickType_t last_wake=xTaskGetTickCount();
    TickType_t period=pdMS_TO_TICKS(10u);
    if(!period)period=1u;
    for(;;){
        bool levels[OT_IN_COUNT];
        uint8_t instantaneous=0;
        for(unsigned i=0;i<OT_IN_COUNT;i++){
            const int level=gpio_get_level(input_pin[i]);
            levels[i]=OT_INPUT_ACTIVE_LOW?(level==0):(level!=0);
            if(levels[i])instantaneous|=(uint8_t)(1u<<i);
        }
        uint8_t physical=ot_debounce(&sampler,levels,OT_DEBOUNCE_TICKS);
        /* Replay every captured Left/Right transition before evaluating the
         * absolute release deadlines. Never clear physical turn history when
         * Brake is asserted: Brake > Turn is enforced downstream by arbitration,
         * while preserving turn state lets the signal resume without a reset. */
        const uint64_t now_us=(uint64_t)esp_timer_get_time();
        bool left_valid=false,right_valid=false;
        const bool left=turn_edge_queue_update(0u,&turn_signal[0],now_us,
            input_boot_snapshot_us,&left_valid);
        const bool right=turn_edge_queue_update(1u,&turn_signal[1],now_us,
            input_boot_snapshot_us,&right_valid);
        /* Publish all six unaffected inputs every sampler tick. A deferred
         * Left/Right lane retains its last committed bit, but cannot hold Brake,
         * Reverse, Park, AUX or the other valid turn lane stale. */
        uint8_t published=(uint8_t)(physical&
            (uint8_t)~(OT_SIG_LEFT|OT_SIG_RIGHT));
        portENTER_CRITICAL(&state_lock);
        const uint8_t previous=live_input_mask;
        if(left_valid){
            if(left)published|=OT_SIG_LEFT;
        }else{
            published=(uint8_t)((published&(uint8_t)~OT_SIG_LEFT)|
                                (previous&OT_SIG_LEFT));
        }
        if(right_valid){
            if(right)published|=OT_SIG_RIGHT;
        }else{
            published=(uint8_t)((published&(uint8_t)~OT_SIG_RIGHT)|
                                (previous&OT_SIG_RIGHT));
        }
        TaskHandle_t target=NULL;
        if(published!=previous){live_input_mask=published;target=render_task_handle;}
        /* Sampler never writes NVS or swaps a journaled profile. Physical
         * edges reset the independent quiet-window gate for the promoter. */
        if(instantaneous==0u && published==0u){
            if(input_quiet_samples<OT_DEBOUNCE_TICKS)input_quiet_samples++;
        }else input_quiet_samples=0u;
        portEXIT_CRITICAL(&state_lock);
        if(left_valid&&right_valid)
            ot_sw_idf_input_sampled(); /* only complete eight-input snapshots advance watchdog */
        if(target)xTaskNotifyGive(target);
        /* In-flight/unpublished turn edges defer only the sampler heartbeat;
         * unaffected physical inputs remain live and immediately arbitrated. */
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
static void stop_continuous_on_fault(void){
    portENTER_CRITICAL(&state_lock);
    ot_fx_bank_stop_continuous(&fx_bank);
    portEXIT_CRITICAL(&state_lock);
    request_render_refresh();
}
static void output_runtime_fault(unsigned ch,const char *step,esp_err_t error){
    ESP_LOGE(TAG,"RMT output %u %s failed: %s",ch,step,esp_err_to_name(error));
    ot_st_set_fault("external pixel RMT runtime failure");
    portENTER_CRITICAL(&state_lock);
    native_outputs_ready=false;
    output_runtime_fault_mask|=(1u<<ch);
    ot_pz_diag_clear(&pz_diag);
    pz_preview_cancel_pending_locked();
    ot_pz_preview_cancel(&pz_preview);
    ot_fx_bank_stop_continuous(&fx_bank); /* do not restart decorative playback after a fault */
    portEXIT_CRITICAL(&state_lock);
}
static void led_task(void *p) {
    (void)p;
    bool output_failed[OT_CHAIN_COUNT]={false};
    /* Allocation-free, per-output/per-zone states. One render invocation per zone/frame. */
    static ot_effects_1d_state_t fx_state[OT_CHAIN_COUNT][OT_OM_ZONES];
    /* Reused per-output snapshot prevents in-place overwrite during zone reorder. */
    static ot_effects_1d_state_t prior_fx_state[OT_OM_ZONES];
    /* Current logical zones resolve to stable identity-owned FX bank slots. */
    static uint8_t resolved_fx_bank_index[OT_CHAIN_COUNT][OT_OM_ZONES];
    static ot_wled_rgb_t fx_frame[OT_CHAIN_COUNT][OT_CHAIN_PIXELS];
    static uint32_t prior_fx_generation[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    static uint16_t prior_fx_zone_id[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    static uint16_t prior_fx_panel_id[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    static bool prior_fx_active[OT_CHAIN_COUNT][OT_OM_ZONES]={0};
    static uint8_t prior_pixel_zone_map[OT_CHAIN_COUNT][OT_PZ_MAX_PIXELS];
    static uint8_t pixel_zone_map[OT_CHAIN_COUNT][OT_PZ_MAX_PIXELS];
    static uint16_t pixel_logical_map[OT_CHAIN_COUNT][OT_PZ_MAX_PIXELS];
    /* One reusable compact logical-zone frame. */
    static ot_wled_rgb_t fx_zone_scratch[OT_PZ_MAX_PIXELS];
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
    if(!ot_sw_idf_watch_render()){
        ot_st_set_fault("single-ESP render watchdog registration failed");
        (void)esp_timer_delete(frame_timer);
        vTaskDelete(NULL);return;
    }
    portENTER_CRITICAL(&state_lock);
    render_task_handle=self;
    native_outputs_ready=true; /* RMT, frame timer and WDT initialized. */
    portEXIT_CRITICAL(&state_lock);
    while(1){
       const int64_t frame_started_us=esp_timer_get_time();
       TickType_t now=xTaskGetTickCount();
       uint8_t physical=0;
       ot_demo_t local_demo={0};ot_show_t local_show={0};uint32_t show_started=0;uint8_t combined=0;
       uint32_t local_stop_generation=0;
       uint8_t fallback_mask=0u;
       bool local_fx_enabled=false;ot_fx_zone_bank_t local_fx_bank={0};
       ot_om_output_t local_outputs[OT_OM_OUTPUTS];ot_pz_output_t local_panel[OT_PZ_OUTPUTS];
       ot_pz_diag_state_t local_diag={0};ot_tm_profile_t local_tm;
       portENTER_CRITICAL(&state_lock);
       /* Repeated reset: no decorative effect replay during recovery.
        * OEM physical GPIO trigger overlays remain authoritative. */
       if(ot_sw_idf_decorative_inhibited()){
           demo.mode=OT_DEMO_OFF;demo_deadline=0;
           native_show.mode=OT_SHOW_OFF;
           ble_fx_enabled=false;ble_owns_show=false;
           ot_pz_diag_clear(&pz_diag);
           pz_preview_cancel_pending_locked();
           ot_pz_preview_cancel(&pz_preview);
           ot_fx_bank_stop_all(&fx_bank);
       }
       const uint32_t render_now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
       (void)ot_pz_preview_expire(&pz_preview,render_now_ms);
       physical=live_input_mask;
       if(pz_preview.active&&physical==0u)
           memcpy(local_panel,pz_preview.candidate.outputs,sizeof(local_panel));
       else
           memcpy(local_panel,pz_outputs.outputs,sizeof(local_panel));
       uint16_t local_active_revision[OT_PZ_OUTPUTS];
       memcpy(local_active_revision,pz_active_revision,sizeof(local_active_revision));
       local_tm=tm_profile;
       local_stop_generation=stop_request_generation;
       combined=ot_tm_resolve(&local_tm,physical);
       fallback_mask=physical;
       if(before_deadline(now,sim_deadline)){
           combined|=sim_mask;fallback_mask|=sim_mask;
       }
       (void)ot_pz_diag_expire(&pz_diag,(uint32_t)(esp_timer_get_time()/1000LL));
       local_diag=pz_diag;
       if(before_deadline(now,demo_deadline)) local_demo=demo;
       if(show_window(now)){
           local_show=native_show;show_started=native_show_started_ms;
           local_fx_enabled=ble_fx_enabled;
           if(local_fx_enabled)memcpy(&local_fx_bank,&fx_bank,sizeof(local_fx_bank));
       } else {native_show.mode=OT_SHOW_OFF;ble_fx_enabled=false;}
       portEXIT_CRITICAL(&state_lock);
       ot_om_output_t default_outputs[OT_OM_OUTPUTS];
       ot_om_defaults(default_outputs);
       for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++)
           if(!ot_pz_to_fx_view(&local_panel[ch],ch,&local_outputs[ch]))
               local_outputs[ch]=default_outputs[ch];
       for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++){
           memset(resolved_fx_bank_index[ch],0xff,sizeof(resolved_fx_bank_index[ch]));
           if(local_fx_enabled&&
              !ot_fx_bank_resolve_zone_identity(&local_panel[ch],
                  &local_fx_bank,ch,render_now_ms,resolved_fx_bank_index[ch])){
               ot_st_set_fault("invalid FX Panel/Zone identity map");
               local_fx_enabled=false;
           }
       }
       /* One immutable snapshot of every requested custom palette per frame.
        * No locks or NVS access occur in the per-pixel/FX draw loops. */
       uint8_t palette_ids[OT_OM_OUTPUTS*OT_OM_ZONES];
       ot_palette_entry_t frame_palettes[OT_OM_OUTPUTS*OT_OM_ZONES];
       for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
           const uint8_t bank_z=resolved_fx_bank_index[o][z];
           const uint8_t id=bank_z<OT_OM_ZONES?
               local_fx_bank.zones[o][bank_z].request.palette_id:255u;
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
           bool any_output_completed=false;
           /* Reconfigure only after the preceding frame's async RMT transfers
            * have completed, and before starting ANY new lane transfer. */
           unsigned render_pixels[OT_CHAIN_COUNT]={0};
           for(unsigned ch=0;ch<OT_CHAIN_COUNT;ch++){
               render_pixels[ch]=local_panel[ch].active_pixels?
                   local_panel[ch].active_pixels:OT_PZ_MAX_PIXELS;
               ot_pz_build_pixel_map(&local_panel[ch],pixel_zone_map[ch],pixel_logical_map[ch]);
               if(!output_failed[ch]){
                   frame_ready[ch]=strip_configure_channel(ch,render_pixels[ch]);
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
               /* Render one compact logical frame per zone. Physical ranges are
                * gathered/scattered through the authoritative map; strip order
                * and RMT ownership remain exclusively physical and CORE-owned. */
               unsigned zone_global_start[OT_OM_ZONES]={0};
               unsigned logical_total=0u;
               for(unsigned z=0;z<output->zone_count;z++){
                   zone_global_start[z]=logical_total;
                   logical_total+=output->zones[z].pixels;
               }
               if(native_rendering&&local_fx_enabled){
                   uint16_t old_zone_id[OT_OM_ZONES],old_panel_id[OT_OM_ZONES];
                   uint32_t old_generation[OT_OM_ZONES];
                   bool old_active[OT_OM_ZONES];
                   memcpy(old_zone_id,prior_fx_zone_id[ch],sizeof(old_zone_id));
                   memcpy(old_panel_id,prior_fx_panel_id[ch],sizeof(old_panel_id));
                   memcpy(old_generation,prior_fx_generation[ch],sizeof(old_generation));
                   memcpy(old_active,prior_fx_active[ch],sizeof(old_active));
                   /* Renderer states may own WLED scratch allocations. Transfer
                    * ownership into the snapshot, clear the old bank, then move
                    * each retained state exactly once into its new zone index. */
                   memcpy(prior_fx_state,fx_state[ch],sizeof(prior_fx_state));
                   memset(fx_state[ch],0,sizeof(fx_state[ch]));
                   for(unsigned z=0;z<output->zone_count;z++){
                       const unsigned len=output->zones[z].pixels;
                       const ot_pz_zone_t *zone_cfg=&local_panel[ch].zones[z];
                       const uint8_t bank_z=resolved_fx_bank_index[ch][z];
                       const ot_fx_zone_slot_t *slot=bank_z<OT_OM_ZONES?
                           &local_fx_bank.zones[ch][bank_z]:NULL;
                       const bool active=len>0u&&zone_cfg->role!=OT_OM_OFF&&slot&&
                           ot_fx_slot_active(slot,frame_ms);
                       const int prior_index=active?
                           ot_effects_1d_find_prior_zone_state(zone_cfg->panel_id,
                               zone_cfg->id,slot->generation,old_panel_id,
                               old_zone_id,old_generation,old_active,OT_OM_ZONES):-1;
                       const bool same_identity=prior_index>=0;
                       if(active&&len<=OT_PZ_MAX_PIXELS){
                           if(same_identity){
                               ot_effects_1d_move_state(&fx_state[ch][z],
                                   &prior_fx_state[prior_index]);
                           }else{
                               ot_effects_1d_reset(&fx_state[ch][z],
                                   0x1234u+ch*97u+z*19u+slot->generation);
                           }
                           memset(fx_zone_scratch,0,(size_t)len*sizeof(fx_zone_scratch[0]));
                           for(unsigned px=0;px<render_pixels[ch];px++){
                               if(pixel_zone_map[ch][px]!=(uint8_t)z)continue;
                               const unsigned logical=pixel_logical_map[ch][px];
                               if(logical>=len)continue;
                               const uint8_t old_z=prior_pixel_zone_map[ch][px];
                               if(same_identity&&old_z<OT_OM_ZONES&&old_active[old_z]&&
                                  old_z==(uint8_t)prior_index&&
                                  old_zone_id[old_z]==zone_cfg->id&&
                                  old_panel_id[old_z]==zone_cfg->panel_id)
                                   fx_zone_scratch[logical]=fx_frame[ch][px];
                           }
                           const ot_ble_fx_packet_t *fx=&slot->request;
                           ot_wled_fx_config_t cfg={
                               .effect_id=fx->effect_id,
                               .speed=fx->speed,.intensity=fx->intensity,
                               .brightness=fx->brightness,
                               .palette_id=fx->palette_id,.frame_time_ms=(uint16_t)frame_delta_ms,
                               .foreground={fx->r,fx->g,fx->b},
                               .background={fx->bg_r,fx->bg_g,fx->bg_b},
                               .tertiary={slot->tertiary.r,slot->tertiary.g,slot->tertiary.b},
                               .tertiary_set=slot->tertiary.set,
                               .spark_style=slot->spark.style,
                               .spark_peak=slot->spark.peak,
                               .spark_color=slot->spark.color,
                               .resolved_palette=fx->palette_id==1u?
                                   &global_palette.current:
                                   (fx->palette_id>=73u&&fx->palette_id<=200u&&
                                    frame_palettes[ch*OT_OM_ZONES+z].present)?
                                   &frame_palettes[ch*OT_OM_ZONES+z].palette:NULL
                           };
                           const uint32_t zone_elapsed=frame_ms-slot->started_ms;
                           if(((fx->palette_id>=73u&&fx->palette_id<=200u)&&
                                 !frame_palettes[ch*OT_OM_ZONES+z].present)||
                              !ot_effects_1d_render(&cfg,&fx_state[ch][z],zone_elapsed,
                                                    fx_zone_scratch,len)){
                               memset(fx_zone_scratch,0,(size_t)len*sizeof(fx_zone_scratch[0]));
                               ot_st_set_fault("invalid BLE effect frame");
                               stop_continuous_on_fault();
                           }else if(slot->overlay.style&&
                                    !ot_fx_overlay_apply(&slot->overlay,
                                        slot->generation^((uint32_t)ch<<16)^((uint32_t)z<<8),
                                        zone_elapsed,fx_zone_scratch,len,cfg.brightness)){
                               ot_st_set_fault("invalid effect overlay");
                               stop_continuous_on_fault();
                           }
                           for(unsigned px=0;px<render_pixels[ch];px++){
                               if(pixel_zone_map[ch][px]!=(uint8_t)z)continue;
                               const unsigned logical=pixel_logical_map[ch][px];
                               if(logical<len)fx_frame[ch][px]=fx_zone_scratch[logical];
                           }
                           prior_fx_active[ch][z]=true;
                           prior_fx_generation[ch][z]=slot->generation;
                       }else{
                           prior_fx_active[ch][z]=false;
                       }
                       prior_fx_zone_id[ch][z]=zone_cfg->id;
                       prior_fx_panel_id[ch][z]=zone_cfg->panel_id;
                   }
                   for(unsigned z=output->zone_count;z<OT_OM_ZONES;z++)
                       prior_fx_active[ch][z]=false;
                   /* Deleted/inactive zones did not transfer their state.
                    * Release any source-owned scratch exactly once. */
                   for(unsigned old_z=0;old_z<OT_OM_ZONES;old_z++)
                       ot_effects_1d_reset(&prior_fx_state[old_z],0u);
               }else{
                   for(unsigned z=0;z<OT_OM_ZONES;z++)prior_fx_active[ch][z]=false;
               }
               /* Physical trigger overlays outrank diagnostics, which outrank
                * show/demo pixels. Gaps and UNASSIGNED zones use raw fixed-input
                * commissioning fallback; explicit OFF zones remain dark. */
               frame_captured[ch]=ot_frame_snapshot_begin((uint8_t)ch,
                                        render_pixels[ch],frame_index+1u);
               bool pixel_write_failed=false;
               for(unsigned px=0;px<render_pixels[ch];++px){
                   ot_rgb_t c={0,0,0};
                   const int zone=pixel_zone_map[ch][px]<output->zone_count?
                                  (int)pixel_zone_map[ch][px]:-1;
                   const uint8_t role=zone>=0?local_panel[ch].zones[zone].role:
                                               OT_PZ_ROLE_UNASSIGNED;
                   const uint8_t bank_z=zone>=0?
                       resolved_fx_bank_index[ch][(unsigned)zone]:0xffu;
                   const bool fx_here=zone>=0&&role!=OT_OM_OFF&&native_rendering&&
                       local_fx_enabled&&bank_z<OT_OM_ZONES&&
                       ot_fx_slot_active(&local_fx_bank.zones[ch][bank_z],frame_ms);
                   const bool legacy_here=zone>=0&&role!=OT_OM_OFF&&native_rendering&&
                       !local_fx_enabled&&(local_show.chain_mask&(1u<<ch))&&
                       (local_show.section_mask&(1u<<zone));
                   const bool demo_here=zone>=0&&role!=OT_OM_OFF&&!fx_here&&
                       !legacy_here&&local_demo.mode!=OT_DEMO_OFF;
                   const bool decorated=fx_here||legacy_here||demo_here;
                   bool trigger_here=false;
                   if(zone<0||role==OT_PZ_ROLE_UNASSIGNED){
                       c=ot_pz_fallback_color((unsigned)ch,fallback_mask,255u);
                       trigger_here=c.r!=0u||c.g!=0u||c.b!=0u;
                   }else if(role!=OT_OM_OFF){
                       trigger_here=ot_om_trigger_overlay(role,combined,decorated,
                                             255u,&c);
                   }
                   bool diagnostic_here=false;
                   if(!trigger_here){
                       bool diagnostic_owned=false;ot_rgb_t diagnostic={0,0,0};
                       if(ot_pz_diag_render(&local_diag,(unsigned)ch,(uint16_t)px,
                                            frame_ms,&diagnostic_owned,&diagnostic)&&
                          diagnostic_owned){
                           c=diagnostic;
                           diagnostic_here=true;
                       }
                   }
                   if(!trigger_here&&!diagnostic_here&&zone>=0&&role!=OT_OM_OFF){
                       const unsigned logical_px=zone_global_start[zone]+
                                                 pixel_logical_map[ch][px];
                       if(fx_here){
                           const ot_wled_rgb_t fx=fx_frame[ch][px];
                           const uint8_t bank_z=resolved_fx_bank_index[ch][(unsigned)zone];
                           const ot_fx_zone_slot_t *slot=&local_fx_bank.zones[ch][bank_z];
                           const uint16_t progress=ot_transition_progress(
                               (uint32_t)(frame_ms-slot->started_ms),
                               OT_FX_ENTRY_FADE_MS,OT_TRANSITION_SMOOTH);
                           const ot_transition_rgb_t black={0,0,0};
                           const ot_transition_rgb_t target={fx.r,fx.g,fx.b};
                           const ot_transition_rgb_t blended=ot_transition_blend(
                               black,target,progress);
                           c=(ot_rgb_t){blended.r,blended.g,blended.b};
                       }else if(legacy_here){
                           c=ot_show_pixel(&local_show,&zone_layout,ch,logical_px,elapsed);
                       }else if(demo_here){
                           c=ot_pixel_color(&zone_layout,ch,logical_px,0,&local_demo,frame_index);
                       }
                       if(decorated)c=ot_om_calibrate(c,output);
                   }
                   /* One final ratio-preserving ceiling covers solid/show,
                    * Startup/Shutdown, effects, trigger and diagnostics. */
                   c=ot_om_limit_brightness(c,(uint8_t)OT_MAX_BRIGHTNESS);
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
               if(ch==0)ot_sw_idf_fault_inject(3u); /* Bench-only pre-submit stall. */
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
               if(ch==0)ot_sw_idf_fault_inject(4u); /* Bench-only completion-wait stall. */
               esp_err_t e=led_strip_refresh_async_done(strip[ch]);
               if(e!=ESP_OK){
                   output_runtime_fault((unsigned)ch,"transfer completion",e);
                   output_failed[ch]=true;
                   frame_submitted[ch]=false; /* No complete-frame/Stop/OTA credit. */
                   if(frame_captured[ch])ot_frame_snapshot_complete(false);
                   continue;
               }
               if(frame_captured[ch])ot_frame_snapshot_complete(true);
               memcpy(prior_pixel_zone_map[ch],pixel_zone_map[ch],
                      OT_PZ_MAX_PIXELS*sizeof(prior_pixel_zone_map[ch][0]));
               any_output_completed=true; /* completed RMT, not queued transfer */
               /* A failed lane must not throttle the surviving vehicle lights. */
               if(render_pixels[ch]>longest_pixels)
                   longest_pixels=(uint16_t)render_pixels[ch];
               portENTER_CRITICAL(&state_lock);
               output_refreshes[ch]++;
               pz_render_revision[ch]=local_active_revision[ch];
               portEXIT_CRITICAL(&state_lock);
           }
           /* Acknowledged Stop and FPS must represent complete multi-output
            * refreshes, not only whichever lane happened to succeed. */
           const bool full_frame=ot_frame_all_submitted(frame_submitted,OT_CHAIN_COUNT);
           ot_sw_idf_render_cycle(full_frame,any_output_completed,
                                  output_runtime_fault_mask!=0u,config_recovery_fault);
           /* A created FreeRTOS task does not prove the image can render.
            * Confirm OTA only after every configured RMT lane completes its
            * first frame; startup faults must leave rollback available. */
           if(full_frame && ot_sw_idf_stable() &&
              om_write_mutex && ble_startup_ready && http_startup_ready &&
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
/* This task executes deep NVS recovery and SPI pixel setup synchronously.
 * The v0.1.31 ESP32-S3 physical bootloop was a main-task stack overflow;
 * never silently fall back to the IDF default startup stack. */
#if CONFIG_ESP_MAIN_TASK_STACK_SIZE < 8192
#error "OpenTail app_main requires at least 8192 bytes of main-task stack"
#endif
void app_main(void) {
    core_boot_id=esp_random();
    if(core_boot_id==0u)core_boot_id=1u;
    ESP_LOGI(TAG,"CORE boot identity: %08lx",(unsigned long)core_boot_id);
    ot_sw_idf_boot_record();
    ESP_LOGW(TAG,"CORE %s BENCH ONLY: one ESP watchdog, NO redundant lights.",OT_FW);
    ESP_ERROR_CHECK(nvs_flash_init());
    ot_palette_runtime_load();
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)om_revision[o]=om_active_revision[o]=1u;
    om_load();
    tm_load();
    if(!tm_policy_load()){
        config_recovery_fault=true;
        ESP_LOGE(TAG,"Trigger input policy NVS record invalid or mismatched");
    }
    om_journal_load();
    tm_journal_load();
    pz_load(); /* v4 split lane/map pair authoritative; v3 and v1/v2 are read-only migration inputs */
    if(!ot_layout_valid(&layout)){ESP_LOGE(TAG,"Invalid LED layout");abort();}
    ot_st_init();
    const bool watchdog_ready=ot_sw_idf_configure();
    if(!watchdog_ready)ot_st_set_fault("single-ESP watchdog unavailable");
    /* Start a bounded pending-OTA rollback guard. Confirmation is still
     * deferred until 60s continuous healthy dual-lane/input progress. */
    ot_ota_watch_boot();
    if(ot_sw_idf_boot_loop_seen())
        ot_st_set_fault("repeated watchdog/panic resets; decorative output inhibited");
    ESP_LOGI(TAG,"Startup main-task stack high-water after status SPI: %u bytes",
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
    if(config_recovery_fault)ot_st_set_fault("corrupt configuration recovery journal");
    const bool inputs_ready=watchdog_ready&&input_init();
    if(!inputs_ready)ot_st_set_fault("physical GPIO or watchdog initialization failed");
    /* Capture held inputs before RMT can emit its first frame; debounce
     * continues at 10ms independently once the sampler starts. */
    if(inputs_ready){
        /* Timestamp precedes the GPIO baseline: only ISR events newer than
         * this point are replayed by the turn debouncers. */
        input_boot_snapshot_us=(uint64_t)esp_timer_get_time();
        live_input_mask=tm_gpio_input_mask();
    }
    const bool input_watched=inputs_ready&&ot_sw_idf_watch_inputs();
    if(inputs_ready&&!input_watched)
        ot_st_set_fault("single-ESP input watchdog registration failed");
    const bool sampler_ready=input_watched &&
        xTaskCreate(input_sampler_task,"ot_inputs",3072,NULL,9,NULL)==pdPASS;
    if(input_watched&&!sampler_ready)ot_sw_idf_cancel_inputs();
    if(input_watched&&!sampler_ready)ot_st_set_fault("physical input sampler could not start");
    input_sampler_ready=sampler_ready; /* Set once before any BLE/HTTP service starts. */
    bool outputs_ready=sampler_ready&&!config_recovery_fault&&strip_init();
    /* Do not advertise renderer readiness until led_task initializes its timer. */
    const bool wifi_started=wifi_ap_init();
    if(!wifi_started)ot_st_set_fault("Wi-Fi diagnostic AP startup failed");
    om_write_mutex=xSemaphoreCreateMutex();
    /* Configuration writes already reject a missing mutex. Keep local
     * trigger-driven rendering alive rather than rebooting on this fault. */
    if(!om_write_mutex)ot_st_set_fault("Output Manager save mutex allocation failed");
    else{
        config_promotion_ready=xTaskCreate(config_activation_task,"ot_cfg_activate",
            5120,NULL,5,NULL)==pdPASS;
        if(!config_promotion_ready)
            ot_st_set_fault("durable configuration promoter failed to start");
    }
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
