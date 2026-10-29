#include <assert.h>
#include <string.h>
#include "ble_fx_command.h"
int main(void){
  ot_ble_fx_packet_t v={0};
  /* Wipe id=3, chain 3, zone 1, fg red, bg black, brightness 16, speed 80,
   * intensity 90, direct palette, TTL 15s */
  uint8_t cmd[OT_BLE_FX_WIRE_BYTES]={1,5,3,0,3,1,255,0,0,0,0,0,16,80,90,0,15};
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),false,&v)==OT_BLE_FX_UNAUTHORIZED);
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  assert(v.effect_id==3&&v.chain_mask==3&&v.zone_mask==1&&v.ttl_sec==15);
  assert(ot_ble_fx_parse(cmd,16,true,&v)==OT_BLE_FX_BAD_WIRE);
  cmd[2]=186;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_UNSUPPORTED);
  cmd[2]=3;cmd[5]=0;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[5]=1;cmd[15]=1;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[15]=0;cmd[12]=33;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[12]=32;cmd[16]=61;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[16]=15;cmd[2]=23;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  cmd[2]=0;cmd[3]=128;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  assert(v.effect_id==32768u);
  cmd[15]=4;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  cmd[2]=3;cmd[3]=0;assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);

  /* BLE packet structure remains unchanged; valid palettes now reach
   * only the two source-verified fixed-palette effect renderers. */
  cmd[2]=218;cmd[3]=0;cmd[15]=11;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  assert(v.effect_id==218u&&v.palette_id==11u);
  cmd[2]=179;cmd[15]=12;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  cmd[2]=115;cmd[15]=72;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  cmd[2]=87;cmd[15]=11;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  cmd[15]=73;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[2]=3;cmd[15]=11;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  return 0;
}
