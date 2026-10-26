#include <assert.h>
#include <string.h>
#include "ble_command.h"
int main(void){
 uint8_t key[16];memset(key,0xa7,16);bool auth=false;ot_ble_show_packet_t v={0};
 uint8_t show[]={1,2,1,1,1,255,0,0,6,50,15};
 assert(ot_ble_parse(show,sizeof(show),key,&auth,&v)==OT_BLE_ERR_AUTH);
 uint8_t token[]={1,1,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7,0xa7};
 assert(ot_ble_parse(token,18,key,&auth,&v)==OT_BLE_OK_AUTH && auth);
 assert(ot_ble_parse(show,sizeof(show),key,&auth,&v)==OT_BLE_OK_SHOW);
 assert(v.effect==1&&v.chain_mask==1&&v.brightness==6&&v.ttl_sec==15);
 show[8]=33; assert(ot_ble_parse(show,sizeof(show),key,&auth,&v)==OT_BLE_ERR_RANGE);
 show[8]=6;show[10]=61;assert(ot_ble_parse(show,sizeof(show),key,&auth,&v)==OT_BLE_ERR_RANGE);
 show[10]=15;show[4]=0;assert(ot_ble_parse(show,sizeof(show),key,&auth,&v)==OT_BLE_ERR_RANGE);
 uint8_t stop[]={1,3};assert(ot_ble_parse(stop,sizeof(stop),key,&auth,&v)==OT_BLE_OK_STOP);
 auth=false;assert(ot_ble_parse(stop,sizeof(stop),key,&auth,&v)==OT_BLE_ERR_AUTH);
 token[2]=0x55;assert(ot_ble_parse(token,18,key,&auth,&v)==OT_BLE_ERR_AUTH&&!auth);
 return 0;
}
