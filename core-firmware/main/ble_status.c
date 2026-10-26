/* OpenTail CORE BLE v2 bench enrollment and existing show/output commands.
 * A physical BOOT/GPIO0 press permits reading a device-specific enrollment key.
 * Routine reconnects authenticate by HMAC of a fresh challenge; the key remains
 * in its own NVS namespace and does not ride along with routine BLE commands.
 * First enrollment is NOT authenticated against an active MITM. Bench ONLY.
 * Existing WPA2 Wi-Fi authorization stays available unchanged.
 */
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "nvs.h"
#include "mbedtls/md.h"
#include "status_led.h"
#include "ble_command.h"
#include "ble_fx_command.h"
#include "effects_1d.h"
#include "ble_config.h"
#include <stdio.h>
#include <string.h>
extern bool ot_ble_apply_show(const ot_ble_show_packet_t *request);
extern bool ot_ble_apply_fx(const ot_ble_fx_packet_t *request);
extern void ot_ble_stop_show(void);
extern bool ot_ble_config_get(uint8_t index,ot_om_output_t *out,uint16_t *revision);
extern int ot_ble_config_save(uint8_t index,const ot_om_output_t *out,uint16_t base_revision);
static uint8_t token[OT_BLE_TOKEN_BYTES];
static bool token_ready=false;
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
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
static const char *TAG="OT_BLE";
static uint8_t addr_type;
static uint16_t active_conn=BLE_HS_CONN_HANDLE_NONE;
static bool authorized=false;
static ot_ble_result_t last_result=OT_BLE_ERR_AUTH;
static uint8_t owner_key[16],owner_challenge[16];
static bool owner_valid=false,challenge_ready=false;
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
    if(conn!=active_conn||gpio_get_level(GPIO_NUM_0)!=0)return BLE_ATT_ERR_READ_NOT_PERMITTED;
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
static const ble_uuid128_t ot_reply_uuid=BLE_UUID128_INIT(0x24,0xaf,0x2e,0x10,0xb7,0xb8,0x44,0xcf,0x92,0x7a,0x15,0x49,0x22,0x60,0xa5,0x55);
static int info_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)conn;(void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    const char *message="OpenTail CORE 0.1.24 | BLE HMAC enrollment bench v2";
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
 * [version,count, little-endian effect IDs...] for current six-mode prototype.
 * A future >9-mode library needs a paginated characteristic or long-read protocol.
 */
static int fx_capabilities_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR || conn!=active_conn)
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    uint16_t ids[9]={0};
    size_t total=ot_effects_1d_supported_ids(ids,9u);
    if(total>9u)total=9u; /* Do not advertise truncated modes as fully discoverable. */
    uint8_t payload[20]={1,(uint8_t)total};
    for(size_t i=0;i<total;i++){
        payload[2+i*2]=(uint8_t)ids[i];
        payload[3+i*2]=(uint8_t)(ids[i]>>8);
    }
    return os_mbuf_append(ctx->om,payload,2u+total*2u)==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static int command_write(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR || conn!=active_conn)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    uint8_t buf[20];uint16_t n=0;
    if(ble_hs_mbuf_to_flat(ctx->om,buf,sizeof(buf),&n)!=0){last_result=OT_BLE_ERR_WIRE;return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;}
    ot_ble_show_packet_t show={0};
    if(n==18 && buf[0]==1 && buf[1]==4){
        authorized=owner_verify(buf+2);
        last_result=authorized?OT_BLE_OK_AUTH:OT_BLE_ERR_AUTH;
    } else if(n>=2 && buf[0]==1 && buf[1]==OT_BLE_FX_OPCODE) {
        ot_ble_fx_packet_t fx={0};
        ot_ble_fx_result_t result=ot_ble_fx_parse(buf,n,authorized,&fx);
        if(result==OT_BLE_FX_OK)
            last_result=ot_ble_apply_fx(&fx)?OT_BLE_OK_FX:OT_BLE_ERR_BUSY;
        else if(result==OT_BLE_FX_UNAUTHORIZED)last_result=OT_BLE_ERR_AUTH;
        else if(result==OT_BLE_FX_RANGE || result==OT_BLE_FX_UNSUPPORTED)last_result=OT_BLE_ERR_RANGE;
        else last_result=OT_BLE_ERR_WIRE;
    } else last_result=ot_ble_parse(buf,n,token,&authorized,&show);
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
static int cfg_read(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)arg;
    if(ctx->op!=BLE_GATT_ACCESS_OP_READ_CHR)return BLE_ATT_ERR_UNLIKELY;
    if(conn!=active_conn)return BLE_ATT_ERR_READ_NOT_PERMITTED;
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
static const struct ble_gatt_chr_def characteristics[]={
  {.uuid=&ot_info_uuid.u,.access_cb=info_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_cmd_uuid.u,.access_cb=command_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_reply_uuid.u,.access_cb=reply_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_fx_caps_uuid.u,.access_cb=fx_capabilities_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_enroll_uuid.u,.access_cb=owner_enroll_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_challenge_uuid.u,.access_cb=owner_challenge_read,.flags=BLE_GATT_CHR_F_READ},
  {.uuid=&ot_cfg_cmd_uuid.u,.access_cb=cfg_write,.flags=BLE_GATT_CHR_F_WRITE},
  {.uuid=&ot_cfg_rsp_uuid.u,.access_cb=cfg_read,.flags=BLE_GATT_CHR_F_READ},
  {0}
};
static const struct ble_gatt_svc_def services[]={
  {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&ot_service_uuid.u,.characteristics=characteristics},{0}
};
static int gap_event(struct ble_gap_event *ev,void *arg);
static void advertise(void){
    struct ble_hs_adv_fields fields={0};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    const char *name=ble_svc_gap_device_name();
    fields.name=(uint8_t*)name;fields.name_len=strlen(name);fields.name_is_complete=1;
    int rc=ble_gap_adv_set_fields(&fields);
    if(rc){ESP_LOGE(TAG,"Advertising fields failed: %d",rc);return;}
    struct ble_gap_adv_params p={0};p.conn_mode=BLE_GAP_CONN_MODE_UND;p.disc_mode=BLE_GAP_DISC_MODE_GEN;
    rc=ble_gap_adv_start(addr_type,NULL,BLE_HS_FOREVER,&p,gap_event,NULL);
    if(rc)ESP_LOGE(TAG,"Advertising failed: %d",rc);
}
static int gap_event(struct ble_gap_event *ev,void *arg){
    (void)arg;
    if(ev->type==BLE_GAP_EVENT_CONNECT){
        if(ev->connect.status==0){active_conn=ev->connect.conn_handle;authorized=false;last_result=OT_BLE_ERR_AUTH;challenge_ready=false;pair_failures=0;ot_st_ble_connected(true);cfg_reset();}
        else advertise();
    }else if(ev->type==BLE_GAP_EVENT_DISCONNECT){
        active_conn=BLE_HS_CONN_HANDLE_NONE;authorized=false;last_result=OT_BLE_ERR_AUTH;challenge_ready=false;pair_failures=0;
        ot_st_ble_connected(false);ot_ble_stop_show();cfg_reset();advertise();
    }else if(ev->type==BLE_GAP_EVENT_ADV_COMPLETE)advertise();
    return 0;
}
static void on_sync(void){
    int rc=ble_hs_util_ensure_addr(0);
    if(!rc)rc=ble_hs_id_infer_auto(0,&addr_type);
    if(rc){ESP_LOGE(TAG,"BLE address error: %d",rc);return;}
    advertise();
}
static void host_task(void *arg){(void)arg;nimble_port_run();nimble_port_freertos_deinit();}
void ot_ble_start(void){
    owner_valid=owner_load();
    gpio_config_t boot={0};boot.pin_bit_mask=(1ULL<<0);boot.mode=GPIO_MODE_INPUT;boot.pull_up_en=GPIO_PULLUP_ENABLE;
    gpio_config(&boot);
    esp_fill_random(token,sizeof(token));
    token_ready=true;
    esp_err_t err=nimble_port_init();
    if(err!=ESP_OK){token_ready=false;ESP_LOGE(TAG,"NimBLE init failed: %s",esp_err_to_name(err));return;}
    ble_svc_gap_init();ble_svc_gatt_init();
    int rc=ble_svc_gap_device_name_set("OpenTail CORE");
    if(!rc)rc=ble_gatts_count_cfg(services);
    if(!rc)rc=ble_gatts_add_svcs(services);
    if(rc){token_ready=false;ESP_LOGE(TAG,"BLE service registration failed: %d",rc);return;}
    ble_hs_cfg.sync_cb=on_sync;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG,"BLE output manager and physical BOOT enrollment available; no production pairing security.");
}
#else
void ot_ble_start(void){token_ready=false;}
#endif
