/* OpenTail CORE BLE v2 bench enrollment and existing show/output commands.
 * A physical BOOT/GPIO0 press permits reading a device-specific enrollment key.
 * Routine reconnects authenticate by HMAC of a fresh challenge; the key remains
 * in its own NVS namespace and does not ride along with routine BLE commands.
 * First enrollment is NOT authenticated against an active MITM. Bench ONLY.
 * Existing WPA2 Wi-Fi authorization stays available unchanged.
 */
#include "sdkconfig.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "nvs.h"
#include "mbedtls/md.h"
#include "status_led.h"
#include "ot_version.h"
#include "ble_command.h"
#include "ble_fx_command.h"
#include "effects_1d.h"
#include "fx_preset_record.h"
#include "ble_config.h"
#include "panel_zone_api.h"
#include "panel_zone_protocol.h"
#include "input_telemetry.h"
#include "panel_zone_diag.h"
#include "trigger_manager.h"
#include "ot_frame_snapshot.h"
#include "palette_ble_v3.h"
#include "palette_profile.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
extern bool ot_ble_apply_show(const ot_ble_show_packet_t *request);
extern bool ot_ble_apply_fx(const ot_ble_fx_packet_t *request);
extern bool ot_ble_apply_continuous_fx(const ot_ble_fx_packet_t *request);
extern bool ot_ble_apply_continuous_fx_tertiary(const ot_ble_fx_packet_t *request,
                                                const ot_ble_fx_tertiary_t *tertiary);
extern void ot_ble_stop_show(void);
extern void ot_ble_disconnect_show(void);
extern bool ot_ble_save_fx_preset(uint8_t slot);
extern bool ot_ble_play_fx_preset(uint8_t slot);
extern bool ot_ble_delete_fx_preset(uint8_t slot);
extern bool ot_ble_config_get(uint8_t index,ot_om_output_t *out,uint16_t *revision);
extern int ot_ble_config_save(uint8_t index,const ot_om_output_t *out,uint16_t base_revision);
extern bool ot_ble_trigger_get(ot_tm_profile_t *out,uint16_t *revision,
                               uint8_t *raw,uint8_t *effective,bool *persistent);
extern int ot_ble_trigger_save(const ot_tm_profile_t *out,uint16_t base_revision);
static uint8_t token[OT_BLE_TOKEN_BYTES];
static bool token_ready=false;
/* Latched only after NimBLE accepts the first advertising request. */
static bool ble_radio_ever_ready=false;
bool ot_ble_radio_ever_ready(void){
    return __atomic_load_n(&ble_radio_ever_ready,__ATOMIC_ACQUIRE);
}
void ot_ble_session_hex(char out[33]){
    if(!out)return;
    out[0]='\0';
    if(!token_ready)return;
    for(unsigned i=0;i<OT_BLE_TOKEN_BYTES;i++)snprintf(out+i*2,3,"%02x",token[i]);
}
#if CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_att.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
static const char *TAG="OT_BLE";
static uint8_t addr_type;
static uint16_t active_conn=BLE_HS_CONN_HANDLE_NONE;
static bool authorized=false;
static bool panel_caps_seen=false;
static uint8_t panel_response[OT_CFG_PACKET_BYTES]={0};
static uint8_t panel_response_len=0;
static uint8_t panel_snapshot[OT_PZ_MAX_WIRE_BYTES];
static uint8_t panel_upload[OT_PZ_MAX_WIRE_BYTES];
static bool panel_received[OT_PZ_MAX_WIRE_BYTES];
static uint16_t panel_upload_crc=0,panel_upload_revision=0;
static uint8_t panel_read_index=255,panel_upload_index=255;
typedef enum {
    OT_PZ_TX_IDLE=0,OT_PZ_TX_ACTIVE=1,OT_PZ_TX_COMMITTING=2,
    OT_PZ_TX_COMMITTED=3,OT_PZ_TX_CANCELLED=4,OT_PZ_TX_EXPIRED=5,
    OT_PZ_TX_ERROR=6
} ot_pz_tx_status_t;
typedef struct {
    uint32_t session_id,last_sequence,last_packet_crc32;
    uint32_t expires_ms,base_crc32,candidate_crc32;
    uint32_t last_rejected_sequence,missing_sequence_count;
    uint32_t out_of_order_count,duplicate_count,conflict_count;
    uint32_t committing_reject_count;
    uint16_t base_revision,pixel_capacity;
    uint8_t lane,status,last_error,last_rejected_reason;
    bool active,has_sequence,committing;
    bool sequence_sync_warning,sequence_sync_fault;
} ot_pz_tx_session_t;
static ot_pz_tx_session_t panel_preview_tx;
static void panel_preview_reject_control(uint8_t reason);
static ot_ble_result_t last_result=OT_BLE_ERR_AUTH;
static uint8_t owner_key[16],owner_challenge[16];
static bool owner_valid=false,challenge_ready=false;
static bool boot_enrollment_ready=false;
static uint8_t pair_failures=0;
static bool owner_load(void){
    nvs_handle_t h;
    if(nvs_open("ot_blepair",NVS_READONLY,&h)!=ESP_OK)return false;
    size_t n=sizeof(owner_key);
    esp_err_t e=nvs_get_blob(h,"owner",owner_key,&n);
    nvs_close(h);
    return e==ESP_OK && n==sizeof(owner_key);
}
static bool owner_create(void){
    if(owner_valid)return true;
    uint8_t candidate[16];esp_fill_random(candidate,sizeof(candidate));
    nvs_handle_t h;
    if(nvs_open("ot_blepair",NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(h,"owner",candidate,sizeof(candidate));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    if(e!=ESP_OK)return false;
    memcpy(owner_key,candidate,sizeof(owner_key));owner_valid=true;
    return true;
}
/* GPIO0 BOOT must be pressed AFTER power-up, never while resetting. */
static int owner_enroll_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    if(conn!=active_conn||!boot_enrollment_ready||gpio_get_level(GPIO_NUM_0)!=0)return BLE_ATT_ERR_READ_NOT_PERMITTED;
    if(!owner_create())return BLE_ATT_ERR_UNLIKELY;
    return os_mbuf_append(ctx->om,owner_key,sizeof(owner_key))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static int owner_challenge_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    if(conn!=active_conn||!owner_valid||pair_failures>=5)return BLE_ATT_ERR_READ_NOT_PERMITTED;
    esp_fill_random(owner_challenge,sizeof(owner_challenge));challenge_ready=true;
    return os_mbuf_append(ctx->om,owner_challenge,sizeof(owner_challenge))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static bool owner_verify(const uint8_t mac[16]){
    if(!owner_valid||!challenge_ready||pair_failures>=5)return false;
    challenge_ready=false; /* never permit challenge replay */
    static const uint8_t context[]="OpenTail-BLE-v2";
    uint8_t message[sizeof(context)-1+16],expected[32];
    memcpy(message,context,sizeof(context)-1);
    memcpy(message+sizeof(context)-1,owner_challenge,16);
    const mbedtls_md_info_t *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(!md||mbedtls_md_hmac(md,owner_key,sizeof(owner_key),message,sizeof(message),expected)!=0)return false;
    uint8_t mismatch=0;
    for(int i=0;i<16;i++)mismatch|=(uint8_t)(expected[i]^mac[i]);
    if(mismatch)pair_failures++;
    return mismatch==0;
}

static const ble_uuid128_t ot_service_uuid=BLE_UUID128_INIT(0x21,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_info_uuid=BLE_UUID128_INIT(0x22,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_cmd_uuid=BLE_UUID128_INIT(0x23,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_cfg_cmd_uuid=BLE_UUID128_INIT(0x25,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_cfg_rsp_uuid=BLE_UUID128_INIT(0x26,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_enroll_uuid=BLE_UUID128_INIT(0x27,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_challenge_uuid=BLE_UUID128_INIT(0x28,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_fx_caps_uuid=BLE_UUID128_INIT(0x29,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_frame_cmd_uuid=BLE_UUID128_INIT(0x2a,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_frame_rsp_uuid=BLE_UUID128_INIT(0x2b,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_trigger_uuid=BLE_UUID128_INIT(0x2c,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_palette_cmd_uuid=BLE_UUID128_INIT(0x2d,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_palette_rsp_uuid=BLE_UUID128_INIT(0x2e,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_palette_caps_uuid=BLE_UUID128_INIT(0x2f,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_panel_caps_uuid=BLE_UUID128_INIT(0x30,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_panel_cfg_cmd_uuid=BLE_UUID128_INIT(0x31,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_panel_cfg_rsp_uuid=BLE_UUID128_INIT(0x32,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_pixel_probe_uuid=BLE_UUID128_INIT(0x33,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_input_diag_uuid=BLE_UUID128_INIT(0x34,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_panel_preview_control_uuid=BLE_UUID128_INIT(0x35,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_panel_preview_update_uuid=BLE_UUID128_INIT(0x36,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_panel_preview_status_uuid=BLE_UUID128_INIT(0x37,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static const ble_uuid128_t ot_reply_uuid=BLE_UUID128_INIT(0x24,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static int info_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)conn;(void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    const char *message="OpenTail CORE " OT_FW_VERSION " | BLE HMAC enrollment bench v2" " | continuous-fx-v1" " | " OT_BLE_FX_TERTIARY_CAPABILITY " | " OT_BLE_FX_TERTIARY13_CAPABILITY;
    return os_mbuf_append(ctx->om,message,strlen(message))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static int reply_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    const char *result=conn==active_conn?ot_ble_result_string(last_result):"ERR AUTH";
    return os_mbuf_append(ctx->om,result,strlen(result))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
/* Read-only BLE FX capability descriptor. No Wi-Fi, WLED runtime or authentication
 * is required to discover what this firmware actually implements. Version 1:
 * [version,count, little-endian effect IDs...] with offset-aware GATT reads.
 */
static int fx_capabilities_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR || conn!=active_conn)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    /* NimBLE's ble_gatts_val_access() applies Read Blob offsets AFTER this
     * callback returns the COMPLETE value. Do not slice by ctx->offset or ATT
     * MTU here: that double-applies the offset and returns GATT 7 on Android.
     * v1 format: up to 254 IDs, 510 bytes, within the 512-byte ATT limit. */
    uint16_t ids[254]={0};
    size_t total=ot_effects_1d_supported_ids(ids,254u);
    if(total>254u)total=254u;
    uint8_t payload[2u+254u*2u]={1,(uint8_t)total};
    for(size_t i=0;i<total;i++){
        payload[2u+i*2u]=(uint8_t)ids[i];
        payload[3u+i*2u]=(uint8_t)(ids[i]>>8);
    }
    const size_t payload_len=2u+total*2u;
    return os_mbuf_append(ctx->om,payload,payload_len)==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
/* Revoked owner sessions discard cached output, frame and trigger replies. */
static void cfg_reset(void);
static void panel_session_reset(void);
static void frame_reply_reset(void);
static uint8_t trigger_last_status=0;
static int command_write(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR || conn!=active_conn)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t buf[20];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,buf,sizeof(buf),&n)!=0){last_result=OT_BLE_ERR_WIRE;return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
    ot_ble_show_packet_t show={0};
    const bool was_authorized=authorized;
    if(n==18 && buf[0]==1 && buf[1]==4){
        authorized=owner_verify(buf+2);
        last_result=authorized?OT_BLE_OK_AUTH:OT_BLE_ERR_AUTH;
    } else if(n==3 && buf[0]==OT_BLE_WIRE_V1 &&
             buf[1]>=OT_BLE_FX_SAVE_PRESET && buf[1]<=OT_BLE_FX_DELETE_PRESET){
        if(!authorized)last_result=OT_BLE_ERR_AUTH;
        else if(buf[2]>=OT_FX_PRESET_SLOTS)last_result=OT_BLE_ERR_RANGE;
        else {
            bool ok=buf[1]==OT_BLE_FX_SAVE_PRESET?ot_ble_save_fx_preset(buf[2]):
                buf[1]==OT_BLE_FX_PLAY_PRESET?ot_ble_play_fx_preset(buf[2]):
                                                ot_ble_delete_fx_preset(buf[2]);
            last_result=ok?(buf[1]==OT_BLE_FX_SAVE_PRESET?OT_BLE_OK_SAVED:
                 buf[1]==OT_BLE_FX_PLAY_PRESET?OT_BLE_OK_LOADED:OT_BLE_OK_DELETED):
                OT_BLE_ERR_BUSY;
        }
    } else if(n==2 && buf[0]==OT_BLE_WIRE_V1 && buf[1]==OT_BLE_RELEASE_TEMPORARY){
        if(!authorized)last_result=OT_BLE_ERR_AUTH;
        else {ot_ble_disconnect_show();last_result=OT_BLE_OK_RELEASED;}
    } else if(n>=2 && buf[0]==1 && (buf[1]==OT_BLE_FX_OPCODE ||
                     buf[1]==OT_BLE_FX_CONTINUOUS_OPCODE ||
                     buf[1]==OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE)) {
        ot_ble_fx_packet_t fx={0};
        ot_ble_fx_tertiary_t tertiary={0};
        const bool extended=buf[1]==OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE;
        ot_ble_fx_result_t result=extended?
            ot_ble_fx_parse_tertiary(buf,n,authorized,&fx,&tertiary):
            ot_ble_fx_parse(buf,n,authorized,&fx);
        if(result==OT_BLE_FX_OK)
            last_result=(extended?ot_ble_apply_continuous_fx_tertiary(&fx,&tertiary):
                         buf[1]==OT_BLE_FX_CONTINUOUS_OPCODE?ot_ble_apply_continuous_fx(&fx):
                         ot_ble_apply_fx(&fx))?OT_BLE_OK_FX:OT_BLE_ERR_BUSY;
        else if(result==OT_BLE_FX_UNAUTHORIZED)last_result=OT_BLE_ERR_AUTH;
        else if(result==OT_BLE_FX_RANGE || result==OT_BLE_FX_UNSUPPORTED)last_result=OT_BLE_ERR_RANGE;
        else last_result=OT_BLE_ERR_WIRE;
    } else last_result=ot_ble_parse(buf,n,token,&authorized,&show);
    if(was_authorized && !authorized){
        /* Re-authentication failure revokes temporary previews, never GPIO
         * triggers, persistent scenes, or saved configuration profiles. */
        ot_ble_disconnect_show();
        cfg_reset();
        panel_session_reset();
        frame_reply_reset();
        ot_palette_ble_session_reset();
        trigger_last_status=0;
    }
    if(last_result==OT_BLE_OK_SHOW && !ot_ble_apply_show(&show))last_result=OT_BLE_ERR_BUSY;
    if(last_result==OT_BLE_OK_STOP)ot_ble_stop_show();
    /* Errors are returned by read-only reply characteristic; always acknowledge write. */
    return 0;
}
/* BLE Output Config v1: fixed 20-byte frames work even at minimum ATT MTU=23.
 * No configuration is applied until CRC, zone validation, trigger-idle check,
 * NVS persistence, and session revision checks ALL succeed.
 */
static uint8_t cfg_snapshot[OT_CFG_WIRE_BYTES],cfg_upload[OT_CFG_WIRE_BYTES];
static bool cfg_received[OT_CFG_WIRE_BYTES];
static uint16_t cfg_read_revision=0,cfg_upload_revision=0,cfg_upload_crc=0;
static uint8_t cfg_read_index=255,cfg_upload_index=255;
static uint8_t cfg_response[OT_CFG_PACKET_BYTES]={2,0xff,0,5};
static uint8_t cfg_response_len=4;
static void cfg_ack(uint8_t opcode,uint8_t index,uint8_t code){
    memset(cfg_response,0,sizeof(cfg_response));cfg_response[0]=2;
    cfg_response[1]=(uint8_t)(0x80u|opcode);cfg_response[2]=index;
    cfg_response[3]=code;cfg_response_len=4;
}
static void cfg_reset(void){
    cfg_read_index=255;cfg_upload_index=255;
    memset(cfg_received,0,sizeof(cfg_received));cfg_ack(0x7f,0,5);
}
static void panel_session_reset(void){
    panel_caps_seen=false;panel_response_len=0;
    panel_read_index=255;panel_upload_index=255;
    panel_upload_crc=0;panel_upload_revision=0;
    memset(&panel_preview_tx,0,sizeof(panel_preview_tx));
    panel_preview_tx.status=OT_PZ_TX_IDLE;
    memset(panel_response,0,sizeof(panel_response));
    memset(panel_snapshot,0,sizeof(panel_snapshot));
    memset(panel_upload,0,sizeof(panel_upload));
    memset(panel_received,0,sizeof(panel_received));
    ot_ble_diag_disconnect();
}
static void panel_ack(uint8_t opcode,uint8_t lane,uint8_t status){
    panel_response[0]=OT_PZ_WIRE_VERSION;
    panel_response[1]=(uint8_t)(0x80u|opcode);
    panel_response[2]=lane;panel_response[3]=status;
    panel_response_len=4u;
}
static int panel_caps_read(uint16_t conn,uint16_t handle,
                           struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    if(conn!=active_conn||!authorized)return BLE_ATT_ERR_READ_NOT_PERMITTED;
    /* [transport/schema,max panels,zones,ranges,active BLE features,
     *  max payload LE16,chunk bytes,preview lease seconds,effective brightness cap.
     *  Feature bits: bit 0 Pixel Probe, bit 1 live arrangement preview,
     *  bit 2 dedicated 0xFE session/sequence protocol. Fill remains unadvertised. */
    const uint8_t caps[11]={
        OT_PZ_WIRE_VERSION,OT_PZ_SCHEMA_VERSION,
        OT_PZ_MAX_PANELS_PER_OUTPUT,OT_PZ_MAX_ZONES_PER_OUTPUT,
        OT_PZ_MAX_RANGES_PER_ZONE,0x0fu,
        (uint8_t)OT_PZ_MAX_WIRE_BYTES,(uint8_t)(OT_PZ_MAX_WIRE_BYTES>>8),
        15u,(uint8_t)(OT_PZ_PREVIEW_LEASE_MS/1000u),(uint8_t)OT_MAX_BRIGHTNESS
    };
    if(os_mbuf_append(ctx->om,caps,sizeof(caps))!=0)
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    panel_caps_seen=true;
    return 0;
}
static int panel_cfg_read(uint16_t conn,uint16_t handle,
                          struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    if(conn!=active_conn||!authorized||!panel_caps_seen||panel_response_len==0u)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    return os_mbuf_append(ctx->om,panel_response,panel_response_len)==0?
           0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static uint8_t panel_save_status(int rc){
    switch(rc){
        case 0:return 0u;
        case 1:return 7u; /* invalid model */
        case 2:return 2u; /* busy / active trigger / promoter unavailable */
        case 3:return 3u; /* stale revision or legacy downgrade unsupported */
        default:return 4u; /* durable NVS write failure */
    }
}
static int panel_cfg_write(uint16_t conn,uint16_t handle,
                           struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||conn!=active_conn)
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t packet[OT_CFG_PACKET_BYTES];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,packet,sizeof(packet),&n)!=0||n<3u||
       packet[0]!=OT_PZ_WIRE_VERSION){
        panel_ack(0x7fu,0u,1u);return 0;
    }
    const uint8_t opcode=packet[1],lane=packet[2];
    panel_ack(opcode,lane,0u);
    if(!authorized||!panel_caps_seen){panel_ack(opcode,lane,5u);return 0;}
    /* Legacy geometry upload/save/reset paths share the same protected commit
     * boundary as 0xFE. Reads remain available; no competing write may proceed. */
    if(panel_preview_tx.committing&&
       (opcode==3u||opcode==4u||opcode==5u||opcode==6u||opcode==0xffu)){
        panel_preview_reject_control(OT_PZ_TX_ERROR_COMMITTING_LOCKED);
        panel_ack(opcode,lane,OT_PZ_TX_ERROR_COMMITTING_LOCKED);
        return 0;
    }
    /* Two-step destructive map reset. 0xFF/0xAA requests a revision-bound
     * challenge; 0xFF/0xAB confirms it. This is separate from Pixel Probe AA. */
    if(opcode==0xffu&&lane==0xaau){
        if(n!=6u){panel_ack(opcode,packet[3],1u);return 0;}
        const uint8_t reset_lane=packet[3];
        const uint16_t revision=(uint16_t)(packet[4]|((uint16_t)packet[5]<<8));
        uint32_t challenge=0u;
        const int rc=ot_ble_panel_reset_challenge(reset_lane,revision,&challenge);
        if(rc!=0){panel_ack(opcode,reset_lane,rc==2?2u:(rc==3?3u:1u));return 0;}
        panel_response[0]=OT_PZ_WIRE_VERSION;panel_response[1]=0xffu;
        panel_response[2]=0xaau;panel_response[3]=reset_lane;
        panel_response[4]=(uint8_t)challenge;panel_response[5]=(uint8_t)(challenge>>8);
        panel_response[6]=(uint8_t)(challenge>>16);panel_response[7]=(uint8_t)(challenge>>24);
        panel_response[8]=(uint8_t)revision;panel_response[9]=(uint8_t)(revision>>8);
        panel_response[10]=(uint8_t)OT_PZ_RESET_CHALLENGE_TTL_MS;
        panel_response[11]=(uint8_t)(OT_PZ_RESET_CHALLENGE_TTL_MS>>8);
        panel_response_len=12u;return 0;
    }
    if(opcode==0xffu&&lane==0xabu){
        if(n!=10u){panel_ack(opcode,packet[3],1u);return 0;}
        const uint8_t reset_lane=packet[3];
        const uint32_t challenge=(uint32_t)packet[4]|
            ((uint32_t)packet[5]<<8)|((uint32_t)packet[6]<<16)|
            ((uint32_t)packet[7]<<24);
        const uint16_t revision=(uint16_t)(packet[8]|((uint16_t)packet[9]<<8));
        const int rc=ot_ble_panel_reset_confirm(reset_lane,revision,challenge);
        panel_ack(opcode,reset_lane,rc==0?0u:(rc==5?2u:(rc==3?3u:(rc==4?4u:1u))));
        return 0;
    }
    if(lane>=OT_PZ_OUTPUTS){panel_ack(opcode,lane,1u);return 0;}
    switch(opcode){
        case 1u:{ /* immutable active snapshot */
            if(n!=3u){panel_ack(opcode,lane,1u);break;}
            ot_pz_output_t output={0};uint16_t active_rev=0,staged_rev=0;
            bool pending=false,persistent=false;
            if(!ot_ble_panel_get(lane,&output,&active_rev,&staged_rev,
                                 &pending,&persistent)||
               !ot_pz_wire_encode(&output,lane,panel_snapshot)){
                panel_ack(opcode,lane,7u);break;
            }
            panel_read_index=lane;
            const uint16_t crc=ot_cfg_crc16(panel_snapshot,OT_PZ_MAX_WIRE_BYTES);
            panel_response[0]=OT_PZ_WIRE_VERSION;panel_response[1]=0x81u;
            panel_response[2]=lane;panel_response[3]=0u;
            panel_response[4]=(uint8_t)OT_PZ_MAX_WIRE_BYTES;
            panel_response[5]=(uint8_t)(OT_PZ_MAX_WIRE_BYTES>>8);
            panel_response[6]=(uint8_t)crc;panel_response[7]=(uint8_t)(crc>>8);
            panel_response[8]=(uint8_t)active_rev;
            panel_response[9]=(uint8_t)(active_rev>>8);
            panel_response[10]=(uint8_t)staged_rev;
            panel_response[11]=(uint8_t)(staged_rev>>8);
            panel_response[12]=pending?1u:0u;
            panel_response[13]=persistent?1u:0u;
            panel_response_len=14u;break;
        }
        case 2u:{ /* read chunk: max 15 payload bytes */
            if(n!=5u||panel_read_index!=lane){panel_ack(opcode,lane,1u);break;}
            const uint16_t off=(uint16_t)(packet[3]|((uint16_t)packet[4]<<8));
            if(off>=OT_PZ_MAX_WIRE_BYTES){panel_ack(opcode,lane,1u);break;}
            unsigned count=OT_PZ_MAX_WIRE_BYTES-off;
            if(count>15u)count=15u;
            panel_response[0]=OT_PZ_WIRE_VERSION;panel_response[1]=0x82u;
            panel_response[2]=lane;panel_response[3]=packet[3];panel_response[4]=packet[4];
            memcpy(panel_response+5,panel_snapshot+off,count);
            panel_response_len=(uint8_t)(5u+count);break;
        }
        case 3u:{ /* begin durable upload: version/op/lane/length/CRC/base revision */
            if(n!=9u){panel_ack(opcode,lane,1u);break;}
            if(panel_preview_tx.active||panel_preview_tx.committing||
               panel_upload_index!=255u){
                panel_ack(opcode,lane,2u);break;
            }
            const uint16_t length=(uint16_t)(packet[3]|((uint16_t)packet[4]<<8));
            if(length!=OT_PZ_MAX_WIRE_BYTES){panel_ack(opcode,lane,1u);break;}
            panel_upload_crc=(uint16_t)(packet[5]|((uint16_t)packet[6]<<8));
            panel_upload_revision=(uint16_t)(packet[7]|((uint16_t)packet[8]<<8));
            panel_upload_index=lane;
            memset(panel_upload,0,sizeof(panel_upload));
            memset(panel_received,0,sizeof(panel_received));break;
        }
        case 4u:{ /* upload chunk, 1..15 bytes; duplicate bytes must match */
            if(n<6u||panel_upload_index!=lane){panel_ack(opcode,lane,1u);break;}
            const uint16_t off=(uint16_t)(packet[3]|((uint16_t)packet[4]<<8));
            const unsigned count=n-5u;
            if(count>15u||off>=OT_PZ_MAX_WIRE_BYTES||
               (uint32_t)off+count>OT_PZ_MAX_WIRE_BYTES){panel_ack(opcode,lane,1u);break;}
            bool conflict=false;
            for(unsigned i=0;i<count;i++){
                const unsigned pos=(unsigned)off+i;
                if(panel_received[pos]&&panel_upload[pos]!=packet[5u+i]){
                    conflict=true;break;
                }
            }
            if(conflict){panel_ack(opcode,lane,1u);break;}
            for(unsigned i=0;i<count;i++){
                const unsigned pos=(unsigned)off+i;
                panel_upload[pos]=packet[5u+i];panel_received[pos]=true;
            }
            break;
        }
        case 5u:{ /* validate complete immutable upload, then stage CORE config */
            if(n!=3u||panel_upload_index!=lane){panel_ack(opcode,lane,1u);break;}
            for(unsigned i=0;i<OT_PZ_MAX_WIRE_BYTES;i++){
                if(!panel_received[i]){panel_ack(opcode,lane,8u);panel_upload_index=255;return 0;}
            }
            if(ot_cfg_crc16(panel_upload,OT_PZ_MAX_WIRE_BYTES)!=panel_upload_crc){
                panel_ack(opcode,lane,6u);panel_upload_index=255;break;
            }
            ot_pz_output_t output={0};
            if(!ot_pz_wire_decode(panel_upload,lane,&output)){
                panel_ack(opcode,lane,7u);panel_upload_index=255;break;
            }
            panel_ack(opcode,lane,panel_save_status(
                ot_ble_panel_save(lane,&output,panel_upload_revision)));
            panel_upload_index=255;break;
        }
        case 6u: /* cancel */
            if(n!=3u){panel_ack(opcode,lane,1u);break;}
            panel_upload_index=255;memset(panel_received,0,sizeof(panel_received));break;
        case 0xabu: /* proposed Fill opcode intentionally not accepted */
            panel_ack(opcode,lane,9u);break;
        default:panel_ack(opcode,lane,1u);break;
    }
    return 0;
}

static void panel_preview_set_error(uint8_t status){
    panel_preview_tx.last_error=status;
    panel_preview_tx.status=OT_PZ_TX_ERROR;
}
static void panel_preview_sat_add(uint32_t *value,uint32_t amount){
    if(!value)return;
    *value=UINT32_MAX-*value<amount?UINT32_MAX:*value+amount;
}
static void panel_preview_reject_control(uint8_t reason){
    panel_preview_tx.last_error=reason;
    panel_preview_tx.last_rejected_reason=reason;
    panel_preview_tx.sequence_sync_warning=true;
    if(reason==OT_PZ_TX_ERROR_COMMITTING_LOCKED)
        panel_preview_sat_add(&panel_preview_tx.committing_reject_count,1u);
}
static void panel_preview_reject_update(uint32_t sequence,uint8_t reason,
                                        uint32_t missing_count){
    panel_preview_tx.last_rejected_sequence=sequence;
    panel_preview_tx.last_rejected_reason=reason;
    panel_preview_tx.last_error=reason;
    switch(reason){
        case OT_PZ_TX_ERROR_COMMITTING_LOCKED:
            panel_preview_tx.sequence_sync_warning=true;
            panel_preview_sat_add(&panel_preview_tx.committing_reject_count,1u);
            break;
        case OT_PZ_TX_ERROR_SEQUENCE_GAP:
            panel_preview_tx.sequence_sync_warning=true;
            panel_preview_tx.sequence_sync_fault=true;
            panel_preview_sat_add(&panel_preview_tx.missing_sequence_count,missing_count);
            break;
        case OT_PZ_TX_ERROR_SEQUENCE_OUT_OF_ORDER:
            panel_preview_tx.sequence_sync_warning=true;
            panel_preview_tx.sequence_sync_fault=true;
            panel_preview_sat_add(&panel_preview_tx.out_of_order_count,1u);
            break;
        case OT_PZ_TX_ERROR_SEQUENCE_DUPLICATE:
            panel_preview_tx.sequence_sync_warning=true;
            panel_preview_sat_add(&panel_preview_tx.duplicate_count,1u);
            break;
        case OT_PZ_TX_ERROR_SEQUENCE_CONFLICT:
            panel_preview_tx.sequence_sync_warning=true;
            panel_preview_tx.sequence_sync_fault=true;
            panel_preview_sat_add(&panel_preview_tx.conflict_count,1u);
            break;
        case OT_PZ_TX_ERROR_SYNC_REQUIRED:
            panel_preview_tx.sequence_sync_warning=true;
            panel_preview_tx.sequence_sync_fault=true;
            break;
        default:
            break;
    }
}
static bool panel_preview_session_matches(uint8_t lane,uint32_t session_id){
    return panel_preview_tx.active&&panel_preview_tx.lane==lane&&
        panel_preview_tx.session_id==session_id;
}
static void panel_preview_expire_if_needed(uint32_t now_ms){
    if(panel_preview_tx.active&&!panel_preview_tx.committing&&
       (int32_t)(now_ms-panel_preview_tx.expires_ms)>=0){
        ot_ble_panel_preview_cancel();
        panel_preview_tx.active=false;
        panel_preview_tx.status=OT_PZ_TX_EXPIRED;
        panel_preview_tx.last_error=0u;
    }
}
static void panel_preview_refresh_commit_state(void){
    if(!panel_preview_tx.active||!panel_preview_tx.committing)return;
    ot_pz_output_t output={0};
    uint16_t active_revision=0u,staged_revision=0u;
    bool pending=false,persistent=false;
    if(!ot_ble_panel_get(panel_preview_tx.lane,&output,&active_revision,
                         &staged_revision,&pending,&persistent)){
        /* A transient read failure cannot demote or release a durable commit. */
        panel_preview_tx.last_error=OT_PZ_TX_ERROR_INTERNAL;
        return;
    }
    (void)persistent;
    if(panel_preview_tx.candidate_crc32==panel_preview_tx.base_crc32){
        panel_preview_tx.committing=false;panel_preview_tx.active=false;
        panel_preview_tx.status=OT_PZ_TX_COMMITTED;return;
    }
    const uint16_t target=(uint16_t)(panel_preview_tx.base_revision+1u);
    if(pending&&staged_revision==target){
        panel_preview_tx.status=OT_PZ_TX_COMMITTING;return;
    }
    if(!pending&&active_revision==target&&
       ot_ble_panel_render_revision(panel_preview_tx.lane)==target){
        panel_preview_tx.committing=false;panel_preview_tx.active=false;
        panel_preview_tx.status=OT_PZ_TX_COMMITTED;return;
    }
    if(!pending&&active_revision==panel_preview_tx.base_revision&&
       staged_revision==panel_preview_tx.base_revision){
        if(ot_ble_panel_preview_commit_pending(panel_preview_tx.lane,
                                               panel_preview_tx.base_revision)){
            panel_preview_tx.status=OT_PZ_TX_COMMITTING;return;
        }
        panel_preview_tx.status=OT_PZ_TX_ERROR;
        panel_preview_tx.last_error=4u;
        panel_preview_tx.committing=false;
        /* Keep the live shadow candidate available for an explicit retry. */
    }
}
static int panel_preview_control_write(uint16_t conn,uint16_t handle,
        struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||conn!=active_conn||
       !authorized||!panel_caps_seen)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t packet[12]={0};uint16_t n=0;
    ot_pz_tx_control_packet_t command={0};
    if(ble_hs_mbuf_to_flat(ctx->om,packet,sizeof(packet),&n)!=0||
       !ot_pz_tx_parse_control(packet,n,&command)){
        if(panel_preview_tx.committing)
            panel_preview_reject_control(OT_PZ_TX_ERROR_COMMITTING_LOCKED);
        else panel_preview_set_error(OT_PZ_TX_ERROR_INVALID);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    panel_preview_expire_if_needed(now_ms);
    if(command.opcode==OT_PZ_TX_OP_BEGIN){
        if(panel_preview_tx.committing){
            panel_preview_reject_control(OT_PZ_TX_ERROR_COMMITTING_LOCKED);
            return 0;
        }
        if(command.schema_version!=OT_PZ_SCHEMA_VERSION)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if(panel_preview_tx.active||panel_upload_index!=255u){
            panel_preview_set_error(OT_PZ_TX_ERROR_BUSY);return 0;
        }
        const uint8_t lane=command.lane;
        const uint16_t revision=command.base_revision;
        const uint16_t capacity=command.pixel_capacity;
        ot_pz_output_t output={0};uint16_t active=0u,staged=0u;
        bool pending=false,persistent=false;
        if(!ot_ble_panel_get(lane,&output,&active,&staged,&pending,&persistent)||
           pending||revision!=active||revision!=staged||
           capacity!=output.active_pixels||revision==UINT16_MAX){
            panel_preview_set_error(OT_PZ_TX_ERROR_STALE);return 0;
        }
        uint32_t session=esp_random();
        if(session==0u)session=esp_random();
        if(session==0u){panel_preview_set_error(OT_PZ_TX_ERROR_INTERNAL);return 0;}
        const int rc=ot_ble_panel_preview_begin(lane,revision,capacity);
        if(rc!=0){panel_preview_set_error(rc==2?OT_PZ_TX_ERROR_BUSY:
            (rc==3?OT_PZ_TX_ERROR_STALE:OT_PZ_TX_ERROR_INVALID));return 0;}
        memset(&panel_preview_tx,0,sizeof(panel_preview_tx));
        panel_preview_tx.session_id=session;
        panel_preview_tx.base_revision=revision;
        panel_preview_tx.pixel_capacity=capacity;
        panel_preview_tx.lane=lane;
        panel_preview_tx.expires_ms=now_ms+OT_PZ_PREVIEW_LEASE_MS;
        panel_preview_tx.active=true;
        panel_preview_tx.status=OT_PZ_TX_ACTIVE;
        if(!ot_ble_panel_preview_crc32(lane,revision,&panel_preview_tx.base_crc32)){
            ot_ble_panel_preview_cancel();
            panel_preview_tx.active=false;
            panel_preview_set_error(OT_PZ_TX_ERROR_INTERNAL);return 0;
        }
        panel_preview_tx.candidate_crc32=panel_preview_tx.base_crc32;
        return 0;
    }

    const uint8_t lane=command.lane;
    const uint32_t session=command.session_id;
    if(panel_preview_tx.committing){
        /* Only an exact duplicate COMMIT is idempotent. UPDATE, CANCEL,
         * HEARTBEAT, BEGIN, wrong-session and conflicting COMMIT packets are
         * rejected without changing COMMITTING or touching the durable journal. */
        if(command.opcode==OT_PZ_TX_OP_COMMIT&&
           lane==panel_preview_tx.lane&&session==panel_preview_tx.session_id&&
           command.candidate_crc32==panel_preview_tx.candidate_crc32){
            panel_preview_refresh_commit_state();return 0;
        }
        panel_preview_reject_control(OT_PZ_TX_ERROR_COMMITTING_LOCKED);
        return 0;
    }
    /* A delayed exact COMMIT after terminal success is read-only/idempotent. */
    if(!panel_preview_tx.active&&panel_preview_tx.status==OT_PZ_TX_COMMITTED&&
       lane==panel_preview_tx.lane&&session==panel_preview_tx.session_id){
        if(command.opcode==OT_PZ_TX_OP_COMMIT&&
           command.candidate_crc32==panel_preview_tx.candidate_crc32)return 0;
        panel_preview_reject_control(OT_PZ_TX_ERROR_STALE);
        return 0;
    }
    if(!panel_preview_session_matches(lane,session)){
        panel_preview_set_error(OT_PZ_TX_ERROR_STALE);return 0;
    }
    if(command.opcode==OT_PZ_TX_OP_COMMIT){
        if(panel_preview_tx.sequence_sync_fault){
            panel_preview_reject_control(OT_PZ_TX_ERROR_SYNC_REQUIRED);return 0;
        }
        const uint32_t expected=command.candidate_crc32;
        const int rc=ot_ble_panel_preview_commit(lane,
                    panel_preview_tx.base_revision,expected);
        if(rc!=0){panel_preview_set_error(rc==2?OT_PZ_TX_ERROR_BUSY:
            (rc==3?OT_PZ_TX_ERROR_STALE:(rc==4?OT_PZ_TX_ERROR_INTERNAL:
                                          OT_PZ_TX_ERROR_INVALID)));return 0;}
        panel_preview_tx.candidate_crc32=expected;
        panel_preview_tx.committing=true;
        panel_preview_tx.status=OT_PZ_TX_COMMITTING;
        panel_preview_refresh_commit_state();
        return 0;
    }
    if(command.opcode==OT_PZ_TX_OP_CANCEL){
        ot_ble_panel_preview_cancel();
        panel_preview_tx.active=false;
        panel_preview_tx.status=OT_PZ_TX_CANCELLED;
        panel_preview_tx.last_error=OT_PZ_TX_ERROR_NONE;
        return 0;
    }
    const int rc=ot_ble_panel_preview_heartbeat(lane,panel_preview_tx.base_revision);
    if(rc!=0){panel_preview_set_error(rc==2?OT_PZ_TX_ERROR_BUSY:
                                             OT_PZ_TX_ERROR_STALE);return 0;}
    panel_preview_tx.expires_ms=now_ms+OT_PZ_PREVIEW_LEASE_MS;
    panel_preview_tx.status=OT_PZ_TX_ACTIVE;
    panel_preview_tx.last_error=OT_PZ_TX_ERROR_NONE;
    return 0;
}
static int panel_preview_update_write(uint16_t conn,uint16_t handle,
        struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||conn!=active_conn||
       !authorized||!panel_caps_seen)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t packet[43]={0};uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,packet,sizeof(packet),&n)!=0){
        if(panel_preview_tx.committing)
            panel_preview_reject_control(OT_PZ_TX_ERROR_COMMITTING_LOCKED);
        else panel_preview_set_error(OT_PZ_TX_ERROR_INVALID);
        return 0;
    }
    ot_pz_tx_update_packet_t update={0};
    const uint16_t mtu=ble_att_mtu(conn);
    if(!ot_pz_tx_parse_update(packet,n,mtu,&update)){
        if(panel_preview_tx.committing)
            panel_preview_reject_control(OT_PZ_TX_ERROR_COMMITTING_LOCKED);
        else panel_preview_set_error(OT_PZ_TX_ERROR_INVALID);
        return 0;
    }
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    panel_preview_expire_if_needed(now_ms);

    /* This guard intentionally precedes session/sequence checks. Any delayed
     * Write Without Response packet arriving after COMMIT is rejected while
     * the transaction remains COMMITTING; no shadow or NVS mutation occurs. */
    if(panel_preview_tx.committing){
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_COMMITTING_LOCKED,0u);
        return 0;
    }
    if(!panel_preview_session_matches(update.lane,update.session_id)){
        panel_preview_set_error(OT_PZ_TX_ERROR_STALE);return 0;
    }

    const ot_pz_tx_update_admission_t admission=ot_pz_tx_update_admit(
        panel_preview_tx.committing,panel_preview_tx.sequence_sync_fault,
        panel_preview_tx.has_sequence,panel_preview_tx.last_sequence,
        panel_preview_tx.last_packet_crc32,update.sequence,update.packet_crc32);
    if(admission==OT_PZ_TX_UPDATE_REJECT_DUPLICATE){
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_SEQUENCE_DUPLICATE,0u);
        return 0; /* Exact replay is observed, never applied twice. */
    }
    if(admission==OT_PZ_TX_UPDATE_REJECT_GAP){
        const uint32_t distance=!panel_preview_tx.has_sequence?update.sequence:
            (update.sequence>panel_preview_tx.last_sequence?
             update.sequence-panel_preview_tx.last_sequence:
             (UINT32_MAX-panel_preview_tx.last_sequence)+update.sequence);
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_SEQUENCE_GAP,distance>1u?distance-1u:0u);
        return 0;
    }
    if(admission==OT_PZ_TX_UPDATE_REJECT_OUT_OF_ORDER){
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_SEQUENCE_OUT_OF_ORDER,0u);
        return 0;
    }
    if(admission==OT_PZ_TX_UPDATE_REJECT_CONFLICT){
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_SEQUENCE_CONFLICT,0u);
        return 0;
    }
    if(admission==OT_PZ_TX_UPDATE_REJECT_SYNC_REQUIRED){
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_SYNC_REQUIRED,0u);
        return 0;
    }
    if(admission!=OT_PZ_TX_UPDATE_ACCEPT){
        panel_preview_set_error(OT_PZ_TX_ERROR_STALE);return 0;
    }

    const int rc=ot_ble_panel_preview_update_ranges(update.lane,
        panel_preview_tx.base_revision,update.descriptors,update.descriptor_count);
    if(rc!=0){
        const uint8_t error=rc==2?OT_PZ_TX_ERROR_BUSY:
            (rc==3?OT_PZ_TX_ERROR_STALE:OT_PZ_TX_ERROR_INVALID);
        panel_preview_reject_update(update.sequence,error,0u);
        if(rc==3)panel_preview_tx.sequence_sync_fault=true;
        return 0;
    }
    uint32_t candidate_crc=0u;
    if(!ot_ble_panel_preview_crc32(update.lane,panel_preview_tx.base_revision,
                                   &candidate_crc)){
        /* The shadow may already contain this edit. Freeze the session so the
         * client must reconcile/cancel rather than guess whether it was applied. */
        panel_preview_reject_update(update.sequence,
            OT_PZ_TX_ERROR_INTERNAL,0u);
        panel_preview_tx.sequence_sync_fault=true;
        return 0;
    }
    panel_preview_tx.last_sequence=update.sequence;
    panel_preview_tx.last_packet_crc32=update.packet_crc32;
    panel_preview_tx.has_sequence=true;
    panel_preview_tx.candidate_crc32=candidate_crc;
    panel_preview_tx.status=OT_PZ_TX_ACTIVE;
    panel_preview_tx.last_error=OT_PZ_TX_ERROR_NONE;
    /* UPDATE deliberately does not renew the 5-second lease. HEARTBEAT owns it. */
    return 0;
}
static int panel_preview_status_read(uint16_t conn,uint16_t handle,
        struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR||conn!=active_conn||
       !authorized||!panel_caps_seen)return BLE_ATT_ERR_READ_NOT_PERMITTED;
    panel_preview_expire_if_needed((uint32_t)(esp_timer_get_time()/1000LL));
    panel_preview_refresh_commit_state();
    uint8_t payload[OT_PZ_TX_STATUS_V2_BYTES]={0};
    payload[0]=OT_PZ_TX_STATUS_V2_VERSION;payload[1]=panel_preview_tx.status;
    payload[2]=panel_preview_tx.lane;payload[3]=OT_PZ_SCHEMA_VERSION;
    payload[4]=(uint8_t)panel_preview_tx.session_id;
    payload[5]=(uint8_t)(panel_preview_tx.session_id>>8);
    payload[6]=(uint8_t)(panel_preview_tx.session_id>>16);
    payload[7]=(uint8_t)(panel_preview_tx.session_id>>24);
    payload[8]=(uint8_t)panel_preview_tx.base_revision;
    payload[9]=(uint8_t)(panel_preview_tx.base_revision>>8);
    payload[10]=(uint8_t)panel_preview_tx.pixel_capacity;
    payload[11]=(uint8_t)(panel_preview_tx.pixel_capacity>>8);
    payload[12]=(uint8_t)OT_PZ_PREVIEW_LEASE_MS;
    payload[13]=(uint8_t)(OT_PZ_PREVIEW_LEASE_MS>>8);
    payload[14]=(uint8_t)panel_preview_tx.last_sequence;
    payload[15]=(uint8_t)(panel_preview_tx.last_sequence>>8);
    payload[16]=(uint8_t)(panel_preview_tx.last_sequence>>16);
    payload[17]=(uint8_t)(panel_preview_tx.last_sequence>>24);
    payload[18]=(uint8_t)panel_preview_tx.candidate_crc32;
    payload[19]=(uint8_t)(panel_preview_tx.candidate_crc32>>8);
    payload[20]=(uint8_t)(panel_preview_tx.candidate_crc32>>16);
    payload[21]=(uint8_t)(panel_preview_tx.candidate_crc32>>24);
    uint16_t active=0u,staged=0u;
    bool pending=false,persistent=false;
    ot_pz_output_t output={0};
    if(panel_preview_tx.lane<OT_PZ_OUTPUTS)
        (void)ot_ble_panel_get(panel_preview_tx.lane,&output,&active,&staged,
                               &pending,&persistent);
    const uint16_t rendered=panel_preview_tx.lane<OT_PZ_OUTPUTS?
        ot_ble_panel_render_revision(panel_preview_tx.lane):0u;
    payload[22]=(uint8_t)active;payload[23]=(uint8_t)(active>>8);
    payload[24]=(uint8_t)staged;payload[25]=(uint8_t)(staged>>8);
    payload[26]=(uint8_t)rendered;payload[27]=(uint8_t)(rendered>>8);
    payload[28]=pending?1u:0u;payload[29]=persistent?1u:0u;
    payload[30]=panel_preview_tx.last_error;
    const uint32_t now_ms=(uint32_t)(esp_timer_get_time()/1000LL);
    const uint32_t remaining=panel_preview_tx.active&&!panel_preview_tx.committing&&
        (int32_t)(panel_preview_tx.expires_ms-now_ms)>0?
        panel_preview_tx.expires_ms-now_ms:0u;
    payload[31]=(uint8_t)remaining;payload[32]=(uint8_t)(remaining>>8);
    payload[33]=(uint8_t)(remaining>>16);payload[34]=(uint8_t)(remaining>>24);
    const uint32_t rejected=panel_preview_tx.last_rejected_sequence;
    payload[35]=(uint8_t)rejected;payload[36]=(uint8_t)(rejected>>8);
    payload[37]=(uint8_t)(rejected>>16);payload[38]=(uint8_t)(rejected>>24);
    payload[39]=panel_preview_tx.last_rejected_reason;
    const uint32_t counters[5]={
        panel_preview_tx.missing_sequence_count,
        panel_preview_tx.out_of_order_count,
        panel_preview_tx.duplicate_count,
        panel_preview_tx.conflict_count,
        panel_preview_tx.committing_reject_count
    };
    for(unsigned i=0;i<5u;i++){
        const unsigned offset=40u+i*4u;
        payload[offset]=(uint8_t)counters[i];
        payload[offset+1u]=(uint8_t)(counters[i]>>8);
        payload[offset+2u]=(uint8_t)(counters[i]>>16);
        payload[offset+3u]=(uint8_t)(counters[i]>>24);
    }
    payload[60]=(uint8_t)((panel_preview_tx.sequence_sync_warning?1u:0u)|
                          (panel_preview_tx.sequence_sync_fault?2u:0u)|
                          (panel_preview_tx.has_sequence?4u:0u));
    return os_mbuf_append(ctx->om,payload,sizeof(payload))==0?
           0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static int pixel_probe_write(uint16_t conn,uint16_t handle,
                             struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||conn!=active_conn||
       !authorized||!panel_caps_seen)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t packet[OT_PZ_DIAG_PACKET_BYTES];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,packet,sizeof(packet),&n)!=0||
       n!=OT_PZ_DIAG_PACKET_BYTES)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    return ot_ble_pixel_probe(packet,n,true,panel_caps_seen)?
           0:BLE_ATT_ERR_UNLIKELY;
}
static int cfg_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    /* A failed/revoked HMAC authorization must not expose a cached response
     * from an earlier authorized command on the same connection. */
    if(conn!=active_conn || !authorized)return BLE_ATT_ERR_READ_NOT_PERMITTED;
    return os_mbuf_append(ctx->om,cfg_response,cfg_response_len)==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static int cfg_write(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||conn!=active_conn)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t packet[OT_CFG_PACKET_BYTES];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,packet,sizeof(packet),&n)!=0||n<3||packet[0]!=2){
        cfg_ack(0x7f,0,1);return 0;
    }
    const uint8_t opcode=packet[1],index=packet[2];
    cfg_ack(opcode,index,0);
    if(!authorized){cfg_ack(opcode,index,5);return 0;}
    if(index>=OT_OM_OUTPUTS){cfg_ack(opcode,index,1);return 0;}
    switch(opcode){
        case 1: { /* snapshot */
            if(n!=3){cfg_ack(opcode,index,1);break;}
            ot_om_output_t output;
            if(!ot_ble_config_get(index,&output,&cfg_read_revision)){cfg_ack(opcode,index,4);break;}
            ot_cfg_encode(&output,cfg_snapshot);cfg_read_index=index;
            const uint16_t crc=ot_cfg_crc16(cfg_snapshot,OT_CFG_WIRE_BYTES);
            cfg_response[4]=(uint8_t)OT_CFG_WIRE_BYTES;
            cfg_response[5]=(uint8_t)(OT_CFG_WIRE_BYTES>>8);
            cfg_response[6]=(uint8_t)crc;cfg_response[7]=(uint8_t)(crc>>8);
            cfg_response[8]=(uint8_t)cfg_read_revision;
            cfg_response[9]=(uint8_t)(cfg_read_revision>>8);
            cfg_response_len=10;break;
        }
        case 2: { /* chunk read */
            if(n!=5||cfg_read_index!=index){cfg_ack(opcode,index,1);break;}
            const uint16_t off=(uint16_t)(packet[3]|((unsigned)packet[4]<<8));
            if(off>=OT_CFG_WIRE_BYTES){cfg_ack(opcode,index,1);break;}
            unsigned count=OT_CFG_WIRE_BYTES-off;
            if(count>14u)count=14u;
            cfg_response[4]=packet[3];cfg_response[5]=packet[4];
            memcpy(cfg_response+6,cfg_snapshot+off,count);cfg_response_len=(uint8_t)(6+count);
            break;
        }
        case 3: { /* begin upload */
            if(n!=9){cfg_ack(opcode,index,1);break;}
            const uint16_t len=(uint16_t)(packet[3]|((unsigned)packet[4]<<8));
            if(len!=OT_CFG_WIRE_BYTES){cfg_ack(opcode,index,1);break;}
            cfg_upload_crc=(uint16_t)(packet[5]|((unsigned)packet[6]<<8));
            cfg_upload_revision=(uint16_t)(packet[7]|((unsigned)packet[8]<<8));
            cfg_upload_index=index;memset(cfg_upload,0,sizeof(cfg_upload));
            memset(cfg_received,0,sizeof(cfg_received));break;
        }
        case 4: { /* upload 1-15 bytes */
            if(n<6||cfg_upload_index!=index){cfg_ack(opcode,index,1);break;}
            const uint16_t off=(uint16_t)(packet[3]|((unsigned)packet[4]<<8));
            const unsigned count=n-5;
            if(count>OT_CFG_CHUNK_BYTES||off>=OT_CFG_WIRE_BYTES||off+count>OT_CFG_WIRE_BYTES){cfg_ack(opcode,index,1);break;}
            memcpy(cfg_upload+off,packet+5,count);
            for(unsigned i=off;i<off+count;i++)cfg_received[i]=true;
            break;
        }
        case 5: { /* commit */
            if(n!=3||cfg_upload_index!=index){cfg_ack(opcode,index,1);break;}
            for(unsigned i=0;i<OT_CFG_WIRE_BYTES;i++){
                if(!cfg_received[i]){cfg_ack(opcode,index,1);cfg_upload_index=255;return 0;}
            }
            if(ot_cfg_crc16(cfg_upload,OT_CFG_WIRE_BYTES)!=cfg_upload_crc){
                cfg_ack(opcode,index,6);cfg_upload_index=255;break;
            }
            ot_om_output_t output;
            if(!ot_cfg_decode(cfg_upload,&output)){cfg_ack(opcode,index,1);cfg_upload_index=255;break;}
            const int rc=ot_ble_config_save(index,&output,cfg_upload_revision);
            cfg_ack(opcode,index,(uint8_t)rc);cfg_upload_index=255;
            break;
        }
        case 6: { /* cancel */
            if(n!=3){cfg_ack(opcode,index,1);break;}
            cfg_upload_index=255;break;
        }
        default:cfg_ack(opcode,index,1);break;
    }
    return 0;
}
/* BLE post-render snapshot v1: authenticated, immutable, demand-driven.
 * WRITE: [1,1,output] captures next completed RMT frame for that output.
 *        [1,2,output,seq:uint32LE,first:uint16LE,count:uint8] selects slice.
 * READ: status [1,1,state,output,count:u16,seq:u32,crc16:u16]
 *       slice  [1,2,1,output,seq:u32,first:u16,count:u8,RGB888...]
 * State 0 pending, 1 ready, 2 unavailable/error. Snapshot is NOT optical
 * playback or a streaming FPS guarantee. Count <= 56 for MTU/ATT budget.
 */
static uint8_t frame_reply_kind=0,frame_reply_output=0,frame_reply_count=0;
static uint16_t frame_reply_first=0;
static uint32_t frame_reply_sequence=0;
static void frame_reply_reset(void){
    frame_reply_kind=0;frame_reply_output=0;
    frame_reply_first=0;frame_reply_count=0;frame_reply_sequence=0;
    ot_frame_snapshot_reset();
}
static int frame_write(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR || conn!=active_conn || !authorized)
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t p[10];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,p,sizeof(p),&n)!=0 || n<3 || p[0]!=OT_FRAME_SNAPSHOT_VERSION)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if(p[1]==1 && n==3){
        if(!ot_frame_snapshot_request(p[2]))return BLE_ATT_ERR_UNLIKELY;
        frame_reply_kind=1;frame_reply_output=p[2];
        return 0;
    }
    if(p[1]==2 && n==10 && p[9]>=1 && p[9]<=OT_FRAME_SNAPSHOT_CHUNK_PIXELS){
        frame_reply_kind=2;frame_reply_output=p[2];
        frame_reply_sequence=(uint32_t)p[3]|((uint32_t)p[4]<<8)|
                             ((uint32_t)p[5]<<16)|((uint32_t)p[6]<<24);
        frame_reply_first=(uint16_t)p[7]|((uint16_t)p[8]<<8);
        frame_reply_count=p[9];
        return 0;
    }
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
}
static int frame_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR || conn!=active_conn || !authorized)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    uint8_t p[11u+3u*OT_FRAME_SNAPSHOT_CHUNK_PIXELS]={0};
    size_t size=0;
    ot_frame_snapshot_info_t info={0};
    if(!ot_frame_snapshot_info(&info))return BLE_ATT_ERR_UNLIKELY;
    if(frame_reply_kind==1){
        p[0]=1;p[1]=1;p[2]=(uint8_t)(info.ready?1:(info.pending?0:2));
        p[3]=info.output;p[4]=(uint8_t)info.count;p[5]=(uint8_t)(info.count>>8);
        p[6]=(uint8_t)info.sequence;p[7]=(uint8_t)(info.sequence>>8);
        p[8]=(uint8_t)(info.sequence>>16);p[9]=(uint8_t)(info.sequence>>24);
        p[10]=(uint8_t)info.crc;p[11]=(uint8_t)(info.crc>>8);
        size=12;
    }else if(frame_reply_kind==2){
        if(!ot_frame_snapshot_chunk(frame_reply_output,frame_reply_sequence,
            frame_reply_first,frame_reply_count,p+11))
            return BLE_ATT_ERR_UNLIKELY;
        p[0]=1;p[1]=2;p[2]=1;p[3]=frame_reply_output;
        p[4]=(uint8_t)frame_reply_sequence;p[5]=(uint8_t)(frame_reply_sequence>>8);
        p[6]=(uint8_t)(frame_reply_sequence>>16);p[7]=(uint8_t)(frame_reply_sequence>>24);
        p[8]=(uint8_t)frame_reply_first;p[9]=(uint8_t)(frame_reply_first>>8);
        p[10]=frame_reply_count;
        size=11u+3u*frame_reply_count;
    }else return BLE_ATT_ERR_UNLIKELY;
    /* NimBLE performs ATT long-read offset and MTU slicing after callback. */
    return os_mbuf_append(ctx->om,p,size)==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}

static void input_diag_put_le32(uint8_t *dst,uint32_t value){
    dst[0]=(uint8_t)value;dst[1]=(uint8_t)(value>>8);
    dst[2]=(uint8_t)(value>>16);dst[3]=(uint8_t)(value>>24);
}
/* Authenticated, read-only telemetry on the existing OpenTail BLE service.
 * Wire v1 is fixed 82 bytes; no public HTTP endpoint is added. */
static int input_diag_access(uint16_t conn,uint16_t handle,
                             struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    if(conn!=active_conn||!authorized)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    ot_input_diag_snapshot_t snap={0};
    ot_input_diag_snapshot_get(&snap);
    uint8_t payload[OT_INPUT_DIAG_WIRE_BYTES]={0};
    payload[0]=OT_INPUT_DIAG_WIRE_VERSION;
    payload[1]=snap.raw_asserted_mask;
    payload[2]=snap.debounced_asserted_mask;
    input_diag_put_le32(&payload[4],snap.edge_epoch);
    size_t offset=8u;
    for(unsigned i=0;i<OT_INPUT_DIAG_INPUTS;i++,offset+=4u)
        input_diag_put_le32(&payload[offset],snap.raw_edges[i]);
    for(unsigned i=0;i<OT_INPUT_DIAG_INPUTS;i++,offset+=4u)
        input_diag_put_le32(&payload[offset],snap.asserted_edges[i]);
    for(unsigned i=0;i<OT_INPUT_DIAG_INPUTS;i++,offset+=4u)
        input_diag_put_le32(&payload[offset],snap.deasserted_edges[i]);
    for(unsigned i=0;i<2u;i++)payload[offset++]=snap.turn_queue_depth[i];
    for(unsigned i=0;i<2u;i++)payload[offset++]=snap.turn_queue_high_water[i];
    for(unsigned i=0;i<2u;i++,offset+=4u)
        input_diag_put_le32(&payload[offset],snap.turn_queue_overflow_count[i]);
    for(unsigned i=0;i<2u;i++)payload[offset++]=snap.turn_queue_overflow_flag[i];
    return os_mbuf_append(ctx->om,payload,sizeof(payload))==0?
           0:BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int trigger_access(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(conn!=active_conn||!authorized)
        return ctx->op==BLE_GATT_ACCESS_OP_READ_CHR?
            BLE_ATT_ERR_READ_NOT_PERMITTED:BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    if(ctx->op==BLE_GATT_ACCESS_OP_READ_CHR){
        ot_tm_profile_t profile={0};
        uint16_t revision=0;
        uint8_t raw=0,effective=0;
        bool persistent=false;
        if(!ot_ble_trigger_get(&profile,&revision,&raw,&effective,&persistent))
            return BLE_ATT_ERR_UNLIKELY;
        uint8_t response[OT_TM_BLE_READ_BYTES]={0};
        if(!ot_tm_ble_encode(&profile,revision,raw,effective,persistent,
                             trigger_last_status,response))
            return BLE_ATT_ERR_UNLIKELY;
        return os_mbuf_append(ctx->om,response,sizeof(response))==0?
            0:BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if(ctx->op==BLE_GATT_ACCESS_OP_WRITE_CHR){
        uint8_t packet[OT_TM_BLE_WRITE_BYTES]={0};
        uint16_t n=0,revision=0;
        ot_tm_profile_t next={0};
        if(ble_hs_mbuf_to_flat(ctx->om,packet,sizeof(packet),&n)!=0||
           !ot_tm_ble_decode(packet,n,&revision,&next)){
            trigger_last_status=1;
            return 0; /* status is read back; malformed mapping never applies */
        }
        trigger_last_status=(uint8_t)ot_ble_trigger_save(&next,revision);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}
/* Palette writes are allowed only for the active, HMAC-authorized owner. */
static int palette_write(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR||conn!=active_conn||!authorized)
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t p[OT_PALETTE_WIRE_MAX];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,p,sizeof(p),&n)!=0){
        ot_palette_ble_session_reset();return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    ot_palette_ble_process(p,n,(uint32_t)(esp_timer_get_time()/1000LL));return 0;
}
static int palette_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR||conn!=active_conn||!authorized)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    uint8_t reply[OT_PALETTE_REPLY_BYTES];ot_palette_ble_reply(reply);
    return os_mbuf_append(ctx->om,reply,sizeof(reply))==0?
           0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static int palette_caps(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR||conn!=active_conn)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    const uint8_t caps[]={OT_PALETTE_WIRE_VERSION,OT_PALETTE_SLOTS,
         OT_PALETTE_STOPS_MAX,OT_PALETTE_CHUNK_MAX,200u,73u};
    return os_mbuf_append(ctx->om,caps,sizeof(caps))==0?
           0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static const struct ble_gatt_chr_def characteristics[]={
  {.uuid=&ot_info_uuid.u,.access_cb=info_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_cmd_uuid.u,.access_cb=command_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_reply_uuid.u,.access_cb=reply_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_fx_caps_uuid.u,.access_cb=fx_capabilities_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_enroll_uuid.u,.access_cb=owner_enroll_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_challenge_uuid.u,.access_cb=owner_challenge_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_cfg_cmd_uuid.u,.access_cb=cfg_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_cfg_rsp_uuid.u,.access_cb=cfg_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_trigger_uuid.u,.access_cb=trigger_access,.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_frame_cmd_uuid.u,.access_cb=frame_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_frame_rsp_uuid.u,.access_cb=frame_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_palette_cmd_uuid.u,.access_cb=palette_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_palette_rsp_uuid.u,.access_cb=palette_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_palette_caps_uuid.u,.access_cb=palette_caps,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_panel_caps_uuid.u,.access_cb=panel_caps_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_panel_cfg_cmd_uuid.u,.access_cb=panel_cfg_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_panel_cfg_rsp_uuid.u,.access_cb=panel_cfg_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_pixel_probe_uuid.u,.access_cb=pixel_probe_write,
   .flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP},
  {.uuid=&ot_input_diag_uuid.u,.access_cb=input_diag_access,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_panel_preview_control_uuid.u,.access_cb=panel_preview_control_write,
   .flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_panel_preview_update_uuid.u,.access_cb=panel_preview_update_write,
   .flags=BLE_GATT_CHR_F_WRITE_NO_RSP},
  {.uuid=&ot_panel_preview_status_uuid.u,.access_cb=panel_preview_status_read,
   .flags=BLE_GATT_CHR_F_READ},
  {0}
};
static const struct ble_gatt_svc_def services[]={
  {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&ot_service_uuid.u,.characteristics=characteristics},{0}
};
static int gap_event(struct ble_gap_event *ev,void *arg);
static void advertise(void){
    /* One active owner session at a time: never offer a second connection. */
    if(active_conn!=BLE_HS_CONN_HANDLE_NONE)return;
    struct ble_hs_adv_fields fields={0};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    const char *name=ble_svc_gap_device_name();
    fields.name=(uint8_t*)name;fields.name_len=strlen(name);fields.name_is_complete=1;
    int rc=ble_gap_adv_set_fields(&fields);
    if(rc){ESP_LOGE(TAG,"Advertising fields failed: %d",rc);return;}
    struct ble_gap_adv_params p={0};p.conn_mode=BLE_GAP_CONN_MODE_UND;p.disc_mode=BLE_GAP_DISC_MODE_GEN;
    /* Fast but bounded discovery after disconnect; units are 0.625 ms. */
    p.itvl_min=48;p.itvl_max=96; /* 30-60 ms */
    rc=ble_gap_adv_start(addr_type,NULL,BLE_HS_FOREVER,&p,gap_event,NULL);
    if(rc)ESP_LOGE(TAG,"Advertising failed: %d",rc);
    else __atomic_store_n(&ble_radio_ever_ready,true,__ATOMIC_RELEASE);
}
static int gap_event(struct ble_gap_event *ev,void *arg){
    (void)arg;
    if(ev->type==BLE_GAP_EVENT_CONNECT){
        if(ev->connect.status==0){
            if(active_conn!=BLE_HS_CONN_HANDLE_NONE){
                /* A second central must not replace the authenticated owner. */
                (void)ble_gap_terminate(ev->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
                return 0;
            }
            active_conn=ev->connect.conn_handle;authorized=false;last_result=OT_BLE_ERR_AUTH;
            challenge_ready=false;pair_failures=0;ot_st_ble_connected(true);
            cfg_reset();panel_session_reset();frame_reply_reset();ot_palette_ble_session_reset();trigger_last_status=0;
            /* Request responsive live controls; Android may choose other parameters.
             * A central rejection is never a reason to terminate this link. */
            const struct ble_gap_upd_params fast={
                .itvl_min=12,.itvl_max=24,.latency=0,.supervision_timeout=400
            }; /* 15-30 ms, 4 s supervision */
            const int rc=ble_gap_update_params(active_conn,&fast);
            if(rc)ESP_LOGW(TAG,"BLE connection interval request declined: %d",rc);
        }else advertise();
    }else if(ev->type==BLE_GAP_EVENT_DISCONNECT){
        /* A rejected secondary connection can still deliver DISCONNECT. */
        if(ev->disconnect.conn.conn_handle!=active_conn)return 0;
        active_conn=BLE_HS_CONN_HANDLE_NONE;authorized=false;last_result=OT_BLE_ERR_AUTH;
        challenge_ready=false;pair_failures=0;
        ot_st_ble_connected(false);ot_ble_disconnect_show();
        cfg_reset();panel_session_reset();frame_reply_reset();ot_palette_ble_session_reset();trigger_last_status=0;advertise();
    }else if(ev->type==BLE_GAP_EVENT_ADV_COMPLETE)advertise();
    else if(ev->type==BLE_GAP_EVENT_MTU && ev->mtu.conn_handle==active_conn)
        ESP_LOGI(TAG,"BLE negotiated ATT MTU: %u",(unsigned)ev->mtu.value);
    return 0;
}
static void on_sync(void){
    int rc=ble_hs_util_ensure_addr(0);
    if(!rc)rc=ble_hs_id_infer_auto(0,&addr_type);
    if(rc){ESP_LOGE(TAG,"BLE address error: %d",rc);return;}
    advertise();
}
/* A NimBLE host/controller reset does not promise a GAP DISCONNECT callback.
 * Clearing the old connection handle is essential: advertise() otherwise
 * refuses to restart on the next host sync, stranding a previously paired app.
 * Preserve the durable owner key, trigger state and NVS profiles. */
static void on_reset(int reason){
    const bool had_link=active_conn!=BLE_HS_CONN_HANDLE_NONE;
    active_conn=BLE_HS_CONN_HANDLE_NONE;
    authorized=false;last_result=OT_BLE_ERR_AUTH;
    challenge_ready=false;memset(owner_challenge,0,sizeof(owner_challenge));pair_failures=0;
    if(had_link){ot_st_ble_connected(false);ot_ble_disconnect_show();}
    cfg_reset();panel_session_reset();frame_reply_reset();ot_palette_ble_session_reset();trigger_last_status=0;
    ESP_LOGW(TAG,"BLE host reset (%d); advertising resumes after sync",reason);
}
static void host_task(void *arg){(void)arg;nimble_port_run();nimble_port_freertos_deinit();}
bool ot_ble_start(void){
    __atomic_store_n(&ble_radio_ever_ready,false,__ATOMIC_RELEASE);
    owner_valid=owner_load();
    gpio_config_t boot={0};boot.pin_bit_mask=(1ULL<<0);boot.mode=GPIO_MODE_INPUT;boot.pull_up_en=GPIO_PULLUP_ENABLE;
    const esp_err_t boot_err=gpio_config(&boot);
    boot_enrollment_ready=boot_err==ESP_OK;
    if(!boot_enrollment_ready){
        ESP_LOGE(TAG,"BOOT button GPIO setup failed: %s",esp_err_to_name(boot_err));
        ot_st_set_fault("BLE BOOT enrollment button unavailable");
    }
    esp_fill_random(token,sizeof(token));
    token_ready=true;
    esp_err_t err=nimble_port_init();
    if(err==ESP_OK)ble_att_set_preferred_mtu(247);
    if(err!=ESP_OK){token_ready=false;ESP_LOGE(TAG,"NimBLE init failed: %s",esp_err_to_name(err));return false;}
    ble_svc_gap_init();ble_svc_gatt_init();
    int rc=ble_svc_gap_device_name_set("OpenTail CORE");
    if(!rc)rc=ble_gatts_count_cfg(services);
    if(!rc)rc=ble_gatts_add_svcs(services);
    if(rc){token_ready=false;ESP_LOGE(TAG,"BLE service registration failed: %d",rc);return false;}
    ble_hs_cfg.reset_cb=on_reset;
    ble_hs_cfg.sync_cb=on_sync;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG,"BLE output manager started; BOOT enrollment %s; no production pairing security.",
             boot_enrollment_ready?"available":"disabled");
    return true;
}
#else
bool ot_ble_start(void){token_ready=false;return false;}
#endif
