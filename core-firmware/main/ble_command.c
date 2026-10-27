#include "ble_command.h"
static bool token_equal(const uint8_t *a,const uint8_t *b){
    uint8_t diff=0;
    for(unsigned i=0;i<OT_BLE_TOKEN_BYTES;i++)diff|=(uint8_t)(a[i]^b[i]);
    return diff==0;
}
ot_ble_result_t ot_ble_parse(const uint8_t *p,size_t n,const uint8_t secret[OT_BLE_TOKEN_BYTES],
                               bool *authenticated,ot_ble_show_packet_t *show){
    if(!p||!secret||!authenticated||!show||n<2||p[0]!=OT_BLE_WIRE_V1)return OT_BLE_ERR_WIRE;
    if(p[1]==OT_BLE_AUTH){
        if(n!=18)return OT_BLE_ERR_WIRE;
        *authenticated=token_equal(p+2,secret);
        return *authenticated?OT_BLE_OK_AUTH:OT_BLE_ERR_AUTH;
    }
    if(!*authenticated)return OT_BLE_ERR_AUTH;
    if(p[1]==OT_BLE_STOP)return n==2?OT_BLE_OK_STOP:OT_BLE_ERR_WIRE;
    if(p[1]!=OT_BLE_SHOW||n!=OT_BLE_SHOW_BYTES)return OT_BLE_ERR_WIRE;
    if(p[2]<1||p[2]>5||p[3]<1||p[3]>3||p[4]<1||p[8]>32||p[9]>100||p[10]<1||p[10]>60)
        return OT_BLE_ERR_RANGE;
    *show=(ot_ble_show_packet_t){p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10]};
    return OT_BLE_OK_SHOW;
}
const char *ot_ble_result_string(ot_ble_result_t v){
    switch(v){
        case OT_BLE_OK_AUTH:return "OK AUTH";case OT_BLE_OK_SHOW:return "OK SHOW";
        case OT_BLE_OK_STOP:return "OK STOP";case OT_BLE_OK_FX:return "OK FX";
        case OT_BLE_OK_SAVED:return "OK SAVED";case OT_BLE_OK_LOADED:return "OK LOADED";
        case OT_BLE_OK_DELETED:return "OK DELETED";case OT_BLE_ERR_AUTH:return "ERR AUTH";
        case OT_BLE_ERR_WIRE:return "ERR FORMAT";case OT_BLE_ERR_RANGE:return "ERR RANGE";
        default:return "ERR BUSY";
    }
}
