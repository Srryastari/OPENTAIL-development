#include <assert.h>
#include <string.h>
#include "ble_fx_command.h"
#include "board_config.h"
static void make_tertiary(uint8_t cmd[OT_BLE_FX_TERTIARY_WIRE_BYTES],
                          uint16_t id,uint8_t palette){
  memset(cmd,0,OT_BLE_FX_TERTIARY_WIRE_BYTES);
  cmd[0]=1u;cmd[1]=OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE;
  cmd[2]=(uint8_t)id;cmd[3]=(uint8_t)(id>>8);
  cmd[4]=1u;cmd[5]=1u;
  cmd[6]=255u;cmd[7]=0u;cmd[8]=0u;
  cmd[9]=0u;cmd[10]=0u;cmd[11]=0u;
  cmd[12]=24u;cmd[13]=128u;cmd[14]=128u;
  cmd[15]=palette;cmd[16]=1u;
  /* Explicit black: presence is byte 16, not RGB non-zero. */
  cmd[17]=0u;cmd[18]=0u;cmd[19]=0u;
}
static void test_tertiary_admission(void){
  static const uint16_t supported[]={28u,29u,34u,38u,50u,52u,54u,
      55u,56u,84u,87u,95u,103u};
  static const uint16_t unsupported[]={10u,11u,40u,48u,60u,91u};
  uint8_t cmd[OT_BLE_FX_TERTIARY_WIRE_BYTES];
  for(size_t i=0;i<sizeof(supported)/sizeof(supported[0]);i++){
    const uint16_t id=supported[i];
    make_tertiary(cmd,id,id==38u?4u:0u);
    ot_ble_fx_packet_t parsed={0};
    ot_ble_fx_tertiary_t third={0};
    assert(ot_ble_fx_parse_tertiary(cmd,sizeof(cmd),true,&parsed,&third)==OT_BLE_FX_OK);
    assert(parsed.effect_id==id);
    assert(third.set && third.r==0u && third.g==0u && third.b==0u);
  }
  for(size_t i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++){
    make_tertiary(cmd,unsupported[i],0u);
    ot_ble_fx_packet_t parsed,before;
    ot_ble_fx_tertiary_t third,third_before;
    memset(&parsed,0xA5,sizeof(parsed));before=parsed;
    memset(&third,0x5A,sizeof(third));third_before=third;
    assert(ot_ble_fx_parse_tertiary(cmd,sizeof(cmd),true,&parsed,&third)==OT_BLE_FX_UNSUPPORTED);
    assert(memcmp(&parsed,&before,sizeof(parsed))==0);
    assert(memcmp(&third,&third_before,sizeof(third))==0);
  }
  /* Aurora #38 is source-proven only on the BLE-admitted palette 4.
   * Palette 5/default palettes and all six unsupported roles fail closed. */
  make_tertiary(cmd,38u,5u);
  ot_ble_fx_packet_t parsed={0};ot_ble_fx_tertiary_t third={0};
  assert(ot_ble_fx_parse_tertiary(cmd,sizeof(cmd),true,&parsed,&third)==OT_BLE_FX_UNSUPPORTED);
  make_tertiary(cmd,38u,4u);
  assert(ot_ble_fx_parse_tertiary(cmd,sizeof(cmd),false,&parsed,&third)==OT_BLE_FX_UNAUTHORIZED);
  cmd[16]=0u;
  assert(ot_ble_fx_parse_tertiary(cmd,sizeof(cmd),true,&parsed,&third)==OT_BLE_FX_BAD_WIRE);
}

int main(void){
  test_tertiary_admission();
  ot_ble_fx_packet_t v={0};
  /* Wipe id=3, chain 3, zone 1, fg red, bg black, brightness 16, speed 80,
   * intensity 90, direct palette, TTL 15s */
  uint8_t cmd[OT_BLE_FX_WIRE_BYTES]={1,5,3,0,3,1,255,0,0,0,0,0,16,80,90,0,15};
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),false,&v)==OT_BLE_FX_UNAUTHORIZED);
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  assert(v.effect_id==3&&v.chain_mask==3&&v.zone_mask==1&&v.ttl_sec==15);
  cmd[4]=(uint8_t)(1u<<OT_OM_OUTPUTS);
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[4]=3;
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
  /* Continuous lifetime is explicit; legacy TTL zero remains invalid. */
  cmd[2]=0;cmd[3]=0;cmd[15]=0;cmd[16]=0;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  cmd[1]=OT_BLE_FX_CONTINUOUS_OPCODE;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),false,&v)==OT_BLE_FX_UNAUTHORIZED);
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_OK);
  assert(v.ttl_sec==1u && v.effect_id==0);
  cmd[16]=5;
  assert(ot_ble_fx_parse(cmd,sizeof(cmd),true,&v)==OT_BLE_FX_RANGE);
  return 0;
}
